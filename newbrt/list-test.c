#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include "list.h"

struct testlist {
    struct list next;
    int tag;
};

void testlist_init(struct testlist *tl, int tag) {
    tl->tag = tag;
}

void test_push_pop(int n) {
    int i;
    struct list head;

    list_init(&head);
    for (i=0; i<n; i++) {
        struct testlist *tl = (struct testlist *) malloc(sizeof *tl);
        assert(tl);
        testlist_init(tl, i);
        list_push(&head, &tl->next);
        assert(!list_empty(&head));
    }
    for (i=n-1; i>=0; i--) {
        struct list *list;
        struct testlist *tl;

        list = list_head(&head);
        tl  = list_struct(list, struct testlist, next);
        assert(tl->tag == 0);
        list = list_tail(&head);
        tl = list_struct(list, struct testlist, next);
        assert(tl->tag == i);
        list = list_pop(&head);
        tl = list_struct(list, struct testlist, next);
        assert(tl->tag == i);
        free(tl);
    }
    assert(list_empty(&head));
}

void test_push_pop_head(int n) {
    int i;
    struct list head;

    list_init(&head);
    for (i=0; i<n; i++) {
        struct testlist *tl = (struct testlist *) malloc(sizeof *tl);
        assert(tl);
        testlist_init(tl, i);
        list_push(&head, &tl->next);
        assert(!list_empty(&head));
    }
    for (i=0; i<n; i++) {
        struct list *list;
        struct testlist *tl;

        list = list_head(&head);
        tl  = list_struct(list, struct testlist, next);
        assert(tl->tag == i);
        list = list_tail(&head);
        tl = list_struct(list, struct testlist, next);
        assert(tl->tag == n-1);

        list = list_pop_head(&head);
        tl = list_struct(list, struct testlist, next);
        assert(tl->tag == i);
        free(tl);
    }
    assert(list_empty(&head));
}

void test_push_head_pop(int n) {
    int i;
    struct list head;

    list_init(&head);
    for (i=0; i<n; i++) {
        struct testlist *tl = (struct testlist *) malloc(sizeof *tl);
        assert(tl);
        testlist_init(tl, i);
        list_push_head(&head, &tl->next);
        assert(!list_empty(&head));
    }
    for (i=0; i<n; i++) {
        struct list *list;
        struct testlist *tl;

        list = list_head(&head);
        tl  = list_struct(list, struct testlist, next);
        assert(tl->tag == n-1);
        list = list_tail(&head);
        tl = list_struct(list, struct testlist, next);
        assert(tl->tag == i);

        list = list_pop(&head);
        tl = list_struct(list, struct testlist, next);
        assert(tl->tag == i);
        free(tl);
    }
    assert(list_empty(&head));
}

// cant move an empty list
void test_move_empty() {
    struct list h1, h2;

    list_init(&h1);
    list_init(&h2);
    list_move(&h1, &h2);
    assert(list_empty(&h2));
    assert(list_empty(&h1));
}

void test_move(int n) {
    struct list h1, h2;
    int i;

    list_init(&h1);
    list_init(&h2);
    for (i=0; i<n; i++) {
        struct testlist *tl = (struct testlist *) malloc(sizeof *tl);
        assert(tl);
        testlist_init(tl, i);
        list_push(&h2, &tl->next);
    }
    list_move(&h1, &h2);
    assert(!list_empty(&h1));
    assert(list_empty(&h2));
    i = 0;
    while (!list_empty(&h1)) {
        struct list *list = list_pop_head(&h1);
        struct testlist *tl = list_struct(list, struct testlist, next);
        assert(tl->tag == i);
        free(tl);
        i += 1;
    }
    assert(i == n);
}

int main() {
    test_push_pop(0);
    test_push_pop(8);
    test_push_pop_head(0);
    test_push_pop_head(8);
    test_push_head_pop(8);
    test_move(1);
    test_move(8);
    //    test_move_empty();

    return 0;
}

