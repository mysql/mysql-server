CC=gcc
LD=$(CC)
SOURCES=memtest.c
OUTPUT=memtest
all: 
	$(CC) $(SOURCES) -o $(OUTPUT)	

debug: 
	$(CC) -g $(SOURCES) -o $(OUTPUT)

clean:	rm -rf *.o
	rm -rf core*
