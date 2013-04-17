// test explicit generation of a simple template function

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <assert.h>

template <class T> T my_max(T a, T b) {
    return a > b ? a : b;
}

template int my_max(int a, int b);

int main(int argc, char *argv[]) {
    assert(argc == 3);
    int a = atoi(argv[1]);
    int b = atoi(argv[2]);
    int m = my_max<int>(a, b);
    printf("%d %d %d\n", a, b, m);
    assert(m == (a > b ? a : b));
    return 0;
}
