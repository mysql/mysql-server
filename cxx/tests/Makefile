SRCS = $(wildcard *.cpp)
TARGETS = $(patsubst %.cpp,%,$(SRCS))

CPPFLAGS = -I../ -I../../include
CXXFLAGS = -Wall -g
LDLIBS = ../../lib/libtdb_cxx.a ../../lib/libdb.a -lz

all: $(TARGETS)
$(TARGETS): $(DBCXX)

$(DBCXX):
	cd ..;make

clean:
	rm -rf $(TARGETS)

check: $(TARGETS)
	./test1
	./test1e
