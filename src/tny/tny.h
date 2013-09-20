#ifndef TNY_H_
#define TNY_H_

#include <stdio.h>
#include <stdint.h>

#define HASNEXTDATA(X) if ((*pos) + X > length) break

typedef enum {
    ORDER_LITTLE_ENDIAN = 0x03020100ul,
    ORDER_BIG_ENDIAN = 0x00010203ul
} TnyEndianness;

static const union {
	unsigned char bytes[4];
	uint32_t value;
} tnyHostOrder = { { 0, 1, 2, 3 } };

#define HOST_ORDER (tnyHostOrder.value)

typedef enum {
	TNY_NULL,
	TNY_ARRAY,
	TNY_DICT,
	TNY_OBJ,
	TNY_BIN,
	TNY_CHAR,
	TNY_INT32,
	TNY_INT64,
	TNY_DOUBLE
} TnyType;

typedef struct _Tny {
	struct _Tny *prev;
	struct _Tny *next;
	struct _Tny *root;
	TnyType type;
	uint32_t size;
	char *key;
	union {
		struct _Tny *tny;
		void *ptr;
		uint64_t num;
		double flt;
		char chr;
	} value;
} Tny;

/*
	Tny_add:
	Adds a new element after the "prev" element. If prev is NULL, a new document will be
	created.

	Documents can have two types: TNY_ARRAY and TNY_DICT. These two types must only be used
	for a new document. For a sub-document or a embedded document use TNY_OBJ.

	If the document is of type TNY_DICT, the "key" must be set in every element.
	If the key is set even it is a document of type TNY_ARRAY every element containing the same
	key is replaced with the new one.
	This is because every key must appear only once per document.

	The "size" needs only be specified if the element is of type TNY_BIN.

	If the function succeeds it returns the new created element otherwise NULL.
*/
Tny* Tny_add(Tny *prev, TnyType type, char *key, void *value, uint64_t size);

/*
	Tny_remove:
	Removes an element or a complete document.

	if you pass an element somewhere in the document, the elements get deleted.
	But if you pass the beginning of a document (the element with the type TNY_ARRAY or TNY_DICT)
	then to whole document will get deleted.
*/
void Tny_remove(Tny *tny);

/*
	Tny_at:
	Returns the element at position "index".
	Works in a TNY_ARRAY and a TNY_DICT.
*/
Tny* Tny_at(Tny* tny, size_t index);

/*
	Tny_get:
	Returns the element with the specified key.
	Works in a TNY_ARRAY and a TNY_DICT.
*/
Tny* Tny_get(Tny* tny, char *key);

/*
	Tny_calcSize:
	Validates the document and returns the size needed for serialization.
	If the validation fails 0 is returned.
*/
size_t Tny_calcSize(Tny *tny);

/*
	Tny_dumps:
	Serializes the document and stores the result in "data".
	The memory needed for the serialization will get allocated.

	If the function succeeds the size of the serialized document is returned otherwise 0.
*/
size_t Tny_dumps(Tny *tny, void **data);

/*
	_Tny_dumps:
	Helper function for Tny_dumps.
	This function don't need to be called directly.
*/
size_t _Tny_dumps(Tny *tny, char *data, size_t pos);

/*
	Tny_loads:
	Deserializes a serialized document.

	If the function succeeds the deserialized document is returned otherwise NULL.
*/
Tny* Tny_loads(void *data, size_t length);

/*
	_Tny_loads:
	Helper function for Tny_loads.
	This function don't need to be called directly.
*/
Tny* _Tny_loads(char *data, size_t length, size_t *pos);

/*
	Tny_swapBytes32:
	Swaps a 32 Bit value to little endian and back to HOST_ORDER if
	HOST_ORDER is not little endian.

	The function returns "dest"
*/
uint32_t* Tny_swapBytes32(uint32_t *dest, uint32_t *src);

/*
	Tny_swapBytes64:
	Swaps a 64 Bit value to little endian and back to HOST_ORDER if
	HOST_ORDER is not little endian.

	The function returns "dest"
*/
uint64_t* Tny_swapBytes64(uint64_t *dest, uint64_t *src);

/*
	Tny_hasNext:
	Checks if there are more elements to fetch.

	Returns 1 if there are more elements, otherwise 0.
*/
int Tny_hasNext(Tny *tny);

/*
	Tny_next:
	Returns the next element, otherwise NULL.
*/
Tny* Tny_next(Tny *tny);

/*
	Tny_freeContent:
	Frees the value and the key if existing.
*/
void Tny_freeContent(Tny *tny);

/*
	Tny_free:
	Frees the document.
*/
void Tny_free(Tny *tny);

#endif /* TNY_H_ */
