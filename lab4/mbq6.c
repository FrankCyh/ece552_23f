#include <stdio.h>

int main() {
	//double* arr = (double*) malloc (sizeof(double) * 100000);
        double arr[100000];
	int num = 0;
	int step = 64;
        int i;
        for (i = 0 ; i < 100000; i += step) {
		num++;
		if (num == 3) { // To save space on report screenshot...
			if (step != 64) {step = 64;} else {step = 128;}
			num = 0;
		}
		arr[i] += 1;
	}
        return 0;
}
