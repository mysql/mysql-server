/* -*- mode: C; c-basic-offset: 4 -*- */
#include "block_allocator.h"
#include "toku_assert.h"

#include <stdlib.h>

static void ba_alloc_at (BLOCK_ALLOCATOR ba, u_int64_t size, u_int64_t offset) {
    block_allocator_validate(ba);
    block_allocator_alloc_block_at(ba, size, offset);
    block_allocator_validate(ba);
}

static void ba_alloc (BLOCK_ALLOCATOR ba, u_int64_t size, u_int64_t *answer) {
    block_allocator_validate(ba);
    block_allocator_alloc_block(ba, size, answer);
    block_allocator_validate(ba);
}

static void ba_free (BLOCK_ALLOCATOR ba, u_int64_t offset) {
    block_allocator_validate(ba);
    block_allocator_free_block(ba, offset);
    block_allocator_validate(ba);
}

// Simple block allocator test
static void
test_ba0 (void) {
    BLOCK_ALLOCATOR ba;
    u_int64_t b0, b1;
    create_block_allocator(&ba, 100);
    ba_alloc_at(ba, 50, 100);
    ba_alloc_at(ba, 25, 150);
    ba_alloc   (ba, 10, &b0);
    assert(b0==175);
    ba_free(ba, 150);
    ba_alloc_at(ba, 10, 150);
    ba_alloc(ba, 10, &b0);
    assert(b0==160);
    ba_alloc(ba, 10, &b0);
    ba_alloc(ba, 113, &b1);
    assert(113==block_allocator_block_size(ba, b1));
    assert(10==block_allocator_block_size(ba, b0));
    assert(50==block_allocator_block_size(ba, 100));

    u_int64_t b2, b3, b4, b5, b6, b7;
    ba_alloc(ba, 100, &b2);     
    ba_alloc(ba, 100, &b3);     
    ba_alloc(ba, 100, &b4);     
    ba_alloc(ba, 100, &b5);     
    ba_alloc(ba, 100, &b6);     
    ba_alloc(ba, 100, &b7);     
    ba_free(ba, b2);
    ba_alloc(ba, 100, &b2);  
    ba_free(ba, b4);         
    ba_free(ba, b6);         
    u_int64_t b8, b9;
    ba_alloc(ba, 100, &b4);    
    ba_free(ba, b2);           
    ba_alloc(ba, 100, &b6);    
    ba_alloc(ba, 100, &b8);    
    ba_alloc(ba, 100, &b9);    
    ba_free(ba, b6);           
    ba_free(ba, b7);           
    ba_free(ba, b8);           
    ba_alloc(ba, 100, &b6);    
    ba_alloc(ba, 100, &b7);    
    ba_free(ba, b4);           
    ba_alloc(ba, 100, &b4);    

    destroy_block_allocator(&ba);
    assert(ba==0);
}

// Manually to get coverage of all the code in the block allocator.
static void
test_ba1 (int n_initial) {
    BLOCK_ALLOCATOR ba;
    create_block_allocator(&ba, 0);
    int i;
    int n_blocks=0;
    u_int64_t blocks[1000];
    for (i=0; i<1000; i++) {
	if (i<n_initial || random()%2 == 0) {
	    if (n_blocks<1000) {
		ba_alloc(ba, 1, &blocks[n_blocks]);
		//printf("A[%d]=%ld\n", n_blocks, blocks[n_blocks]);
		n_blocks++;
	    } 
	} else {
	    if (n_blocks>0) {
		int blocknum = random()%n_blocks;
		//printf("F[%d]%ld\n", blocknum, blocks[blocknum]);
		ba_free(ba, blocks[blocknum]);
		blocks[blocknum]=blocks[n_blocks-1];
		n_blocks--;
	    }
	}
    }
    
    destroy_block_allocator(&ba);
    assert(ba==0);
}
    

int
main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    test_ba0();
    test_ba1(0);
    test_ba1(10);
    test_ba1(20);
    return 0;
}
