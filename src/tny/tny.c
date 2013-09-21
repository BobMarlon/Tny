#include "tny.h"
#include <stdlib.h>
#include <string.h>

#define HASNEXTDATA(X) if ((*pos) + X > length) break

static size_t _Tny_dumps(const Tny *tny, char *data, size_t pos);
static Tny* _Tny_loads(char *data, size_t length, size_t *pos);
static uint32_t* Tny_swapBytes32(uint32_t *dest, const uint32_t *src);
static uint64_t* Tny_swapBytes64(uint64_t *dest, const uint64_t *src);
static void TnyElement_freeValue(Tny *tny);

union tnyHostOrder tnyHostOrder = { { 0, 1, 2, 3 } };

Tny* Tny_add(Tny *prev, TnyType type, char *key, void *value, uint64_t size)
{
	Tny *tny = NULL;
	enum {CHECK_PRECONDITIONS, ALLOCATE, CHAIN, SET_KEY, SET_VALUE, FAILED};
	int status = CHECK_PRECONDITIONS;
	int loop = 1;

	while (loop) {
		switch (status) {
		case CHECK_PRECONDITIONS:
			if ((prev != NULL && type != TNY_ARRAY && type != TNY_DICT) ||
				(prev == NULL && (type == TNY_ARRAY || type == TNY_DICT))) {

				status = ALLOCATE;
				if (prev != NULL && prev->root->type == TNY_DICT && key == NULL) {
					// Dict must have a key!
					status = FAILED;
				} else if (key != NULL && prev != NULL) {
					tny = Tny_get(prev, key);
					if (tny != NULL) {
						TnyElement_freeValue(tny);
						status = CHAIN;
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
			if (prev != NULL) {
				tny->prev = prev;
				tny->next = prev->next;
				prev->next = tny;
				if (tny->next != NULL) {
					tny->next->prev = tny;
				}

				tny->root = prev->root;
				tny->root->size++;
			} else {
				tny->root = tny;
			}

			if (key != NULL && tny->key == NULL) {
				status = SET_KEY;
			} else {
				status = SET_VALUE;
			}
			break;
		case SET_KEY:
			tny->key = malloc(strlen(key) + 1);

			if (tny->key != NULL) {
				memcpy(tny->key, key, strlen(key) + 1);
				status = SET_VALUE;
			} else {
				status = FAILED;
			}
			break;
		case SET_VALUE:
			tny->type = type;

			if (type != TNY_ARRAY && type != TNY_DICT) {
				// Set size
				tny->size = size;
				// Set value
				if (tny->type == TNY_OBJ) {
					tny->value.ptr = ((Tny*)value)->root;
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
			// SUCCESS
			loop = 0;
			break;
		case FAILED:
			if (tny != NULL) {
				free(tny->key);
				free(tny);
				tny = NULL;
			}
			loop = 0;
			break;
		}
	}

	return tny;
}

void Tny_remove(Tny *tny)
{
	if (tny != NULL) {
		if (tny->root == tny) {
			TnyElement_free(tny);
		} else {
			tny->root->size--;
			tny->prev->next = tny->next;
			tny->next->prev = tny->prev;
			TnyElement_freeValue(tny);
			free(tny->key);
			free(tny);
		}
	}
}

Tny* Tny_at(const Tny* tny, size_t index)
{
	Tny *result = NULL;
	size_t count = 0;

	for (Tny *next = tny->root; next != NULL; next = next->next) {
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
	Tny *result = NULL;
	size_t len = 0;

	if (key != NULL) {
		len = strlen(key);
		for (Tny *next = tny->root; next != NULL; next = next->next) {
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

size_t Tny_calcSize(const Tny *tny)
{
	size_t size = 0;
	size_t tmp = 0;
	uint64_t counter = 0;

	for (const Tny *next = tny; next != NULL; next = next->next) {
		if (next == tny) {
			if (next->type != TNY_ARRAY && next->type != TNY_DICT) {
				size = 0;
				break;
			} else {
				size += 1 + sizeof(uint32_t); // Object type + tny->size number of elements
				continue;
			}
		}

		size += 1; // Value type

		if (tny->root->type == TNY_DICT) {
			size += sizeof(uint32_t); // field where the key length is stored.
			size += strlen(next->key) + 1; // length of the key + 1
		}

		if (next->type == TNY_OBJ) {
			tmp = Tny_calcSize(next->value.tny);
			if (tmp > 0) {
				size += tmp;
			} else {
				size = 0;
				break;
			}
		} else if (next->type == TNY_NULL) {
			size += 0; // No data necessary
		} else if (next->type == TNY_BIN) {
			size += sizeof(uint32_t) + next->size; // tny->size size + value size;
		} else if (next->type == TNY_CHAR) {
			size += 1;
		} else if (next->type == TNY_INT32) {
			size += sizeof(uint32_t);
		} else if (next->type == TNY_INT64) {
			size += sizeof(uint64_t);
		} else if (next->type == TNY_DOUBLE) {
			size += sizeof(double);
		} else {
			size = 0;
			break;
		}

		counter++;
	}

	if (tny->root->size != counter) {
		size = 0;
	}

	return size;
}

size_t _Tny_dumps(const Tny *tny, char *data, size_t pos)
{
	uint32_t size = 0;

	for (const Tny *next = tny; next != NULL; next = next->next) {
		// Add the data type
		data[pos++] = next->type;

		// Add the number of elements if this is the root element.
		if (next->type == TNY_ARRAY || next->type == TNY_DICT) {
			Tny_swapBytes32((uint32_t*)(data + pos), &next->size);
			pos += sizeof(uint32_t);
			continue;
		}

		// Add the key if this is a dictionary
		if (next->root->type == TNY_DICT) {
			size = strlen(next->key) + 1;
			Tny_swapBytes32((uint32_t*)(data + pos), &size);
			pos += sizeof(uint32_t);
			memcpy((data + pos), next->key, size);
			pos += size;
		}

		// Add the value
		if (next->type == TNY_OBJ) {
			pos = _Tny_dumps(next->value.tny, data, pos);
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
	size = Tny_calcSize(tny);
	if (size > 0) {
		*data = malloc(size);
		if (*data != NULL) {
			_Tny_dumps(tny, *data, 0);
		} else {
			size = 0;
		}
	}

	return size;
}

Tny* _Tny_loads(char *data, size_t length, size_t *pos)
{
	Tny *tny = NULL;
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
			tny = Tny_add(tny, type, key, _Tny_loads(data, length, pos), 0);
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

	if (Tny_calcSize(tny->root) == 0) {
		TnyElement_free(tny);
		tny = NULL;
	} else {
		tny = tny->root;
	}

	return tny;
}

Tny* Tny_loads(void *data, size_t length)
{
	size_t pos = 0;

	return _Tny_loads(data, length, &pos);
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

void TnyElement_freeValue(Tny *tny)
{
	if (tny != NULL) {
		if (tny->type == TNY_BIN) {
			free(tny->value.ptr);
		} else if (tny->type == TNY_OBJ) {
			TnyElement_free(tny->value.tny);
		}
		tny->value.ptr = NULL;
	}
}

void TnyElement_free(Tny *tny)
{
	Tny *tmp = NULL;
	Tny *next = tny->root;

	while (next != NULL) {
		tmp = next->next;
		TnyElement_freeValue(next);
		free(next->key);
		free(next);
		next = tmp;
	}
}
