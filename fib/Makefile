fib:
	icc fiblib.c -fPIC -c -o fiblib.o; \
icc --shared fiblib.o -L../cilk_icc -lcilkrts_static -nodefaultlibs -olibfib.so; \
icc -L. -lfib -pthread -lstdc++ fib.c -o fib
clean:
	rm -rf fib libfib.so fiblib.o
