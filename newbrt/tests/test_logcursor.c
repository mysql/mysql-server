#include <toku_portability.h>
#include <string.h>

#include "test.h"
#include "brttypes.h"
#include "includes.h"

int test_0 ();
int test_1 ();
static void usage() {
    printf("test_logcursors [OPTIONS]\n");
    printf("[-v]\n");
    printf("[-q]\n");
}

int test_main(int argc, const char *argv[]) {
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (arg[0] != '-')
            break;
	if (strcmp(arg, "-v")==0) {
	    verbose++;
	} else if (strcmp(arg, "-q")==0) {
	    verbose = 0;
	} else {
	    usage();
	    return 1;
	}
    }

    int r = 0;
    if ( (r=test_0()) !=0 ) return r;
    if ( (r=test_1()) !=0 ) return r;
    return r;
}

int test_0 () {
    int r=0;
    char dbdir[100] = "./dir.test_logcursor";
    struct toku_logcursor *cursor;
    struct log_entry *entry;

    r = toku_logcursor_create(&cursor, dbdir);    if (verbose) printf("create returns %d\n", r);   assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);
    
    r = toku_logcursor_create(&cursor, dbdir);    if (verbose) printf("create returns %d\n", r);   assert(r==0);
    r = toku_logcursor_first(cursor, &entry);     if (verbose) printf("First Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);

    r = toku_logcursor_create(&cursor, dbdir);    if (verbose) printf("create returns %d\n", r);   assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);

    r = toku_logcursor_create(&cursor, dbdir);    if (verbose) printf("create returns %d\n", r);   assert(r==0);
    r = toku_logcursor_last(cursor, &entry);      if (verbose) printf("Last Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);

    r = toku_logcursor_create(&cursor, dbdir);    if (verbose) printf("create returns %d\n", r);   assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      assert(r==DB_NOTFOUND);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);

    r = toku_logcursor_create(&cursor, dbdir);    if (verbose) printf("create returns %d\n", r);   assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      assert(r==DB_NOTFOUND);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);

    r = toku_logcursor_create(&cursor, dbdir);    if (verbose) printf("create returns %d\n", r);   assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); 
    if ( verbose) {
        if ( r == DB_NOTFOUND ) printf("PASS\n"); 
        else printf("FAIL\n"); 
    }
    assert(r==DB_NOTFOUND);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);

    return 0;
}

// test per-file version
int test_1 () {
    int r=0;
    char dbdir[100] = "./dir.test_logcursor";
    char logfile[100] = "log000000000000.tokulog";
    struct toku_logcursor *cursor;
    struct log_entry *entry;

    r = toku_logcursor_create_for_file(&cursor, dbdir, logfile);   
    if (verbose) printf("create returns %d\n", r);   
    assert(r==0);

    r = toku_logcursor_last(cursor, &entry);      
    if (verbose) printf("entry = %c\n", entry->cmd); 
    assert(r==0); 
    assert(entry->cmd =='C');

    r = toku_logcursor_destroy(&cursor);          
    if (verbose) printf("destroy returns %d\n", r);  
    assert(r==0);

    return 0;
}
