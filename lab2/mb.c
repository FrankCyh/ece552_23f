#include <stdio.h>
#include <stdlib.h>
#include <time.h>
int modify_value(int a) {       // I created this function to create a return instruction for testing. Also it uses rand() inside which adds certain complexity.
    return a + (rand() % 5);   // A lot of lines for assmebly code. 
}    			       // Ended with lw $31,20($sp); lw $16,16($sp); addu $sp,$sp,24; j $31; .end modify_value


int main() {
    // srand(time(NULL));          // Add certain degree of randomness if needed. 
    int a = 10000;
    while (a > 5) {		// A loop to add conditional branches.
	a = a / 3;             
	a = modify_value(a);   // Call function for PC to jump to address.
    }
    int b = 2;                  
    for (int i = 0 ; i < 100; i++){   // Fixed Upperbound Conditional branches
        if (b * a > 32) {             // Add complexity to two-level predictor state
            break;
        }
	a = a - b;
	b = b + 1;
	a = (a > 0) ? a : a * (-1); // Software aspect not to let a < 0 to satisfy if condition
    }

    return 0;

}
/* Put it side by side so the screenshot would not take too much space in report.
main:					modify_value: 
	.frame $sp,24,$31			Presented above next to C code for better understanding.
	.mask  0x80000000, -8
	.fmask 0x000000000,0
	subu   $sp, $sp, 24
	jal    __main
	move   $4, $0
	jal    time
	move   $4, $2
	jal    srand
	lw     $31, 16($sp)
	addu   $sp, $sp, 24
	j      $31
	.end   main
*/
