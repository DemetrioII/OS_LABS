#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include "contract.h"

const char* LIBRARY_NAMES[] = {
	"./libeuclid_leibniz.so",
	"./libnaive_wallis.so"
};
const int NUM_LIBS = 2;

void* current_lib_handle = NULL;
LibraryFunctions current_lib;
int current_lib_index = 0;

int load_library(int index) {
	if (index < 0 || index >= NUM_LIBS) {
		return 0;
	}

	if (current_lib_handle != NULL) {
		dlclose(current_lib_handle);
	}

	current_lib_handle = dlopen(LIBRARY_NAMES[index], RTLD_LAZY);
	if (!current_lib_handle) {
		fprintf(stderr, "Ошибка загрузки библиотеки: %s\n", dlerror());
		return 0;
	}

	gcd_func gcd_ptr = (gcd_func)dlsym(current_lib_handle, index == 0 ? "gcd_euclid" : "gcd_naive");
	pi_func pi_ptr = (pi_func)dlsym(current_lib_handle, index == 0 ? "pi_leibniz" : "pi_wallis");

	if (!gcd_ptr || !pi_ptr) {
		fprintf(stderr, "Ошибка загрузки функции: %s\n", dlerror());
		dlclose(current_lib_handle);
		return 0;
	}

	current_lib.gcd = gcd_ptr;
	current_lib.pi = pi_ptr;
	current_lib.name = index == 0 ? "Euclid+Leibniz" : "Naive+Wallis";
	current_lib_index = index;

	return 1;
}

void print_library_info() { 
	printf("Доступные библиотеки:\n");
	for (int i = 0; i < NUM_LIBS; ++i) {
		printf("	%d: %s", i, LIBRARY_NAMES[i]);
		if (i == current_lib_index)
			printf(" (текущая)");

		printf("\n");
	}
	printf("\n");
}

int main() {
	printf("Программа 2: Динамическая загрузка библиотек\n\n");
	if (!load_library(0)) {
		fprintf(stderr, "Не удалось загрузить библиотеку по умолчанию\n");
		return 1;
	}

	print_library_info();

	printf("Доступные команды:\n");
	printf("	0 - переключить библиотеку\n");
	printf("	1 a b - Вычислить НОД 2 чисел a и b\n");
	printf("	2 n - Вычислить Пи с n итерациями\n");
	printf("	i - информация о текущей библиотеке\n");
	printf("	q - выход\n\n");

	char command[100];
	while (1) {
		printf("> ");
		if (fgets(command, sizeof(command), stdin) == NULL) {
			break;
		}

		command[strcspn(command, "\n")] == 0;
		if(command[0] == 'q' || command[0] == 'Q') {
			break;
		}

		if (command[0] == '0') {
			int new_index = (current_lib_index + 1) % NUM_LIBS;
			if (load_library(new_index)) {

				printf("Переключено на библиотеку: %s\n", current_lib.name);
			} else {
				printf("Ошибка переключения библиотеки\n");
			}
		}

		else if (command[0] == 'i' || command[0] == 'I') {
			printf("Текущая библиотека: %s\n", current_lib.name);
			printf("Индекс: %d\n", current_lib_index);
			printf("Путь: %s\n", LIBRARY_NAMES[current_lib_index]);
		}
		else if (command[0] == '1') {
			int a, b;
			if (sscanf(command + 1, "%d %d", &a, &b) == 2) {
				int result = current_lib.gcd(a, b);
				printf("НОД(%d, %d) = %d\n", a, b, result);
			} else {
				printf("Ошибка: требуется 2 числа\n");
			}
		}
		else if (command[0] == '2') {
			int n;
			if (sscanf(command + 1, "%d", &n) == 1) {
				if (n > 0) {
					double result = current_lib.pi(n);
					printf("Пи(%d итераций) = %.15f\n", n, result);
				} else {
					printf("Ошибка: количество итераций должно быть положительным числом\n");
				}
			} else {
				printf("Ошибка: требуется одно число\n");
			}
		}
		else if (command[0] != '\0') {
			printf("Неизвестная команда\n");
		}
	}

	if (current_lib_handle != NULL) {
		dlclose(current_lib_handle);
	}

	return 0;
}
