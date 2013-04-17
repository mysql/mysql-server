#include <toku_portability.h>
#include <string.h>

#include "test.h"
#include "brttypes.h"
#include "includes.h"

int test_0 ();

int test_main(int argc __attribute__((unused)), const char *argv[] __attribute__((unused))) {
    int r = 0;

    r = test_0();

    return r
}

int test_0 () {
    int r=0;
    char dbdir[100] = "./dir.test_logcursor.tdb";
    struct toku_logcursor *cursor;
    struct log_entry *entry;

    r = toku_logcursor_create(&cursor, dbdir);
    if ( r!=0 ) return r;
    r = toku_logcursor_next(cursor, &entry);    printf("Entry = %c\n", entry->cmd);
    r = toku_logcursor_next(cursor, &entry);    printf("Entry = %c\n", entry->cmd);
    r = toku_logcursor_next(cursor, &entry);    printf("Entry = %c\n", entry->cmd);
    printf("r=%d\n", r);
    r = toku_logcursor_destroy(&cursor);
    
    r = toku_logcursor_create(&cursor, dbdir);
    if ( r!=0 ) return r;
    r = toku_logcursor_prev(cursor, &entry);    printf("Entry = %c\n", entry->cmd);
    r = toku_logcursor_prev(cursor, &entry);    printf("Entry = %c\n", entry->cmd);
    r = toku_logcursor_prev(cursor, &entry);    printf("Entry = %c\n", entry->cmd);
    printf("r=%d\n", r);
    r = toku_logcursor_destroy(&cursor);

    r = toku_logcursor_create(&cursor, dbdir);
    if ( r!=0 ) return r;
    r = toku_logcursor_next(cursor, &entry);    printf("Entry = %c\n", entry->cmd);
    r = toku_logcursor_next(cursor, &entry);    printf("Entry = %c\n", entry->cmd);
    r = toku_logcursor_next(cursor, &entry);    printf("Entry = %c\n", entry->cmd);
    r = toku_logcursor_prev(cursor, &entry);    printf("Entry = %c\n", entry->cmd);
    r = toku_logcursor_prev(cursor, &entry);    printf("Entry = %c\n", entry->cmd);
    r = toku_logcursor_prev(cursor, &entry);    printf("Entry = %c\n", entry->cmd);
    r = toku_logcursor_prev(cursor, &entry);    printf("Entry = %c\n", entry->cmd);
    if ( r == DB_NOTFOUND ) printf("PASS\n"); else printf("FAIL\n");

    r = toku_logcursor_destroy(&cursor);

    return 0;
}

