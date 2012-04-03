CFLAGS=-W -Wall -Werror -ggdb3 -O3 -std=c99
#CPPFLAGS=-I../xz-5.0.3/src/liblzma/api
LDLIBS=-llzma
#LDFLAGS=-L../xz-5.0.3/src/liblzma/.libs -Wl,-rpath,../xz-5.0.3/src/liblzma/.libs

default: lzma-compress lzma-compress-mt
lzma-compress-mt: CFLAGS+=-pthread
clean:
	rm -f lzma-compress lzma-compress-mt
