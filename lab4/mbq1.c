#include <stdio.h>
#include <stdlib.h>

int main() {

	double* arr =  (double*) malloc(10000*sizeof(double));
	int i;
	int value;
	for (i = 0; i < 10000; i += 1) {	// I set block_size to be 8 bytes. Known that one double is 8 byte, the next index is basically accessing the next block.
		arr[i] += 1;
		// Do nothing
		// printf("%f", arr[i]);
	}
	return 0;
}
