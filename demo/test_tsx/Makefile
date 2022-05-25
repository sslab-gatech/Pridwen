all: test_tsx

test_tsx: main.o fun.o
	gcc -o $@ $^

main.o: main.c
	gcc -c $< -o $@

fun.o: fun.s
	gcc -c $< -o $@

.PHONY: all clean
clean:
	rm -rf *.o test_tsx

