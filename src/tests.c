#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "tny/tny.h"

void printObj(Tny *tny, int level);

int printElement(Tny *tny, int level)
{
	if (tny->type == TNY_NULL) {
		printf("NULL\n");
	} else if (tny->type == TNY_OBJ) {
		printf("\n");
		printObj(tny->value.tny, level + 1);
	} else if (tny->type == TNY_BIN) {
		printf("%s\n", (char*)tny->value.ptr);
	} else if (tny->type == TNY_INT32) {
		printf("%"PRIu32"\n", (uint32_t)tny->value.num);
	} else if (tny->type == TNY_INT64) {
		printf("%"PRIu64"\n", tny->value.num);
	} else if (tny->type == TNY_CHAR) {
		printf("%c\n", tny->value.chr);
	} else if (tny->type == TNY_DOUBLE) {
		printf("%g\n", tny->value.flt);
	} else {
		return 1;
	}

	return 0;
}

void printObj(Tny *tny, int level)
{
	char spaces[level + 1];
	Tny *next = NULL;
	TnyType type = TNY_NULL;
	int count = 0;
	int i;

	if (level > 0) {
		for (i = 0; i < level; i++) {
			spaces[i] = '\t';
		}
		spaces[level + 1] = '\0';
	} else {
		spaces[0] = '\0';
	}

	for (next = tny->root; next != NULL; next = next->next) {
		if (next == tny->root) {
			type = next->type;
			continue;
		}

		if (type == TNY_ARRAY) {
			printf("%s[%d]: ", spaces, count);
			if (printElement(next, level)) {
				printf("\nCorrupted Tny-Object!\n");
				return;
			}
			count++;
		} else if (type == TNY_DICT) {
			printf("%s%s: ", spaces, next->key);
			if (printElement(next, level)) {
				printf("\nCorrupted Tny-Object!\n");
				return;
			}
		}
	}
}

int Tny_cmp(Tny *left, Tny *right)
{
	Tny *lnext = left->root;
	Tny *rnext = right->root;
	TnyType type = TNY_NULL;

	if (lnext->type == rnext->type) {
		type = lnext->type;

		lnext = lnext->next;
		rnext = rnext->next;
		while (lnext != NULL && rnext != NULL) {
			if (lnext->type != rnext->type) {
				return 1;
			}

			if (type == TNY_DICT) {
				if (strcmp(lnext->key, rnext->key) != 0) {
					return 1;
				}
			}

			if (lnext->type == TNY_OBJ) {
				if (Tny_cmp(lnext->value.tny, rnext->value.tny) != 0) {
					return 1;
				}
			} else if (lnext->type == TNY_BIN) {
				if (lnext->size != rnext->size) {
					return 1;
				}

				if (memcmp(lnext->value.ptr, rnext->value.ptr, lnext->size) != 0) {
					return 1;
				}
			} else if (lnext->value.num != lnext->value.num) {
				return 1;
			}

			lnext = lnext->next;
			rnext = rnext->next;
		}

		if (lnext == NULL && rnext == NULL) {
			return 0;
		} else {
			return 1;
		}
	} else {
		return 1;
	}
}

int serialize_deserialize(Tny *tny)
{
	void *dump = NULL;
	Tny *newObj = NULL;
	size_t size = 0;
	int error = 1;

	if (tny != NULL) {
		size = Tny_dumps(tny, &dump);
		if (size > 0) {
			newObj = Tny_loads(dump, size);
			if (newObj != NULL) {
				if (Tny_cmp(tny, newObj) == 0) {
					if (*tny->docSizePtr == *newObj->docSizePtr) {
						error = 0;
					}
				}
			}
			Tny_free(newObj);
		}
		free(dump);
	}


	return error;
}

int main(void)
{
	Tny *root = NULL;
	Tny *embedded = NULL;
	Tny *tmp = NULL;
	char *message = "Message";
	uint32_t ui32 = 0xB16B00B5;
	uint64_t ui64 = 0xDEADBEEFABAD1DEAlu;
	char c = 'A';
	double flt = 13.37f;
	char *typesStr[] = {"BINARY", "INT32", "INT64", "CHAR", "NULL", "DOUBLE"};
	TnyType types[] = {TNY_BIN, TNY_INT32, TNY_INT64, TNY_CHAR, TNY_NULL, TNY_DOUBLE};
	void *values[] = {message, &ui32, &ui64, &c, NULL, &flt};
	size_t sizes[] = {strlen(message),0, 0, 0, 0, 0};
	char *keys[] = {"Key1", "Key2", "Key3", "Key4", "Key5", "Key6"};
	char corruptedObj[] = {0x01, 0x01, 0x00, 0x00, 0x00, 0x04, 0x08, 0x00, 0x00,
						   0x00, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65};
	int errors = 0;
	uint32_t counter = 0;
	uint32_t i = 0;

	/****************************************************
	 *  WARNING: BEHAVIOUR CHANGED!                     *
	 *  ----------------------------------------------  *
	 *                                                  *
	 *  Tny now performs a deep copy if you add an      *
	 *  object to another object. This means that you   *
	 *  have to free every created object yourself.     *
	 *                                                  *
	 *  This is because the size of an object now gets  *
	 *  calculated while adding new elements to the     *
	 *  list.                                           *
	 ****************************************************/


	/* Checking every datatype in an array. */
	for (i = 0; i < 6; i++) {
		root = Tny_add(NULL, TNY_ARRAY, NULL, NULL, 0);
		root = Tny_add(root, types[i], NULL, values[i], sizes[i]);
		if(serialize_deserialize(root)) {
			printf("Test with type %s in an array failed.\n", typesStr[i]);
			errors++;
		}
		Tny_free(root);
	}

	/* Checking every datatype in a dictionary. */
	for (i = 0; i < 6; i++) {
		root = Tny_add(NULL, TNY_DICT, NULL, NULL, 0);
		root = Tny_add(root, types[i], keys[i], values[i], sizes[i]);
		if(serialize_deserialize(root)) {
			printf("Test with type %s in a dictionary failed.\n", typesStr[i]);
			errors++;
		}
		Tny_free(root);
	}

	/* Adding every datatype to an array. */
	root = Tny_add(NULL, TNY_ARRAY, NULL, NULL, 0);
	for (i = 0; i < 6; i++) {
		root = Tny_add(root, types[i], NULL, values[i], sizes[i]);
	}
	if (serialize_deserialize(root)) {
		printf("Test with all types in an array failed.\n");
		errors++;
	}
	Tny_free(root);

	/* Adding every datatype to a dictionary. */
	root = Tny_add(NULL, TNY_DICT, NULL, NULL, 0);
	for (i = 0; i < 6; i++) {
		root = Tny_add(root, types[i], keys[i], values[i], sizes[i]);
	}
	if (serialize_deserialize(root)) {
		printf("Test with all types in a dictionary failed.\n");
		errors++;
	}
	Tny_free(root);


	/* Adding a dictionary to an array. */
	root = Tny_add(NULL, TNY_ARRAY, NULL, NULL, 0);
	for (i = 0; i < 6; i++) {
		root = Tny_add(root, types[i], NULL, values[i], sizes[i]);
	}
	embedded = Tny_add(NULL, TNY_DICT, NULL, NULL, 0);
	for (i = 0; i < 6; i++) {
		embedded = Tny_add(embedded, types[i], keys[i], values[i], sizes[i]);
	}
	Tny_add(root, TNY_OBJ, NULL, embedded, 0);
	if (serialize_deserialize(root)) {
		printf("Test with a sub document in an array failed.\n");
		errors++;
	}

	if (root != NULL && embedded != NULL) {
		/* Fetching a random element from an array */
		tmp = Tny_at(root, 2);
		if (tmp == NULL || tmp->value.num != *(uint64_t*)values[2]) {
			printf("Fetching element at position 2 failed!\n");
			errors++;
		}

		/* Fetching a random element from a dictionary */
		tmp = Tny_get(embedded, "Key3");
		if (tmp == NULL || tmp->value.num != *(uint64_t*)values[2]) {
			printf("Fetching element with key 'Key3' failed!\n");
			errors++;
		}

		/* Remove an element. */
		Tny_remove(Tny_at(root, 2));
		tmp = Tny_at(root, 2);
		if (tmp == NULL || tmp->value.chr != *(char*)values[3]) {
			printf("Deleting element at position 2 failed!\n");
			errors++;
		}

		/* Adding element after position 2 */
		ui32 = 0x12345678;
		tmp = Tny_at(root, 2);
		Tny_add(tmp, TNY_INT32, NULL, &ui32, 0);
		tmp = Tny_at(root, 3);
		if (tmp == NULL || tmp->value.num != ui32) {
			printf("Adding element after position 2 failed!\n");
			errors++;
		}
	}
	Tny_free(embedded);
	Tny_free(root);

	/* Updating the value of a dictionary entry */
	root = Tny_add(NULL, TNY_DICT, NULL, NULL, 0);
	ui32 = 5;
	root = Tny_add(root, TNY_INT32, "Key", &ui32, 0);
	ui32 = 10;
	root = Tny_add(root, TNY_INT32, "Key", &ui32, 0);
	if (root == NULL || Tny_get(root, "Key")->value.num != ui32) {
		printf("Updating the value of a dictionary entry failed!\n");
		errors++;
	}

	if (serialize_deserialize(root)) {
		printf("After updating the value of a dictionary entry docSize got corrupted!\n");
		errors++;
	}
	Tny_free(root);


	/* Further testing of docSize. */
	root = Tny_add(NULL, TNY_ARRAY, NULL, NULL, 0);
	embedded = Tny_add(NULL, TNY_DICT, NULL, NULL, 0);
	embedded = Tny_add(embedded, types[0], keys[0], values[0], sizes[0]);
	root = Tny_add(root, TNY_OBJ, NULL, embedded, 0);
	tmp = Tny_at(root, 0);
	tmp = Tny_get(tmp->value.tny, keys[0]);
	Tny_remove(tmp);
	if (serialize_deserialize(root)) {
		printf("tny->docSize got corrupted!\n");
		errors++;
	}

	/* If you are using the same variable everywhere, be sure to store the root element
	   before you are deleting something. */
	root = root->root;
	Tny_remove(Tny_at(root, 0));
	if (serialize_deserialize(root)) {
		printf("tny->docSize got corrupted!\n");
		errors++;
	}
	Tny_free(embedded);
	Tny_free(root);


	/* Testing the iterator functions. */
	root = Tny_add(NULL, TNY_ARRAY, NULL, NULL, 0);
	for (i = 0; i < 10; i++) {
		root = Tny_add(root, TNY_INT32, NULL, &i, 0);
	}

	counter = 0;
	root = root->root;
	while (Tny_hasNext(root)) {
		root = Tny_next(root);
		if (root->value.num != counter) {
			printf("Iterator Test (1) failed!\n");
			errors++;
			break;
		}
		counter++;
	}
	if (counter != root->root->size) {
		printf("Iterator Test (2) failed!\n");
		errors++;
	}
	Tny_free(root);


	/* Loading a corrupted document containing a corrupted size field. */
	root = Tny_loads(corruptedObj, sizeof(corruptedObj));
	if (root == NULL || root->size != 0) {
		printf("Loading of a corrupted document failed!\n");
		errors++;
	}
	Tny_free(root);

	printf("Tny tests completed with %u error(s).\n", errors);

	return EXIT_SUCCESS;
}
