#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "memory.h"
#include "fifo.h"

static void fifo_init(struct fifo *fifo) {
    fifo->head = fifo->tail = 0;
    fifo->n = 0;
}

static struct fifo_entry *fifo_peek(struct fifo *fifo) {
    return fifo->head;
}

static void fifo_enq(struct fifo *fifo, struct fifo_entry *entry) {
    entry->next = 0;
    if (fifo->head == 0)
        fifo->head = entry;
    else
        fifo->tail->next = entry;
    fifo->tail = entry;
    fifo->n += 1;
}

static struct fifo_entry *fifo_deq(struct fifo *fifo) {
    struct fifo_entry *entry = fifo->head;
    if (entry) {
        fifo->head = entry->next;
        if (fifo->head == 0)
            fifo->tail = 0;
        fifo->n -= 1;
        assert(fifo->n >= 0);
    }
    return entry;
}

static void fifo_destroy(struct fifo *fifo) {
    struct fifo_entry *entry;
    while ((entry = fifo_deq(fifo)) != 0)
        toku_free(entry);
}

int toku_fifo_create(FIFO *ptr) {
    struct fifo *fifo = toku_malloc(sizeof (struct fifo));
    if (fifo == 0) return ENOMEM;
    fifo_init(fifo);
    *ptr = fifo;
    return 0;
}

void toku_fifo_free(FIFO *ptr) {
    struct fifo *fifo = *ptr; *ptr = 0;
    fifo_destroy(fifo);
    toku_free(fifo);
}

int toku_fifo_n_entries(FIFO fifo) {
    return fifo->n;
}

int toku_fifo_enq(FIFO fifo, const void *key, unsigned int keylen, const void *data, unsigned int datalen, int type) {
    struct fifo_entry *entry = toku_malloc(sizeof (struct fifo_entry) + keylen + datalen);
    if (entry == 0) return ENOMEM;
    entry->type = type;
    entry->keylen = keylen;
    memcpy(entry->key, key, keylen);
    entry->vallen = datalen;
    memcpy(entry->key + keylen, data, datalen);
    fifo_enq(fifo, entry);
    return 0;
}

/* peek at the head of the fifo */
int toku_fifo_peek(FIFO fifo, bytevec *key, unsigned int *keylen, bytevec *data, unsigned int *datalen, int *type) {
    struct fifo_entry *entry = fifo_peek(fifo);
    if (entry == 0) return -1;
    *key = entry->key;
    *keylen = entry->keylen;
    *data = entry->key + entry->keylen;
    *datalen = entry->vallen;
    *type = entry->type;
    return 0;
}

int toku_fifo_deq(FIFO fifo) {
    struct fifo_entry *entry = fifo_deq(fifo);
    if (entry == 0) return ENOMEM;
    toku_free(entry);
    return 0;
}

void toku_fifo_iterate (FIFO fifo, void(*f)(bytevec key,ITEMLEN keylen,bytevec data,ITEMLEN datalen,int type, void*), void *arg) {
    struct fifo_entry *entry;
    for (entry = fifo_peek(fifo); entry; entry = entry->next)
        f(entry->key, entry->keylen, entry->key + entry->keylen, entry->vallen, entry->type, arg);
}




    
