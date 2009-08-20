#ifndef TOKU_VARRAY_H
#define TOKU_VARRAY_H

// Variable sized arrays of pointers (like an STL vector<void *>)

struct varray;

// Allocate and initialize an array
// Effect: a new varray is allocated and its initial size is set to n
// Returns: 0 if success and *vap points to the new varray
int varray_create(struct varray **vap, int n);

// Returns: the current size of the array
int varray_current_size(struct varray *va);

// Free an array
// Effect: the varray at *vap is freed
void varray_destroy(struct varray **vap);

// Append an element to the end of the array
// Effect: The size of the array is 1 larger than before and the last 
// element is the new element
// Returns: 0 if success
int varray_append(struct varray *va, void *p);

// Sort the array elements
void varray_sort(struct varray *va, int (*)(const void *, const void *));
                  
// Apply a function to all of the elements in the array
// Effect: The function f is called for every element in the array, with
// p set to the element and with an extra argument
void varray_iterate(struct varray *va, void (*f)(void *p, void *extra), void *extra);

#endif
