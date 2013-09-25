#include "tny.h"
#include <stdlib.h>
#include <string.h>

#define HASNEXTDATA(X) if ((*pos) + X > length) break

static void Tny_addSize(Tny *tny, size_t size);
static void Tny_subSize(Tny *tny, size_t size);
static size_t Tny_valueSize(TnyType type, size_t size);
static size_t _Tny_dumps(const Tny *tny, char *data, size_t pos);
static Tny* _Tny_loads(char *data, size_t length, size_t *pos, size_t *docSizePtr);
static uint32_t* Tny_swapBytes32(uint32_t *dest, const uint32_t *src);
static uint64_t* Tny_swapBytes64(uint64_t *dest, const uint64_t *src);
static void Tny_freeValue(Tny *tny);

union tnyHostOrder tnyHostOrder = { { 0, 1, 2, 3 } };

Tny* Tny_add(Tny *prev, TnyType type, char *key, void *value, uint64_t size)
{
	Tny *tny = NULL;
	enum {CHECK_PRECONDITIONS, ALLOCATE, CHAIN, SET_KEY, SET_VALUE, FAILED};
	int status = CHECK_PRECONDITIONS;
	int loop = 1;
	int isoverwrite = 0;
	size_t keyLen = 0;

	while (loop) {
		switch (status) {
		case CHECK_PRECONDITIONS:
			if ((prev != NULL && type != TNY_ARRAY && type != TNY_DICT) ||
				(prev == NULL && (type == TNY_ARRAY || type == TNY_DICT))) {

				status = ALLOCATE;
				if (prev != NULL && prev->root->type == TNY_DICT && key == NULL) {
					/* Dict must have a key! */
					status = FAILED;
				} else if (key != NULL && prev != NULL && prev->root->type == TNY_DICT) {
					tny = Tny_get(prev, key);
					if (tny != NULL) {
						Tny_freeValue(tny);
						status = SET_VALUE;
						isoverwrite = 1;
					}
				}
			} else {
				status = FAILED;
			}
			break;
		case ALLOCATE:
			tny = malloc(sizeof(Tny));

			if (tny != NULL) {
				memset(tny, 0, sizeof(Tny));
				status = CHAIN;
			} else {
				status = FAILED;
			}
			break;
		case CHAIN:
			if (prev != tny) {
				if (prev != NULL) {
					tny->prev = prev;
					tny->next = prev->next;
					prev->next = tny;
					if (tny->next != NULL) {
						tny->next->prev = tny;
					}

					tny->root = prev->root;
					tny->root->size++;
					tny->docSizePtr  = tny->root->docSizePtr;
				} else {
					tny->root = tny;
					tny->docSizePtr = &tny->docSize;
				}
			}

			if (key != NULL && tny->key == NULL && prev->root->type == TNY_DICT) {
				status = SET_KEY;
			} else {
				status = SET_VALUE;
			}
			break;
		case SET_KEY:
			keyLen = strlen(key) + 1;
			tny->key = malloc(keyLen);

			if (tny->key != NULL) {
				memcpy(tny->key, key, keyLen);
				Tny_addSize(tny, sizeof(uint32_t) + keyLen);
				status = SET_VALUE;
			} else {
				status = FAILED;
			}
			break;
		case SET_VALUE:
			tny->type = type;

			if (type != TNY_ARRAY && type != TNY_DICT) {
				/* Set size */
				tny->size = size;
				/* Set value */
				if (tny->type == TNY_OBJ) {
					if (value != NULL) {
						tny->value.tny = Tny_copy(tny->root->docSizePtr, value);
						if (tny->value.tny == NULL) {
							status = FAILED;
							break;
						}
					}
				} else if (tny->type == TNY_BIN) {
					tny->value.ptr = malloc(size);

					if (tny->value.ptr != NULL) {
						memcpy(tny->value.ptr, value, size);
					} else {
						status = FAILED;
						break;
					}
				} else if (tny->type == TNY_CHAR) {
					tny->value.chr = *((char*)value);
				} else if (tny->type == TNY_INT32) {
					tny->value.num = *((uint32_t*)value);
				} else if (tny->type == TNY_INT64) {
					tny->value.num = *((uint64_t*)value);
				} else if (tny->type == TNY_DOUBLE) {
					tny->value.flt = *((double*)value);
				}
			}
			Tny_addSize(tny, Tny_valueSize(tny->type, tny->size));

			/* SUCCESS */
			loop = 0;
			break;
		case FAILED:
			if (tny != NULL && !isoverwrite) {
				free(tny->key);
				free(tny);
			}
			tny = NULL;
			loop = 0;
			isoverwrite = 0;
			break;
		}
	}

	return isoverwrite ? prev : tny;
}

Tny* Tny_copy(size_t *docSizePtr, const Tny *src)
{
	Tny *dest = NULL;
	Tny *newObj = NULL;
	Tny *next = NULL;

	for (next = src->root; next != NULL; next = next->next) {
		if (next->type == TNY_BIN || next->type == TNY_OBJ) {
			newObj = Tny_add(dest, next->type, next->key, next->value.ptr, next->size);
		} else {
			newObj = Tny_add(dest, next->type, next->key, &next->value.num, next->size);
		}

		if (newObj != NULL) {
			if (dest == NULL) {
				newObj->docSizePtr = docSizePtr;
				*docSizePtr += newObj->docSize;
			}
			dest = newObj;
		} else {
			Tny_free(dest);
			return NULL;
		}
	}

	return dest->root;
}

void Tny_addSize(Tny *tny, size_t size)
{
	*tny->docSizePtr += size;
	if (tny->docSizePtr != &tny->docSize) {
		tny->docSize += size;
	}

	if (tny->docSizePtr != &tny->root->docSize) {
		tny->root->docSize += size;
	}
}

void Tny_subSize(Tny *tny, size_t size)
{
	*tny->docSizePtr -= size;
	if (tny->docSizePtr != &tny->docSize) {
		tny->docSize -= size;
	}

	if (tny->docSizePtr != &tny->root->docSize) {
		tny->root->docSize -= size;
	}
}

void Tny_remove(Tny *tny)
{
	if (tny != NULL) {
		if (tny->root == tny) {
			Tny_free(tny);
		} else {
			tny->root->size--;
			if (tny->root->type == TNY_DICT) {
				Tny_subSize(tny, sizeof(uint32_t) + strlen(tny->key) + 1);
			}

			tny->prev->next = tny->next;
			if (tny->next != NULL) {
				tny->next->prev = tny->prev;
			}
			Tny_freeValue(tny);
			free(tny->key);
			free(tny);
		}
	}
}

Tny* Tny_at(const Tny* tny, size_t index)
{
	Tny *next = NULL;
	Tny *result = NULL;
	size_t count = 0;

	for (next = tny->root; next != NULL; next = next->next) {
		if (next == tny->root) {
			continue;
		}

		if (count == index) {
			result = next;
			break;
		}
		count++;
	}

	return result;
}

Tny* Tny_get(const Tny* tny, const char *key)
{
	Tny *next = NULL;
	Tny *result = NULL;
	size_t len = 0;

	if (key != NULL) {
		len = strlen(key);
		for (next = tny->root; next != NULL; next = next->next) {
			if (next == tny->root) {
				continue;
			}

			if (next->key != NULL && len == strlen(next->key) && memcmp(next->key, key, len) == 0) {
				result = next;
				break;
			}
		}
	}

	return result;
}

size_t Tny_valueSize(TnyType type, size_t size)
{
	size_t result = 1; /* Because of value type field. */

	if (type == TNY_ARRAY || type == TNY_DICT) {
		result += sizeof(uint32_t); /* Number of elements field. */
	} else if (type == TNY_OBJ) {
		result += 0; /* only the type field is needed. */
	} else if (type == TNY_NULL) {
		result += 0; /* No data necessary */
	} else if (type == TNY_BIN) {
		result += sizeof(uint32_t) + size; /* tny->size size + value size */
	} else if (type == TNY_CHAR) {
		result += 1;
	} else if (type == TNY_INT32) {
		result += sizeof(uint32_t);
	} else if (type == TNY_INT64) {
		result += sizeof(uint64_t);
	} else if (type == TNY_DOUBLE) {
		result += sizeof(double);
	}

	return result;
}

size_t _Tny_dumps(const Tny *tny, char *data, size_t pos)
{
	const Tny *next = NULL;
	uint32_t size = 0;

	for (next = tny; next != NULL; next = next->next) {
		/* Add the data type */
		data[pos++] = next->type;

		/* Add the number of elements if this is the root element. */
		if (next->type == TNY_ARRAY || next->type == TNY_DICT) {
			Tny_swapBytes32((uint32_t*)(data + pos), &next->size);
			pos += sizeof(uint32_t);
			continue;
		}

		/* Add the key if this is a dictionary */
		if (next->root->type == TNY_DICT) {
			size = strlen(next->key) + 1;
			Tny_swapBytes32((uint32_t*)(data + pos), &size);
			pos += sizeof(uint32_t);
			memcpy((data + pos), next->key, size);
			pos += size;
		}

		/* Add the value */
		if (next->type == TNY_OBJ) {
			if (next->value.tny != NULL) {
				pos = _Tny_dumps(next->value.tny, data, pos);
			} else {
				pos = 0;
				break;
			}
		} else if (next->type == TNY_BIN) {
			Tny_swapBytes32((uint32_t*)(data + pos), &next->size);
			pos += sizeof(uint32_t);
			memcpy((data + pos), next->value.ptr, next->size);
			pos += next->size;
		} else if (next->type == TNY_CHAR) {
			data[pos++] = next->value.chr;
		} else if (next->type == TNY_INT32) {
			Tny_swapBytes32((uint32_t*)(data + pos), (uint32_t*)&next->value.num);
			pos += sizeof(uint32_t);
		} else if (next->type == TNY_INT64) {
			Tny_swapBytes64((uint64_t*)(data + pos), &next->value.num);
			pos += sizeof(uint64_t);
		} else if (next->type == TNY_DOUBLE) {
			Tny_swapBytes64((uint64_t*)(data + pos), &next->value.num);
			pos += sizeof(double);
		}
	}

	return pos;
}

size_t Tny_dumps(const Tny *tny, void **data)
{
	size_t size = 0;

	*data = NULL;
	tny = tny->root;
	size = tny->docSize;
	*data = malloc(size);
	if (*data != NULL) {
		size = _Tny_dumps(tny, *data, 0);
		if (size == 0) {
			free(*data);
			*data = NULL;
		}
	} else {
		size = 0;
	}

	return size;
}

Tny* _Tny_loads(char *data, size_t length, size_t *pos, size_t *docSizePtr)
{
	Tny *tny = NULL;
	Tny *newObj = NULL;
	TnyType type = TNY_NULL;
	uint32_t size = 0;
	uint32_t i32 = 0;
	uint64_t i64 = 0;
	double flt = 0.0f;
	char *key = NULL;
	uint64_t counter = 0;
	uint64_t elements = 0;

	while ((*pos) < length) {
		type = data[(*pos)++];
		if (tny == NULL) {
			if (type == TNY_ARRAY || type == TNY_DICT) {
				HASNEXTDATA(sizeof(uint32_t));
				Tny_swapBytes32(&size, (uint32_t*)(data + (*pos)));
				*pos += sizeof(uint32_t);
				elements = size;
				tny = Tny_add(NULL, type, NULL, NULL, size);
				if (tny != NULL) {
					if (docSizePtr != NULL) {
						tny->docSizePtr = docSizePtr;
						*tny->docSizePtr += tny->docSize;
					}
				} else {
					break;
				}
				continue;
			} else {
				break;
			}
		}

		if (tny->root->type == TNY_DICT) {
			HASNEXTDATA(sizeof(uint32_t));
			Tny_swapBytes32(&size, (uint32_t*)(data + (*pos)));
			*pos += sizeof(uint32_t);
			HASNEXTDATA(size);
			if (data[(*pos) + size - 1] == '\0') {
				key = data + (*pos);
				*pos += size;
			} else {
				break;
			}
		} else {
			key = NULL;
		}

		if (type == TNY_NULL) {
			tny = Tny_add(tny, type, key, NULL, 0);
	 	} else if (type == TNY_OBJ) {
	 		newObj = _Tny_loads(data, length, pos, tny->root->docSizePtr);
	 		if (newObj != NULL) {
				tny = Tny_add(tny, type, key, NULL, 0);
				if (tny != NULL) {
					tny->value.tny = newObj->root;
				} else {
					break;
				}
	 		} else {
	 			break;
	 		}
		} else if (type == TNY_BIN) {
			HASNEXTDATA(sizeof(uint32_t));
			Tny_swapBytes32(&size, (uint32_t*)(data + (*pos)));
			*pos += sizeof(uint32_t);
			HASNEXTDATA(size);
			tny = Tny_add(tny, type, key, (data + *pos), size);
			*pos += size;
		} else if (type == TNY_CHAR) {
			HASNEXTDATA(1);
			tny = Tny_add(tny, type, key, (data + *pos), 0);
			(*pos)++;
		} else if (type == TNY_INT32) {
			HASNEXTDATA(sizeof(uint32_t));
			Tny_swapBytes32(&i32, (uint32_t*)(data + (*pos)));
			*pos += sizeof(uint32_t);
			tny = Tny_add(tny, type, key, &i32, 0);
		} else if (type == TNY_INT64) {
			HASNEXTDATA(sizeof(uint64_t));
			Tny_swapBytes64(&i64, (uint64_t*)(data + (*pos)));
			*pos += sizeof(uint64_t);
			tny = Tny_add(tny, type, key, &i64, 0);
		} else if (type == TNY_DOUBLE) {
			HASNEXTDATA(sizeof(double));
			Tny_swapBytes64((uint64_t*)&flt, (uint64_t*)(data + (*pos)));
			*pos += sizeof(double);
			tny = Tny_add(tny, type, key, &flt, 0);
		}

		counter++;
		if (counter >= elements) {
			break;
		}
	}

	return tny;
}

Tny* Tny_loads(void *data, size_t length)
{
	size_t pos = 0;

	return _Tny_loads(data, length, &pos, NULL);
}

static uint32_t* Tny_swapBytes32(uint32_t *dest, const uint32_t *src)
{
	union {
		uint32_t num;
		char c[4];
	} bytes;
	char c;

	if (ORDER_LITTLE_ENDIAN != HOST_ORDER) {
		bytes.num = *src;
		c = bytes.c[0]; bytes.c[0] = bytes.c[3]; bytes.c[3] = c;
		c = bytes.c[1]; bytes.c[1] = bytes.c[2]; bytes.c[2] = c;
		memcpy(dest, &bytes.num, sizeof(uint32_t));
	} else {
		memcpy(dest, src, sizeof(uint32_t));
	}

	return dest;
}

static uint64_t* Tny_swapBytes64(uint64_t *dest, const uint64_t *src)
{
	union {
		uint64_t num;
		char c[8];
	} bytes;
	char c;

	if (ORDER_LITTLE_ENDIAN != HOST_ORDER) {
		bytes.num = *src;
		c = bytes.c[0]; bytes.c[0] = bytes.c[7]; bytes.c[7] = c;
		c = bytes.c[1]; bytes.c[1] = bytes.c[6]; bytes.c[6] = c;
		c = bytes.c[2]; bytes.c[2] = bytes.c[5]; bytes.c[5] = c;
		c = bytes.c[3]; bytes.c[3] = bytes.c[4]; bytes.c[4] = c;
		memcpy(dest, &bytes.num, sizeof(uint64_t));
	} else {
		memcpy(dest, src, sizeof(uint64_t));
	}

	return dest;
}

int Tny_hasNext(const Tny *tny)
{
	return tny->next != NULL;
}

Tny* Tny_next(const Tny *tny)
{
	return tny->next;
}

void Tny_freeValue(Tny *tny)
{
	if (tny != NULL) {
		Tny_subSize(tny, Tny_valueSize(tny->type, tny->size));
		if (tny->type == TNY_BIN) {
			free(tny->value.ptr);
		} else if (tny->type == TNY_OBJ) {
			Tny_free(tny->value.tny);
		}
		tny->value.ptr = NULL;
	}
}

void Tny_free(Tny *tny)
{
	Tny *tmp = NULL;
	Tny *next = NULL;
	TnyType type = TNY_NULL;

	if (tny != NULL) {
		type = tny->root->type;

		/* Get the last element.
		   The linked list has to be free'd from back to front because of tny->docSizePtr
		   which points to the root element. */
		for (next = tny; next != NULL; next = next->next) {
			tny = next;
		}

		next = tny;
		while (next != NULL) {
			tmp = next->prev;
			Tny_freeValue(next);
			if (type == TNY_DICT && next->key != NULL) {
				Tny_subSize(next, sizeof(uint32_t) + strlen(next->key) + 1);
			}
			free(next->key);
			free(next);
			next = tmp;
		}
	}
}
