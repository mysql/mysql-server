#include <varray.h>
#include <memory.h>
#include <toku_assert.h>
#include <errno.h>

struct varray {
    int c;      // current number in the array
    int n;      // size of the array
    void **a;   // array of pointers
};

int varray_create(struct varray **vap, int n) {
    struct varray *va = toku_malloc(sizeof (struct varray));
    if (va == NULL) {
        int e = errno; return e;
    }
    va->n = n;
    va->c = 0;
    va->a = toku_malloc(va->n * (sizeof (void *)));
    if (va->a == NULL) {
        int e = errno;
        toku_free(va);
        return e;
    }
    *vap = va;
    return 0;
}

void varray_destroy(struct varray **vap) {
    struct varray *va = *vap; *vap = NULL;
    toku_free(va->a);
    toku_free(va);
}

int varray_current_size(struct varray *va) {
    return va->c;
}

int varray_append(struct varray *va, void *p) {
    if (va->c >= va->n) {
        void *newa = toku_realloc(va->a, 2 * va->n * sizeof (void *));
        if (newa == NULL) {
            int e = errno;
            assert(e != 0);
            return e;
        }
        va->a = newa;
        va->n *= 2;
    }
    va->a[va->c++] = p;
    return 0;
}

void varray_sort(struct varray *va, int (*compare)(const void *a, const void *b)) {
    qsort(va->a, va->c, sizeof (void *), compare);
}
                  
void varray_iterate(struct varray *va, void (*f)(void *p, void *extra), void *extra) {
    for (int i = 0; i < va->c; i++) 
        f(va->a[i], extra);
}
