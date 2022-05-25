
file_path := ../Enclave/sgxwasm
obj_path := 
cflags := -I../Enclave -g -Wall -Wextra -Werror
uname := $(shell uname -s)
osflags :=
ifeq ($(uname), Linux)
	osflags += -pthread -lm
endif

files := $(wildcard $(file_path)/*.c)
tmp_objs := $(files:.c=.o)
objs := $(subst $(file_path)/, $(obj_path), $(tmp_objs))

all: sgxwasm

sgxwasm: $(objs) main.o check_tsx.o
	$(CC) -o $@ $^ $(cflags) $(osflags)

main.o: main.c
	$(CC) -c -o $@ $< $(cflags)

ifeq ($(uname), Linux)
check_tsx.o: ../Enclave/sgxwasm/check_tsx.s
	$(CC) -c -o $@ $<
else
check_tsx.o: check_tsx.s
	$(CC) -c -o $@ $<
endif

$(objs): %.o: $(file_path)/%.c
	$(CC) -c -o $@ $< $(cflags)

print-%:
	@echo $* = $($*)

clean:
	rm -f $(objs) main.o sgxwasm check_tsx.o
