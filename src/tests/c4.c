#include <assert.h>
#include <string.h>
#include <db.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "test.h"

DB_ENV *dbenv;
DBC *delete_cursor, *name_cursor, *dbc;
DB  *dbp, *expiredb, *namedb;
DBT  key, pkey, data;

struct timestamp {
    unsigned int tv_sec; /* in newtork order */
    unsigned int tv_usec; /* in network order */
};

struct primary_key {
    int rand; /* in network order */
    struct timestamp ts;
};

struct name_key {
    unsigned char* name;
};

struct primary_data {
    struct timestamp creationtime;
    struct timestamp expiretime; /* not valid if doesexpire==0 */
    unsigned char doesexpire;
    struct name_key name;
};

static void free_pd (struct primary_data *pd) {
    free(pd->name.name);
    free(pd);
}

static void write_uchar_to_dbt (DBT *dbt, const unsigned char c) {
    assert(dbt->size+1 <= dbt->ulen);
    ((char*)dbt->data)[dbt->size++]=c;
}

static void write_name_to_dbt (DBT *dbt, const struct name_key *nk) {
    int i;
    for (i=0; 1; i++) {
	write_uchar_to_dbt(dbt, nk->name[i]);
	if (nk->name[i]==0) break;
    }
}

static void read_uchar_from_dbt (const DBT *dbt, int *off, unsigned char *uchar) {
    assert(*off < dbt->size);
    *uchar = ((unsigned char *)dbt->data)[(*off)++];
}

static void read_uint_from_dbt (const DBT *dbt, int *off, unsigned int *uint) {
    unsigned char a,b,c,d;
    read_uchar_from_dbt(dbt, off, &a);
    read_uchar_from_dbt(dbt, off, &b);
    read_uchar_from_dbt(dbt, off, &c);
    read_uchar_from_dbt(dbt, off, &d);
    *uint = (a<<24)+(b<<16)+(c<<8)+d;
}

static void read_timestamp_from_dbt (const DBT *dbt, int *off, struct timestamp *ts) {
    read_uint_from_dbt(dbt, off, &ts->tv_sec);
    read_uint_from_dbt(dbt, off, &ts->tv_usec);
}

static void read_name_from_dbt (const DBT *dbt, int *off, struct name_key *nk) {
    unsigned char buf[1000];
    int i;
    for (i=0; 1; i++) {
	read_uchar_from_dbt(dbt, off, &buf[i]);
	if (buf[i]==0) break;
    }
    nk->name=(unsigned char*)(strdup((char*)buf));
}

static void read_pd_from_dbt (const DBT *dbt, int *off, struct primary_data *pd) {
    read_timestamp_from_dbt(dbt, off, &pd->creationtime);
    read_timestamp_from_dbt(dbt, off, &pd->expiretime);
    read_uchar_from_dbt(dbt, off, &pd->doesexpire);
    read_name_from_dbt(dbt, off, &pd->name);
}

int name_callback (DB *secondary __attribute__((__unused__)), const DBT *key, const DBT *data, DBT *result) {
    struct primary_data *pd = malloc(sizeof(*pd));
    int off=0;
    read_pd_from_dbt(data, &off, pd);
    static int buf[1000];

    result->ulen=1000;
    result->data=buf;
    result->size=0;
    write_name_to_dbt(result,  &pd->name);
    free_pd(pd);
    return 0;
}

int expire_callback (DB *secondary __attribute__((__unused__)), const DBT *key, const DBT *data, DBT *result) {
    struct primary_data *d = data->data;
    if (d->doesexpire) {
	result->flags=0;
	result->size=sizeof(struct timestamp);
	result->data=&d->expiretime;
	return 0;
    } else {
	return DB_DONOTINDEX;
    }
}

DB_TXN *null_txn = 0;

#undef DIR
#ifdef USE_TDB
#define DIR "dir.test_db_curs4.c.tdb"
#else
#define DIR "dir.test_db_curs4.c.bdb"
#endif

void create_databases (void) {
    int r;

    r = db_env_create(&dbenv, 0);                                                            CKERR(r);
    r = dbenv->open(dbenv, DIR, DB_PRIVATE|DB_INIT_MPOOL|DB_CREATE, 0);                      CKERR(r);

    r = db_create(&dbp, dbenv, 0);                                                           CKERR(r);
    r = dbp->open(dbp, null_txn, "primary.db", NULL, DB_BTREE, DB_CREATE, 0600);             CKERR(r);

    r = db_create(&namedb, dbenv, 0);                                                        CKERR(r);
    r = namedb->open(namedb, null_txn, "name.db", NULL, DB_BTREE, DB_CREATE, 0600);          CKERR(r);

    r = db_create(&expiredb, dbenv, 0);                                                      CKERR(r);
    r = expiredb->open(expiredb, null_txn, "expire.db", NULL, DB_BTREE, DB_CREATE, 0600);    CKERR(r);
    
    r = dbp->associate(dbp, NULL, namedb, name_callback, 0);                                 CKERR(r);
    r = dbp->associate(dbp, NULL, expiredb, expire_callback, 0);                             CKERR(r);
    
    r = dbp->cursor(dbp, null_txn, &dbc, 0);                                                 CKERR(r);
    r = namedb->cursor(namedb, null_txn, &name_cursor, 0);                                   CKERR(r);
    r = expiredb->cursor(expiredb, null_txn, &delete_cursor, 0);                             CKERR(r);
}

void close_databases (void) {
    int r;
    r = dbc->c_close(dbc); CKERR(r);
    r = delete_cursor->c_close(delete_cursor); CKERR(r);
    r = name_cursor->c_close(name_cursor); CKERR(r);

    r = namedb->close(namedb, 0);     CKERR(r);
    r = dbp->close(dbp, 0);           CKERR(r);
    r = expiredb->close(expiredb, 0); CKERR(r);

    r = dbenv->close(dbenv, 0);       CKERR(r);
}

static int rcounter;
void rstep (int n) {
    while (rcounter!=n) {
	random();
	rcounter++;
    }
}

void doit_a (void) {
    int r;
#include "c4.c.a"
}

void doit_b (void) {
    int r;
#include "c4.c.b"
}

int main (int argc, char *argv[]) {
    if (argc==1) {
	printf("initing %s\n", DIR);
	system("rm -rf " DIR);
	mkdir(DIR, 0777); 
	create_databases();
	doit_a();
	close_databases();
    } else {
	create_databases();
	doit_b();
	close_databases();
    }
    return 0;
}
