#include<stdlib.h>

int main() {

	// volatile double* arr = (double*)malloc(sizeof(double) * 1000000);
	/*double arr[1000000];
	int i;

	int sum = 0;
	for ( i = 0; i < 1000000; i += i % 100) { // i will be increasing alternatingly between 1 and 2. i = {1, 2, 4, 5, 7, 8, 10 ...}
		// sum += arr[i-1];
		// i = i + change;
		// change = change * 2; 
		
		//int index = (i * i + 3 * i + 5) % 1000000; // Non-linear access pattern
		//int value = arr[index];
		//arr[i] = i;
		// index = (index * index + 3 * index + 7) % 1000000;
		arr[i] = i; 
	}*/

	/*for (i = 0; i < 10000; i += 5) {
		sum += arr[i];
	}*/
	double* arr = (double*) malloc (sizeof(double) * 1000000);
	int step = 128;
	int i = 0;
	for (i = 0 ; i < 1000000; i += step) {
		arr[i] += 1;
		if (step != 32) {
			step = 32;
		} else {
			step = 128;
		}
	}
	return 0;

}
