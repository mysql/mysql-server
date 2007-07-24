#include "cachetable.h"
#include "hashtable.h"
#include "pma.h"
#include "brt.h"
//#include "pma.h"

typedef long long diskoff;  /* Offset in a disk. -1 is the NULL pointer. */

enum { TREE_FANOUT = 16 }; //, NODESIZE=1<<20 };
enum { KEY_VALUE_OVERHEAD = 8 }; /* Must store the two lengths. */
struct nodeheader_in_file {
    int n_in_buffer;
    
};
enum { BUFFER_HEADER_SIZE = (4 // height//
			     + 4 // n_children
			     + TREE_FANOUT * 8 // children
			     ) };
typedef struct brtnode *BRTNODE;
/* Internal nodes. */
struct brtnode {
    enum typ_tag tag;
    unsigned int nodesize;
    diskoff thisnodename;
    int    height; /* height is always >= 0.  0 for leaf, >0 for nonleaf. */
    union node {
	struct nonleaf {
	    int             n_children;  /* if n_children==TREE_FANOUT+1 then the tree needs to be rebalanced. */
	    bytevec         childkeys[TREE_FANOUT];   /* Pivot keys.  Child 0's keys are <= childkeys[0].  Child 1's keys are <= childkeys[1].
							 Note: It is possible that Child 1's keys are == to child 0's key's, so it is
							 not necessarily true that child 1's keys are > childkeys[0].
						         However, in the absense of duplicate keys, child 1's keys *are* > childkeys[0]. */
	    unsigned int    childkeylens[TREE_FANOUT];
	    unsigned int    totalchildkeylens;
	    diskoff         children[TREE_FANOUT+1];  /* unused if height==0 */   /* Note: The last element of these arrays is used only temporarily while splitting a node. */
	    HASHTABLE       htables[TREE_FANOUT+1];
	    unsigned int    n_bytes_in_hashtable[TREE_FANOUT+1]; /* how many bytes are in each hashtable (including overheads) */
	    unsigned int    n_bytes_in_hashtables;
	} n;
	struct leaf {
	    PMA buffer;
	    unsigned int n_bytes_in_buffer;
	} l;
    } u;
};

struct brt_header {
    int dirty;
    unsigned int nodesize;
    diskoff freelist;
    diskoff unused_memory;
    diskoff unnamed_root;
    int n_named_roots; /* -1 if the only one is unnamed */
    char  **names;
    diskoff *roots;
};


struct brt {
    CACHEFILE cf;
    char *database_name;
    // The header is shared.  It is also ephemeral.
    struct brt_header *h;

    BRT_CURSOR cursors_head, cursors_tail;

    int (*compare_fun)(DB*,const DBT*,const DBT*);

    void *skey,*sval; /* Used for DBT return values. */
};

/* serialization code */
void serialize_brtnode_to(int fd, diskoff off, diskoff size, BRTNODE node);
int deserialize_brtnode_from (int fd, diskoff off, BRTNODE *brtnode, int nodesize);
unsigned int serialize_brtnode_size(BRTNODE node); /* How much space will it take? */
int keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len);

void verify_counts(BRTNODE);

int serialize_brt_header_to (int fd, struct brt_header *h);
int deserialize_brtheader_from (int fd, diskoff off, struct brt_header **brth);

//static inline int brtnode_n_hashtables(BRTNODE node) { if (node->height==0) return 1; else return node->u.n.n_children; }

//int write_brt_header (int fd, struct brt_header *header);

#if 1
#define DEADBEEF ((void*)0xDEADBEEF)
#else
#define DEADBEEF ((void*)0xDEADBEEFDEADBEEF)
#endif

