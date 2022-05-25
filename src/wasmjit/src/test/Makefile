all: test_assembler test_label test_register test_util test_vector

clean:
	rm *.o test_assembler test_label test_register test_util test_vector

LCFLAGS ?= -I..

test_assembler: test_assembler.o ../wasmjit/register.o ../wasmjit/assembler.o ../wasmjit/util.o ../wasmjit/vector.o
	$(CC) -o $@ $^ $(LCFLAGS)

test_label: test_label.o ../wasmjit/register.o ../wasmjit/assembler.o ../wasmjit/util.o ../wasmjit/vector.o
	$(CC) -o $@ $^ $(LCFLAGS)

test_register: test_register.o ../wasmjit/register.o ../wasmjit/assembler.o ../wasmjit/util.o ../wasmjit/vector.o
	$(CC) -o $@ $^ $(LCFLAGS)

test_util: test_util.o ../wasmjit/util.o ../wasmjit/vector.o
	$(CC) -o $@ $^ $(LCFLAGS)

test_vector: test_vector.o ../wasmjit/util.o ../wasmjit/vector.o
	$(CC) -o $@ $^ $(LCFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(LCFLAGS)
