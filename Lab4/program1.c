#include <stdio.h>
#include <stdlib.h>
#include "contract.h"

extern int gcd_euclid(int a, int b);
extern double pi_leibniz(int iterations);

int main() {
	LibraryFunctions lib = {
		.gcd = gcd_euclid,
		.pi = pi_leibniz,
		.name="Euclid+Leibniz (static link)"
	};

	printf("Программа 1: Использование библиотеки на этапе компиляции\n");
	printf("Текущая библиотека: %s\n\n", lib.name);
	printf("Доступные команды:\n");
	printf("\t0 - информация о библиотеке\n");
	printf("\t1 a b - вычислить НОД чисел a и b\n");
	printf("\t2 n - вычислить Пи с n итерациями\n");
	printf("\tq - выход\n\n");

	char command[100];
	while (1) {
		printf("> ");
		if (fgets(command, sizeof(command), stdin) == NULL) {
			break;
		}

		if (command[0] == 'q' || command[0] == 'Q') {
			break;
		}

		if (command[0] == '0') {
			printf("Текущая библиотека: %s\n", lib.name);
			printf("Алгоритм НОД: Евклида\n");
			printf("Алгоритм Пи: Формула Лейбница\n");
		} else if (command[0] == '1') {
			int a, b;
			if (sscanf(command + 1, "%d %d", &a, &b) == 2) {
				int result = lib.gcd(a, b);
				printf("НОД(%d, %d) = %d\n", a, b, result);
			} else {
				printf("Ошибка: требуется 2 числа\n");
			}
		} else if (command[0] == '2') {
			int n;
			if (sscanf(command + 1, "%d", &n) == 1) {
				if (n > 0) {
					double result = lib.pi(n);
					printf("Число Пи:(%d итераций)= %.15f\n", n, result);
				} else {
					printf("Ошибка: количество итераций должно быть положительным числом!\n");
				}
			} else {
				printf("Ошибка: требуется одно число!\n");
			}
		} else if (command[0] != '\n') {
			printf("Неизвестная команда\n");
		}
	}

	return 0;
}
