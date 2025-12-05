CC = gcc
CFLAGS = -fPIC -Wall -Wextra
LDFLAGS = -ldl 
TARGET1 = program1
TARGET2 = program2
LIBS = libeuclid_leibniz.so libnaive_wallis.so 

.PHONY: all clean test

all: $(LIBS) $(TARGET1) $(TARGET2)

libeuclid_leibniz.so: src/euclid_leibniz.c
	$(CC) $(CFLAGS) -shared -o $@ $^

libnaive_wallis.so: src/naive_wallis.c
	$(CC) $(CFLAGS) -shared -o $@ $^

program1: program1.c libeuclid_leibniz.so
	$(CC) -o $@ program1.c -L. -libeuclid_leibniz -Wl,-rpath,.

program2: program2.c contract.h
	$(CC) -o $@ program2.c $(LDFLAGS)

test: all
	@echo "=== Тестирование программы 1 (статическая линковка) ==="
	@echo "1 48 18" | ./program1 | grep -A1 "НОД"
	@echo "=== Тестирование программы 2 (динамическая загрузка) ==="
	@echo "1 48 18" | ./program2 | grep -A1 "НОД"

clean: 
	rm -f $(TARGET1) $(TARGET2) $(LIBS) *.o
