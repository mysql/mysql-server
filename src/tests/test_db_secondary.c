/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <string.h>
#include <db.h>
#include <assert.h>
#include <errno.h>
/*#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>*/

#define CKERR(r) \
do {                                                                                                        \
    if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);  \
} while (0)

// DIR is defined in the Makefile

int dbtcmp(DBT *dbt1, DBT *dbt2) {
    int r;
    
    r = dbt1->size - dbt2->size;  if (r) return r;
    return memcmp(dbt1->data, dbt2->data, dbt1->size);
}

struct student_record {
	char student_id[4];
	char last_name[15];
	char first_name[15];
};
#define SPACES "               "
DB *dbp;
DB *sdbp;
DB_TXN *const null_txn = 0;
DB_ENV *dbenv;
/*
 * getname -- extracts a secondary key (the last name) from a primary
 * 	key/data pair
 */
int getname(DB *secondary, const DBT *pkey, const DBT *pdata, DBT *skey)
{
	/*
	 * Since the secondary key is a simple structure member of the
	 * record, we don't have to do anything fancy to return it.  If
	 * we have composite keys that need to be constructed from the
	 * record, rather than simply pointing into it, then the user's
	 * function might need to allocate space and copy data.  In
	 * this case, the DB_DBT_APPMALLOC flag should be set in the
	 * secondary key DBT.
	 */
	memset(skey, 0, sizeof(DBT));
	skey->data = ((struct student_record *)pdata->data)->last_name;
	skey->size = sizeof ((struct student_record *)pdata->data)->last_name;
	return (0);
}

void second_setup() {
    int r;

    /* Open/create primary */
    r = db_create(&dbp, dbenv, 0);                                              assert(r == 0);
    r = dbp->open(dbp, NULL, "students.db", NULL, DB_BTREE, DB_CREATE, 0600);   assert(r == 0);

    /*
     * Open/create secondary. Note that it supports duplicate data
     * items, since last names might not be unique.
     */
    r = db_create(&sdbp, dbenv, 0);                                             assert(r == 0);
    r = sdbp->set_flags(sdbp, DB_DUP | DB_DUPSORT);                             assert(r == 0);
    r = sdbp->open(sdbp, NULL, "lastname.db", NULL, DB_BTREE, DB_CREATE, 0600); assert(r == 0);
    

    /* Associate the secondary with the primary. */
    r = dbp->associate(dbp, NULL, sdbp, getname, 0);                            assert(r == 0);
}

void setup_student(struct student_record *s) {
    memset(s, 0, sizeof(struct student_record));
    memcpy(&s->student_id, "WC42"      SPACES,  sizeof(s->student_id));
    //Padded with enough spaces to fill out last/first name.
    memcpy(&s->last_name,  "Churchill" SPACES,  sizeof(s->last_name));
    memcpy(&s->first_name, "Winston"   SPACES,  sizeof(s->first_name));
}

void insert_test() {
    struct student_record s;
    DBT data;
    DBT key;
    DBT skey;
    DBT testdata;
    DBT testkey;
    int r;
    
    setup_student(&s);
    memset(&testdata,   0, sizeof(DBT));
    memset(&testkey,    0, sizeof(DBT));
    memset(&skey,       0, sizeof(DBT));
    memset(&key,        0, sizeof(DBT));
    memset(&data,       0, sizeof(DBT));
    key.data = "WC42";
    key.size = strlen("WC42");
    data.data = &s;
    data.size = sizeof(s);
    //Set up secondary key
    r = getname(sdbp, &key, &data, &skey);              assert(r == 0);
        
    r = dbp->put(dbp, null_txn, &key, &data, 0);        assert(r == 0);
    

    /* Try to get it from primary. */
    r = dbp->get(dbp, null_txn, &key, &testdata, 0);    assert(r == 0);
    r = dbtcmp(&data, &testdata);                       assert(r == 0);
    
    /* Try to get it from secondary. */
    r = sdbp->get(sdbp, null_txn, &skey, &testdata, 0); assert(r == 0);
    r = dbtcmp(&data, &testdata);                       assert(r == 0);

    /* Try to pget from secondary */ 
    r = sdbp->pget(sdbp, null_txn, &skey, &testkey, &testdata, 0);  assert(r == 0);
    r = dbtcmp(&data, &testdata);                       assert(r == 0);
    r = dbtcmp(&testkey, &key);                         assert(r == 0);
    
    /* Make sure we fail 'pget' from primary */
    r = dbp->pget(dbp, null_txn, &key, &testkey, &data, 0);         assert(r == EINVAL);
}

void delete_from_primary() {
    int r;
    DBT key;

    memset(&key, 0, sizeof(DBT));
    key.data = "WC42";
    key.size = 4;
    r = dbp->del(dbp, null_txn, &key, 0);               assert(r == 0);
}

void delete_from_secondary() {
    int r;
    DBT skey;
    DBT data;
    struct student_record s;

    setup_student(&s);
    memset(&skey, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));

    data.data = &s;
    data.size = sizeof(s);
    r = getname(sdbp, NULL, &data, &skey);              assert(r == 0);
    r = sdbp->del(sdbp, null_txn, &skey, 0);            assert(r == 0);
}

void verify_gone() {
    int r;
    DBT key;
    DBT skey;
    DBT data;
    struct student_record s;

    memset(&key, 0, sizeof(DBT));
    memset(&skey, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));
    key.data = "WC42";
    key.size = 4;

    /* Try (fail) to get it from primary. */
    r = dbp->get(dbp, null_txn, &key, &data, 0);        assert(r == DB_NOTFOUND);

    /* Try (fail) to get it from secondary. */
    setup_student(&s);
    r = getname(sdbp, NULL, &data, &skey);              assert(r == 0);
    r = sdbp->get(sdbp, null_txn, &skey, &data, 0);     assert(r == DB_NOTFOUND);

    /* Try (fail) to pget from secondary */ 
    setup_student(&s);
    r = getname(sdbp, NULL, &data, &skey);              assert(r == 0);
    r = sdbp->pget(sdbp, null_txn, &skey, &key, &data, 0);assert(r == DB_NOTFOUND);
}

int main() {
    int r;

    second_setup();
    insert_test();
    delete_from_primary();
    verify_gone();
    insert_test();
    delete_from_secondary();
    verify_gone();
    
    r = dbp->close(dbp, 0);                             assert(r == 0);
    r = sdbp->close(sdbp, 0);                           assert(r == 0);
    return 0;
}
