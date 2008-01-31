// Clip horizontally (100 chars by default)
#include <stdio.h>
#include <stdlib.h>
int main (int argc, char *argv[]) {
    int targcol=100;
    int c;
    int col=0;
    if (argc>1) {
	targcol=atoi(argv[1]);
    }
    while ((c=getchar())!=EOF) {
	if (c=='\n') { col=0; putchar(c); }
	else if (col==targcol) { col++; printf("..."); }
	else if (col>targcol) { col++; }
	else { col++; putchar(c); }
    }
    return 0;
}
