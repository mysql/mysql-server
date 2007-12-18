SRCS = $(wildcard *.cpp)
TARGETS = $(patsubst %.cpp,%,$(SRCS))

DBCXX = ../dbt.o ../db.o ../dbenv.o ../dbt.o
CPPFLAGS = -I../ -I../../include
CXXFLAGS = -Wall -g
LDFLAGS = -L../lib -ldb

$(TARGETS): $(DBCXX)

all: $(TARGETS)

clean:
	rm -rf $(TARGETS)
