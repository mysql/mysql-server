CPPFLAGS = -I../include
CXXFLAGS = -Wall -g
CC = c++
LDFLAGS = -lz
SRCS = $(wildcard *.cpp)
OBJS = $(patsubst %.cpp, %.o, $(SRCS))

LIBNAME = libtdb_cxx

default: install
install: $(LIBNAME).a
	cp $< ../lib/
$(OBJS): ../include/db_cxx.h
test1: test1.o dbt.o db.o dbenv.o ../lib/libdb.a

$(LIBNAME).a: $(OBJS)
	$(AR) rv $@ $(OBJS)
clean:
	rm -f $(OBJS) $(LIBNAME).a $(LIBNAME).so


