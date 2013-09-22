#ifndef TNY_H_
#define TNY_H_

#include <stddef.h>
#include <stdint.h>

typedef enum {
    ORDER_LITTLE_ENDIAN = 0x03020100ul,
    ORDER_BIG_ENDIAN = 0x00010203ul
} TnyEndianness;

union tnyHostOrder {
	unsigned char bytes[4];
	uint32_t value;
};

extern union tnyHostOrder tnyHostOrder;

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
	size_t docSize;
	size_t *docSizePtr;
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
 * 	Tny_copy:
 * 	Performs a deep copy of the "src"-object.
 * 	"docSizePtr" is only needed for internal use of Tny and can be NULL.
 */
Tny* Tny_copy(size_t *docSizePtr, const Tny *src);

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
Tny* Tny_at(const Tny* tny, size_t index);

/*
	Tny_get:
	Returns the element with the specified key.
	Works in a TNY_ARRAY and a TNY_DICT.
*/
Tny* Tny_get(const Tny* tny, const char *key);

/*
	Tny_dumps:
	Serializes the document and stores the result in "data".
	The memory needed for the serialization will get allocated.

	If the function succeeds the size of the serialized document is returned otherwise 0.
*/
size_t Tny_dumps(const Tny *tny, void **data);

/*
	Tny_loads:
	Deserializes a serialized document.

	If the function succeeds the deserialized document is returned otherwise NULL.
*/
Tny* Tny_loads(void *data, size_t length);

/*
	Tny_hasNext:
	Checks if there are more elements to fetch.

	Returns 1 if there are more elements, otherwise 0.
*/
int Tny_hasNext(const Tny *tny);

/*
	Tny_next:
	Returns the next element, otherwise NULL.
*/
Tny* Tny_next(const Tny *tny);

/*
	Tny_free:
	Frees the document.
*/
void Tny_free(Tny *tny);

#endif /* TNY_H_ */
