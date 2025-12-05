#include <stdlib.h>
#include <stdio.h>

int gcd_naive(int a, int b) {
	if (a < 0) a = -a;
	if (b < 0) b = -b;

	int min = (a < b) ? a : b;
	int res = 1;
	for (int i = 2; i <= min; ++i) {
		if (a % i == 0 && b % i == 0) {
			res = i;
		}
	}

	return res;
}

double pi_wallis(int iterations) {
	if (iterations <= 0) return 0.0;

	double pi = 1.0;
	for (int i = 1; i <= iterations; ++i) {
		double n = (double)i;
		double term = (4.0 * n * n) / (4.0 * n * n - 1.0);
		pi *= term;
	}
	return 2.0 * pi;
}
