#include "tny/tny.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>


int main(int argc, char **argv)
{
	struct timeval t0, t1;
	double creation = 0.0f;
	double serialization = 0.0f;
	double deserialization = 0.0f;
	Tny *array = NULL;
	Tny *dict = NULL;
	char *name = "John Doe";
	char *street = "Some street name";
	uint32_t streetnr = 10;
	int count = 100000;
	size_t size = 0;
	void *dump = NULL;

	gettimeofday(&t0, NULL);
	array = Tny_add(NULL, TNY_ARRAY, NULL, NULL, 0);
	for(int i = 0; i < count; i++) {
		dict = Tny_add(NULL, TNY_DICT, NULL, NULL, 0);
		dict = Tny_add(dict, TNY_BIN, "Name", name, sizeof(name));
		dict = Tny_add(dict, TNY_BIN, "Street", street, sizeof(street));
		dict = Tny_add(dict, TNY_INT32, "Nr", &streetnr, 0);
		array = Tny_add(array, TNY_OBJ, NULL, dict, 0);
		Tny_free(dict);
	}
	gettimeofday(&t1, NULL);
	creation = t1.tv_sec - t0.tv_sec + 1E-6 * (t1.tv_usec - t0.tv_usec);

	gettimeofday(&t0, NULL);
	size = Tny_dumps(array, &dump);
	gettimeofday(&t1, NULL);
	serialization = t1.tv_sec - t0.tv_sec + 1E-6 * (t1.tv_usec - t0.tv_usec);

	gettimeofday(&t0, NULL);
		dict = Tny_loads(dump, size);
		Tny_free(dict);
	gettimeofday(&t1, NULL);
	deserialization = t1.tv_sec - t0.tv_sec + 1E-6 * (t1.tv_usec - t0.tv_usec);


	printf("Created an array with %d objects in %.2g seconds.\n", count, creation);
	printf("The serialization of this object took %g seconds.\n", serialization);
	printf("The deserialization: of this dump took %g seconds.\n", deserialization);
	printf("The serialized document would be %luB long.\n", size);

	free(dump);
	Tny_free(array);

	return EXIT_SUCCESS;
}
