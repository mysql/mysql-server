SRCS = $(wildcard *.cpp)
TARGETS = $(patsubst %.cpp,%,$(SRCS))

CPPFLAGS = -I../ -I../../include
CXXFLAGS = -Wall -g
LDLIBS = ../../lib/libtdb_cxx.a ../../lib/libdb.a -lz

ifeq ($(OSX),OSX)
 VGRIND=
else
 VGRIND=valgrind --quiet --error-exitcode=1 --leak-check=yes
endif

all: $(TARGETS)
$(TARGETS): $(DBCXX)

$(DBCXX):
	cd ..;make

clean:
	rm -rf $(TARGETS)

check: $(TARGETS)
	$(VGRIND) ./test1
	$(VGRIND) ./test1e
	rm -f foo.db
	$(VGRIND) ./db_create foo.db a b c d
	$(VGRIND) ./db_dump foo.db > foo.out
	(echo " 61";echo " 62";echo " 63";echo " 64")>foo.expectout
	diff foo.out foo.expectout
	$(VGRIND) ./db_dump_e foo.db > foo.out
	diff foo.out foo.expectout
	$(VGRIND) ./exceptions
