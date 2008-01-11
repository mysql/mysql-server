#include "brttypes.h"

struct fifo_entry {
    struct fifo_entry *next;
    unsigned int keylen;
    unsigned int vallen;
    unsigned char type;
    unsigned char key[];
};

struct fifo {
    struct fifo_entry *head, *tail;
    int n;
};

typedef struct fifo *FIFO;

int toku_fifo_create(FIFO *);
void toku_fifo_free(FIFO *);
int toku_fifo_n_entries(FIFO);
int toku_fifo_enq (FIFO, const void *key, ITEMLEN keylen, const void *data, ITEMLEN datalen, int type);
int toku_fifo_peek (FIFO, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen, int *type);
int toku_fifo_deq(FIFO);
int toku_fifo_peek_deq (FIFO, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen, int *type);
void toku_fifo_iterate (FIFO, void(*f)(bytevec key,ITEMLEN keylen,bytevec data,ITEMLEN datalen,int type, void*), void*);

#define FIFO_ITERATE(fifo,keyvar,keylenvar,datavar,datalenvar,typevar,body) ({ \
            struct fifo_entry *entry; \
            for (entry = fifo->head; entry; entry = entry->next) { \
                unsigned int keylenvar = entry->keylen; \
                void *keyvar = entry->key; \
                unsigned int datalenvar = entry->vallen; \
                void *datavar = entry->key + entry->keylen; \
                unsigned int typevar = entry->type; \
                body; \
            } \
        })

