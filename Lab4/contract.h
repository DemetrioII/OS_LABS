#ifndef CONTRACT_H
#define CONTRACT_H

typedef int (*gcd_func)(int, int);

typedef double(*pi_func)(int);

typedef struct {
	gcd_func gcd;
	pi_func pi;
	const char* name;
} LibraryFunctions;

#endif
