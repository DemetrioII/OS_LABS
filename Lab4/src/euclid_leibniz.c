#include <stdio.h>

int gcd_euclid(int a, int b) {
	while (b != 0) {
		int t = b;
		b = a % b;
		a = t;
	}
	if (a < 0)
		return -a;
	return a;
}

double pi_leibniz(int iterations) {
	if (iterations <= 0) return 0.0;

	double pi = 0.0;
	for (int i = 0; i < iterations; ++i) {
		double term = 1.0 / (2 * i + 1);
		if (i % 2 == 0) {
			pi += term;
		} else {
			pi -= term;
		}
	}
	return 4.0 * pi;
}
