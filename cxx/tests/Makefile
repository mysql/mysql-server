SRCS = $(wildcard *.cpp)
TARGETS = $(patsubst %.cpp,%,$(SRCS))

DBCXX = ../dbt.o ../db.o ../dbenv.o ../dbc.o
CPPFLAGS = -I../ -I../../include
CXXFLAGS = -Wall -g
LDLIBS = ../../lib/libdb.a -lz

all: $(TARGETS)
$(TARGETS): $(DBCXX)

$(DBCXX):
	cd ..;make

clean:
	rm -rf $(TARGETS)
