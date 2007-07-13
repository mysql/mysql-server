CFLAGS = -W -Wall -Wno-unused -g
CPPFLAGS = -I../include -I../newbrt
ydb.o: ../include/db.h ../newbrt/cachetable.h
libdb.so: ydb.c
	cc  $(CPPFLAGS)  ydb.c -shared -fPIC -o libdb.so $(CFLAGS)
libdb.a(ydb.o): ydb.o
