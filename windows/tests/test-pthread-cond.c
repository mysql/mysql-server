#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <windows.h>
#include "toku_pthread.h"

struct q {
    toku_pthread_mutex_t m;
    toku_pthread_cond_t r;
    toku_pthread_cond_t w;
    void *item;
};

static void q_init(struct q *q) {
    toku_pthread_mutex_init(&q->m, NULL);
    toku_pthread_cond_init(&q->r, NULL);
    toku_pthread_cond_init(&q->w, NULL);
    q->item = NULL;
}

static void q_destroy(struct q *q) {
    toku_pthread_cond_destroy(&q->w);
    toku_pthread_cond_destroy(&q->r);
    toku_pthread_mutex_destroy(&q->m);
}

static void *q_get(struct q *q) {
    void *item;
    toku_pthread_mutex_lock(&q->m);
    while (q->item == NULL)
        toku_pthread_cond_wait(&q->r, &q->m);
    item = q->item; q->item = NULL;
    toku_pthread_mutex_unlock(&q->m);
    toku_pthread_cond_signal(&q->w);
    return item;
}

static void q_put(struct q *q, void *item) {
    toku_pthread_mutex_lock(&q->m);
    while (q->item != NULL) 
        toku_pthread_cond_wait(&q->w, &q->m);
    q->item = item;
    toku_pthread_mutex_unlock(&q->m);
    toku_pthread_cond_signal(&q->r);
}
    
static void *writer(void *arg) {
    struct q *q = arg;
    int i;

    printf("%s %p %lu\n", __FUNCTION__, arg, GetCurrentThreadId());
    for (i=0; i<100; i++) 
        q_put(q, (void*)(i+1));
    return arg;
}

static void *reader(void *arg) {
    struct q *q = arg;
    int i;

    printf("%s %p %lu\n", __FUNCTION__, arg, GetCurrentThreadId());
    for (i=0; i<100; i++) {
        void *item = q_get(q);
        printf("%p\n", item); fflush(stdout);
        Sleep(i);
    }
    return arg;
}

int main(void) {
    int i;
    void *ret;
    toku_pthread_t t[2];
    struct q q;

    q_init(&q);
    toku_pthread_create(&t[0], NULL, reader, &q);
    toku_pthread_create(&t[1], NULL, writer, &q);
    for (i=0; i<2; i++)
        toku_pthread_join(t[i], &ret);
    q_destroy(&q);
    return 0;
}

