/* merger test.
 * Test 0:   A super-simple merge of one file with a few records.
 * Test 1:   A fairly simple merge of a few hundred records in a few files, the data in the files is interleaved randomly (that is the "pop" operation will tend to get data from a randomly chosen file.
 * Test 2:   A merge of a thousand files, (interleaved randomly), each file perhaps a megabyte.   This test is intended to demonstrate performance improvements when we have a proper merge heap.
 * Test 3:   A merge of 10 files, (interleaved randomly) each file perhaps a gigabyte.  This test is intended to demonstrate performance improvements when we pipeline the reads properly.
 * Test 4:   A merge of 100 files, presorted, each perhaps a 10MB.  All the records in the first file are less than all the records in the second.   This test is intended to show performance improvements when we prefer to refill the buffer that is the most empty.
 */

#include "toku_assert.h"
#include "merger.h"
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>

static int default_compare (DB *db __attribute__((__unused__)), const DBT *keya, const DBT *keyb) {
    if (keya->size < keyb->size) {
	int c = memcmp(keya->data, keyb->data, keya->size);
	if (c==0) return -1; // tie breaker is the length, a is shorter so it's first.
	else return c;
    } else if (keya->size > keyb->size) {
	int c = memcmp(keya->data, keyb->data, keyb->size);
	if (c==0) return +1; // tie breaker is the length, a is longer so it's second
	else return c;
    } else {
	return memcmp(keya->data, keyb->data, keya->size);
    }
}

static void write_ij (FILE *f, u_int32_t i, u_int32_t j) {
    u_int32_t len=sizeof(i);
    {   int r = fwrite(&len, sizeof(len), 1, f);  assert(r==1); }
    i = htonl(i);
    {   int r = fwrite(&i,   sizeof(i),   1, f);  assert(r==1); }
    {   int r = fwrite(&len, sizeof(len), 1, f);  assert(r==1); }
    j = htonl(j);
    {   int r = fwrite(&j,   sizeof(j),   1, f);  assert(r==1); }
}

static void check_ij (MERGER m, u_int32_t i, u_int32_t j) {
    DBT key,val;
    memset(&key, 0, sizeof(key));
    memset(&val, 0, sizeof(val));
    int r = merger_pop(m, &key, &val);
    assert(r==0);
    assert(key.size = 4);
    assert(val.size = 4);
    u_int32_t goti = ntohl(*(u_int32_t*)key.data);
    u_int32_t gotj = ntohl(*(u_int32_t*)val.data);
    //printf("Got %d,%d (expect %d,%d)\n", goti, gotj, i, j);
    assert(goti == i);
    assert(gotj == j);
}

static void check_nothing_is_left (MERGER m) {
    DBT key,val;
    memset(&key, 0, sizeof(key));
    memset(&val, 0, sizeof(val));
    int r = merger_pop(m, &key, &val);
    assert(r!=0); // should be nothing left.
}

static void test0 (void) {
    char *fname = "merger-test0.data";
    char *fnames[] = {fname};
    FILE *f = fopen(fname, "w");
    assert(f);
    const u_int32_t N = 100;
    for (u_int32_t i=0; i<N; i++) {
	write_ij(f, i, N-i);
    }
    { int r = fclose(f);  assert(r==0); }
    MERGER m = create_merger (1, fnames, NULL, default_compare, NULL);
    for (u_int32_t i=0; i<N; i++) {
	check_ij(m, i, N-i);
    }
    check_nothing_is_left(m);
    merger_close(m);
    { int r  = unlink(fname); assert(r==0); }
}

static void test1 (void) {
    const int       NFILES = 10;
    const u_int32_t NRECORDS = NFILES*100;
    char *fnames[NFILES];
    {
	FILE *files[NFILES];
	for (int i=0; i<NFILES; i++) {
	    char fname[] = "merger-test-XXX.data";
	    snprintf(fname, sizeof(fname), "merger-test-%3x.data", i);
	    fnames[i] = strdup(fname);
	    files [i] = fopen(fname, "w");
	}
	for (u_int32_t i=0; i<NRECORDS; i++) {
	    int fnum = random()%NFILES;
	    write_ij(files[fnum], i, 2*i);
	}
	for (int i=0; i<NFILES; i++) {
	    int r = fclose(files[i]); assert(r==0);
	}
    }
    MERGER m = create_merger(NFILES, fnames, NULL, default_compare, NULL);
    for (u_int32_t i=0; i<NRECORDS; i++) {
	check_ij(m, i, 2*i);
    }
    check_nothing_is_left(m);
    merger_close(m);
    for (int i=0; i<NFILES; i++) {
	free(fnames[i]);
    }
}

int main (int argc, char *argv[] __attribute__((__unused__))) {
    assert(argc==1);
    test0();
    test1();
    return 0;
}
 
