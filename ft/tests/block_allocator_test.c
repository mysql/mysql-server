/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."


#include "test.h"
#include "includes.h"

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

static void
ba_check_l (BLOCK_ALLOCATOR ba, u_int64_t blocknum_in_layout_order, u_int64_t expected_offset, u_int64_t expected_size)
{
    u_int64_t actual_offset, actual_size;
    int r = block_allocator_get_nth_block_in_layout_order(ba, blocknum_in_layout_order, &actual_offset, &actual_size);
    assert(r==0);
    assert(expected_offset == actual_offset);
    assert(expected_size   == actual_size);
}

static void
ba_check_none (BLOCK_ALLOCATOR ba, u_int64_t blocknum_in_layout_order)
{
    u_int64_t actual_offset, actual_size;
    int r = block_allocator_get_nth_block_in_layout_order(ba, blocknum_in_layout_order, &actual_offset, &actual_size);
    assert(r==-1);
}


// Simple block allocator test
static void
test_ba0 (void) {
    BLOCK_ALLOCATOR ba;
    u_int64_t b0, b1;
    create_block_allocator(&ba, 100, 1);
    assert(block_allocator_allocated_limit(ba)==100);
    ba_alloc_at(ba, 50, 100);
    assert(block_allocator_allocated_limit(ba)==150);
    ba_alloc_at(ba, 25, 150);
    ba_alloc   (ba, 10, &b0);
    ba_check_l (ba, 0, 0,   100);
    ba_check_l (ba, 1, 100,  50);
    ba_check_l (ba, 2, 150,  25);
    ba_check_l (ba, 3, b0,  10);
    ba_check_none (ba, 4);
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
    create_block_allocator(&ba, 0, 1);
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
    
// Check to see if it is first fit or best fit.
static void
test_ba2 (void)
{
    BLOCK_ALLOCATOR ba;
    u_int64_t b[6];
    enum { BSIZE = 1024 };
    create_block_allocator(&ba, 100, BSIZE);
    assert(block_allocator_allocated_limit(ba)==100);
    ba_check_l    (ba, 0, 0, 100);
    ba_check_none (ba, 1);

    ba_alloc (ba, 100, &b[0]);
    ba_check_l    (ba, 0, 0, 100);
    ba_check_l    (ba, 1, BSIZE, 100);
    ba_check_none (ba, 2);

    ba_alloc (ba, BSIZE+100, &b[1]);
    ba_check_l    (ba, 0, 0, 100);
    ba_check_l    (ba, 1,   BSIZE,       100);
    ba_check_l    (ba, 2, 2*BSIZE, BSIZE+100);
    ba_check_none (ba, 3);

    ba_alloc (ba, 100, &b[2]);
    ba_check_l    (ba, 0, 0, 100);
    ba_check_l    (ba, 1,   BSIZE,       100);
    ba_check_l    (ba, 2, 2*BSIZE, BSIZE+100);
    ba_check_l    (ba, 3, 4*BSIZE,       100);
    ba_check_none (ba, 4);

    ba_alloc (ba, 100, &b[3]);
    ba_alloc (ba, 100, &b[4]);
    ba_alloc (ba, 100, &b[5]);
    ba_check_l    (ba, 0, 0, 100);
    ba_check_l    (ba, 1,   BSIZE,       100);
    ba_check_l    (ba, 2, 2*BSIZE, BSIZE+100);
    ba_check_l    (ba, 3, 4*BSIZE,       100);
    ba_check_l    (ba, 4, 5*BSIZE,       100);
    ba_check_l    (ba, 5, 6*BSIZE,       100);
    ba_check_l    (ba, 6, 7*BSIZE,       100);
    ba_check_none (ba, 7);
   
    ba_free (ba, 4*BSIZE);
    ba_check_l    (ba, 0, 0, 100);
    ba_check_l    (ba, 1,   BSIZE,       100);
    ba_check_l    (ba, 2, 2*BSIZE, BSIZE+100);
    ba_check_l    (ba, 3, 5*BSIZE,       100);
    ba_check_l    (ba, 4, 6*BSIZE,       100);
    ba_check_l    (ba, 5, 7*BSIZE,       100);
    ba_check_none (ba, 6);

    u_int64_t b2;
    ba_alloc(ba, 100, &b2);
    assert(b2==4*BSIZE);
    ba_check_l    (ba, 0, 0, 100);
    ba_check_l    (ba, 1,   BSIZE,       100);
    ba_check_l    (ba, 2, 2*BSIZE, BSIZE+100);
    ba_check_l    (ba, 3, 4*BSIZE,       100);
    ba_check_l    (ba, 4, 5*BSIZE,       100);
    ba_check_l    (ba, 5, 6*BSIZE,       100);
    ba_check_l    (ba, 6, 7*BSIZE,       100);
    ba_check_none (ba, 7);

    ba_free (ba,   BSIZE);
    ba_free (ba, 5*BSIZE);
    ba_check_l    (ba, 0, 0, 100);
    ba_check_l    (ba, 1, 2*BSIZE, BSIZE+100);
    ba_check_l    (ba, 2, 4*BSIZE,       100);
    ba_check_l    (ba, 3, 6*BSIZE,       100);
    ba_check_l    (ba, 4, 7*BSIZE,       100);
    ba_check_none (ba, 5);

    // This alloc will allocate the first block after the reserve space in the case of first fit.
    u_int64_t b3;
    ba_alloc(ba, 100, &b3);
    assert(b3==  BSIZE);      // First fit.
    // if (b3==5*BSIZE) then it is next fit.

    // Now 5*BSIZE is free
    u_int64_t b5;
    ba_alloc(ba, 100, &b5);
    assert(b5==5*BSIZE);
    ba_check_l    (ba, 0, 0, 100);
    ba_check_l    (ba, 1,   BSIZE,       100);
    ba_check_l    (ba, 2, 2*BSIZE, BSIZE+100);
    ba_check_l    (ba, 3, 4*BSIZE,       100);
    ba_check_l    (ba, 4, 5*BSIZE,       100);
    ba_check_l    (ba, 5, 6*BSIZE,       100);
    ba_check_l    (ba, 6, 7*BSIZE,       100);
    ba_check_none (ba, 7);

    // Now all blocks are busy
    u_int64_t b6, b7, b8;
    ba_alloc(ba, 100, &b6);
    ba_alloc(ba, 100, &b7);
    ba_alloc(ba, 100, &b8);
    assert(b6==8*BSIZE);
    assert(b7==9*BSIZE);
    assert(b8==10*BSIZE);
    ba_check_l    (ba, 0, 0, 100);
    ba_check_l    (ba, 1,   BSIZE,       100);
    ba_check_l    (ba, 2, 2*BSIZE, BSIZE+100);
    ba_check_l    (ba, 3, 4*BSIZE,       100);
    ba_check_l    (ba, 4, 5*BSIZE,       100);
    ba_check_l    (ba, 5, 6*BSIZE,       100);
    ba_check_l    (ba, 6, 7*BSIZE,       100);
    ba_check_l    (ba, 7, 8*BSIZE,       100);
    ba_check_l    (ba, 8, 9*BSIZE,       100);
    ba_check_l    (ba, 9, 10*BSIZE,       100);
    ba_check_none (ba, 10);
    
    ba_free(ba, 9*BSIZE);
    ba_free(ba, 7*BSIZE);
    u_int64_t b9;
    ba_alloc(ba, 100, &b9);
    assert(b9==7*BSIZE);

    ba_free(ba, 5*BSIZE);
    ba_free(ba, 2*BSIZE);
    u_int64_t b10, b11;
    ba_alloc(ba, 100, &b10);
    assert(b10==2*BSIZE);
    ba_alloc(ba, 100, &b11);
    assert(b11==3*BSIZE);
    ba_alloc(ba, 100, &b11);
    assert(b11==5*BSIZE);

    destroy_block_allocator(&ba);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    test_ba0();
    test_ba1(0);
    test_ba1(10);
    test_ba1(20);
    test_ba2();
    return 0;
}
