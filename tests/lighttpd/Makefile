CC = /usr/local/musl/bin/musl-gcc
#LLC = ../../llvm/build/bin/llc

# Set according to the environment
uname := $(shell uname -s)
ifeq ($(uname), Linux)
EMCC := ../../src/emsdk/emscripten/1.38.12/emcc
else
EMCC := ../../build/emscripten/emcc
endif

obj = src/server.o src/response.o src/connections.o \
      src/inet_ntop_cache.o src/network.o src/network_write.o \
      src/configfile.o src/configparser.o src/proc_open.o \
      src/base64.o src/buffer.o src/log.o src/keyvalue.o \
      src/chunk.o src/http_chunk.o src/stream.o src/fdevent.o \
      src/gw_backend.o src/stat_cache.o src/plugin.o src/joblist.o \
      src/etag.o src/array.o src/data_string.o src/data_array.o \
      src/data_integer.o src/algo_sha1.o src/md5.o src/vector.o \
      src/fdevent_select.o src/fdevent_libev.o src/fdevent_poll.o \
      src/fdevent_linux_sysepoll.o src/fdevent_solaris_devpoll.o \
      src/fdevent_solaris_port.o src/fdevent_freebsd_kqueue.o src/data_config.o \
      src/crc32.o src/connections-glue.o src/configfile-glue.o \
      src/http-header-glue.o src/http_auth.o src/http_vhostdb.o \
      src/rand.o src/request.o src/sock_addr.o src/splaytree.o \
      src/status_counter.o src/safe_memclear.o

plugin = src/mod_indexfile.o src/mod_userdir.o src/mod_dirlisting.o src/mod_staticfile.o

src = $(obj:.o=.c)
bc_objs = $(obj:.o=.bc)
plugin_bc_objs = $(plugin:.o=.bc)

#curr_dir = $(shell pwd)
#libdir = ${curr_dir}/module

libdir := ../
EMCC_FLAG := --emit-symbol-map -DHAVE_CONFIG_H -DLIBRARY_DIR="\"$(libdir)\"" -DLIGHTTPD_STATIC -s ERROR_ON_UNDEFINED_SYMBOLS=0
EMCC_LINKFLAG := --emit-symbol-map -s ERROR_ON_UNDEFINED_SYMBOLS=0

# generic options for gcc
BASE_CFLAGS = -fPIC -fno-asynchronous-unwind-tables \
              -fno-jump-tables \
              -Wall -pedantic -Wno-unused-function -std=gnu11

#LIBSGXDIR = ..
CFLAGS = $(BASE_CFLAGS) \
         -fno-stack-protector -fvisibility=hidden \
	 -DHAVE_CONFIG_H -DLIBRARY_DIR="\"$(libdir)\"" -DLIGHTTPD_STATIC

all: server.js

server.js: $(bc_objs) $(plugin_bc_objs)
	@echo "LINK $$@"
	$(EMCC) $(EMCC_LINKFLAG) $^ -o $@

%.bc: %.c
	@echo "Generate bytecode: $<"
	$(EMCC) $(EMCC_FLAG) $< -o $@

server: $(obj) $(plugin)
	$(CC) -static  -o $@ $^
#	gcc -o $@ $^

$(obj): %.o: %.c
	$(CC) -static -c $< -o $@ -DHAVE_CONFIG_H -DLIBRARY_DIR="\"$(libdir)\"" -DLIGHTTPD_STATIC
#	gcc -c $< -o $@ -DHAVE_CONFIG_H -DLIBRARY_DIR="\"$(libdir)\"" -DLIGHTTPD_STATIC
	echo "cc <= $<"

$(plugin): %.o: %.c
	$(CC) -static -c $< -o $@ -DHAVE_CONFIG_H -DLIBRARY_DIR="\"$(libdir)\"" -DLIGHTTPD_STATIC
#	gcc -c $< -o $@ -DHAVE_CONFIG_H -DLIBRARY_DIR="\"$(libdir)\"" -DLIGHTTPD_STATIC
	echo "cc <= $<"

#$(Object_Files): %.o: %.c
#	$(CC) $(CFLAGS) -c $< -o $@
#	gcc -c $< -o $@ -dhave_config_h -dlibrary_dir="\"$(libdir)\""
#	echo "cc <= $<"

print-%  : ; @echo $* = $($*)

.PHONY: all clean

clean:
	rm -f server $(obj) $(plugin) $(bc_objs) $(plugin_bc_objs) *.wasm *.js

