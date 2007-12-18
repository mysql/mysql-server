SRCS = $(wildcard *.cpp)
TARGETS = $(patsubst %.cpp,%,$(SRCS))

DBCXX = ../dbt.o ../db.o ../dbenv.o
CPPFLAGS = -I../ -I../../include
CXXFLAGS = -Wall -g
LDFLAGS = -L../lib -ldb

$(TARGETS): $(DBCXX)

$(DBCXX):
	cd ..;make
all: $(TARGETS)

clean:
	rm -rf $(TARGETS)
