/** @file
 *
 *	The Tny binary format looks like this (ABNF):
 *
 * \code{.txt}
 * 	Document            =  (ArrayHeader *ArrayElement) / (DictionaryHeader *DictionaryElement)
 *	ArrayHeader         =  ArrayType NumberOfElements
 *	DictionaryHeader    =  DictionaryType NumberOfElements
 *	NumberOfElements    =  int32
 *	; Dictionary element
 *	DictionaryElement   =  NullType   Key
 *	DictionaryElement   =/ ObjectType Key ObjectValue
 *	DictionaryElement   =/ BinaryType Key BinaryValue
 *	DictionaryElement   =/ CharType   Key CharValue
 *	DictionaryElement   =/ Int32Type  Key Int32Value
 *	DictionaryElement   =/ Int64Type  Key Int64Value
 *	DictionaryElement   =/ DoubleType Key DoubleValue
 *	Key                 =  int32 1*(%x01-FF) %x00
 *	; Array element
 *	ArrayElement        =  NullType
 *	ArrayElement        =/ ObjectType 	ObjectValue
 *	ArrayElement        =/ BinaryType 	BinaryValue
 *	ArrayElement        =/ CharType 	CharValue
 *	ArrayElement        =/ Int32Type 	Int32Value
 *	ArrayElement        =/ Int64Type 	Int64Value
 *	ArrayElement        =/ DoubleType 	DoubleValue
 *	; Types
 *	NullType            =  %x00
 *	ArrayType           =  %x01
 *	DictionaryType      =  %x02
 *	ObjectType          =  %x03
 *	BinaryType          =  %x04
 *	CharType            =  %x05
 *	Int32Type           =  %x06
 *	Int64Type           =  %x07
 *	DoubleType          =  %x08
 *	; Values
 *	ObjectValue         =  Document
 *	BinaryValue         =  int32 *CharValue
 *	CharValue           =  %x00-FF
 *	Int32Value          =  int32
 *	Int64Value          =  int64
 *	DoubleValue         =  int64
 *	int32               =  4(%x00-FF)
 *	int64               =  8(%x00-FF)
 *	\endcode
 */
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

/** \brief TnyType contains every supported type.
 *
 *  \enum TnyType
 */
typedef enum {
	TNY_NULL,		/**< NULL type */
	TNY_ARRAY,		/**< Document type: Array */
	TNY_DICT,		/**< Document type: Dictionary */
	TNY_OBJ,		/**< Object type: Used for sub documents. */
	TNY_BIN,		/**< Binary type: Used for binary data (the size has to be specified). */
	TNY_CHAR,		/**< Character type: Used for storing a single character. */
	TNY_INT32,		/**< 32 bit integer type. */
	TNY_INT64,		/**< 64 bit integer type. */
	TNY_DOUBLE		/**< Double-precision floating-point number. */
} TnyType;

/** \brief Tny is the main type. Every Tny-document
 * 		   consists of chained Tny-elements.
 */
typedef struct _Tny {
	struct _Tny *prev;			/**< Points to the previous element. */
	struct _Tny *next;			/**< Points to the next element. */
	struct _Tny *root;			/**< Points to the root element of the document. */
	TnyType type;				/**< Contains the type of the element.
									 If the element is the root element, it contains the document type. */
	size_t docSize;				/**< Contains the size in bytes of the value.
	 	 	 	 	 	 	 	 	 If this is the root element, it contains the size of the document. */
	size_t *docSizePtr;			/**< Points to the docSize element of the root element where the document size is stored. */
	uint32_t size;				/**< Contains the size of the value. If this is the root element, it
	 	 	 	 	 	 	 	 	 contains the number of elements stored in the document. */
	char *key;					/**< Contains the key of the element if the document is of type TNY_DICT. */
	union {
		struct _Tny *tny;
		void *ptr;
		uint64_t num;
		double flt;
		char chr;
	} value;					/**< Union to access the value depending on the type. */
} Tny;

/** \brief Adds a new element after the \p prev element.
 *
 *	\param[in] prev
 *				is the previous element. If \p prev is NULL, a new document will be created.
 *	\param[in] type
 *				is the type of the new element. If this is the first element
 *				you can either use #TNY_ARRAY for an array or #TNY_DICT for a dictionary.
 *	\param[in] key
 *				If the document is of type #TNY_DICT, the \p key must be set in every element.
 *				Otherwise \p key can be NULL.
 *	\param[in] value
 *				is the value of the new element. If the type is #TNY_OBJ a deep copy of \p value
 *				will be performed.
 *	\param[in] size
 *				needs only to be set if the element is of type #TNY_BIN. Otherwise it can be 0.
 *	\return
 *				If the function succeeds it returns the new created element, otherwise NULL.
 */
Tny* Tny_add(Tny *prev, TnyType type, char *key, void *value, uint64_t size);

/** \brief Performs a deep copy of the \p src object.
 *
 *	\param[in] docSizePtr
 *				is only needed for internal use of Tny and can be NULL.
 *	\param[in] src
 *				is the source document which will be copied.
 *	\returns
 *				the newly copied element. If any error occurs, NULL is returned.
 */
Tny* Tny_copy(size_t *docSizePtr, const Tny *src);

/** \brief Removes an element or a complete document.
 *
 *	\param[in] tny
 *				is the element which shall be deleted. If an element somewhere in the document is
 *				passed, the elements get deleted. But if you pass the beginning of a document
 *				(the element with the type #TNY_ARRAY or #TNY_DICT) then to entire document will be deleted.
 */
void Tny_remove(Tny *tny);

/** \brief Returns the element at position \p index.
 *
 *	Works in a TNY_ARRAY and a TNY_DICT.
 *
 *	\param[in] tny
 *				is the document or an element somewhere in the document.
 *	\param[in] index
 *				is the position of the element in the document \p tny.
 *	\returns
 *				the element if the function succeeds, otherwise NULL.
 */
Tny* Tny_at(const Tny* tny, size_t index);

/** \brief Returns the element with the specified key.
 *
 *	Works in a TNY_ARRAY and a TNY_DICT.
 *
 *	\param[in] tny
 *				is the document or an element somewhere in the document.
 *	\param[in] key
 *				is the key of the element in the document \p tny.
 *	\returns
 *				the element if the function succeeds, otherwise NULL.
 */
Tny* Tny_get(const Tny* tny, const char *key);

/** \brief Serializes a document.
 *
 *	Serializes the document and stores the result in \p data.
 *	The memory needed for the serialization will get allocated.
 *
 *	\param[in] tny
 *				is the document which shall be serialized.
 *	\param[out] data
 *				is the position where the serialized document is copied to.
 *	\returns
 *				the size in bytes of the serialized document. If the function fails,
 *				0 is returned.
 */
size_t Tny_dumps(const Tny *tny, void **data);

/** \brief Deserializes a serialized document.
 *
 *	\param[in] data
 *				contains the serialized document.
 *	\param[in] length
 *				is the size in bytes of the serialized document.
 *	\returns
 *				the deserialized document. If the function fails, NULL is returned.
 */
Tny* Tny_loads(void *data, size_t length);

/** \brief Checks if there are more elements to fetch.
 *
 *	Simple iterator function which makes it easy to walk through an Tny document.
 *
 *	\param[in] tny
 *				is the current element.
 *	\returns
 *				1 if there are more elements, otherwise 0.
 */
int Tny_hasNext(const Tny *tny);

/** \brief Returns the next element
 *
 *	\param[in] tny
 *				is the last element fetched by \link Tny_next \endlink.
 *	\returns
 *				the next element in the list, otherwise NULL.
 */
Tny* Tny_next(const Tny *tny);

/** \brief Frees the document.
 *
 * 	\param[in] tny
 * 				is the document which shall be free'd.
 */
void Tny_free(Tny *tny);

#endif /* TNY_H_ */
