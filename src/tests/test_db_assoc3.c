/* Primary with two associated things. */

#include <string.h>
#include <db.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "test.h"


/* Primary is a map from a UID which consists of a random number followed by the current time. */

struct timestamp {
    unsigned int tv_sec; /* in newtork order */
    unsigned int tv_usec; /* in network order */
};


struct primary_key {
    int rand; /* in network order */
    struct timestamp ts;
};
struct primary_data {
    struct timestamp creationtime;
    struct timestamp expiretime; /* not valid if doesexpire==0 */
    char doesexpire;
    char namelen;
    char name[0];
};

struct name_key {
    char namelen;
    char name[0];
};

int name_callback (DB *secondary __attribute__((__unused__)), const DBT *key, const DBT *data, DBT *result) {
    struct primary_data *d = data->data;
    result->flags=0;
    result->size=1+d->namelen;
    result->data=&d->name[0];
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

// The expire_key is simply a timestamp.

DB_ENV *dbenv;
DB *dbp,*namedb,*expiredb;

DB_TXN * const null_txn=0;

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
}

void close_databases (void) {
    int r;
    r = namedb->close(namedb, 0);   CKERR(r);
    r = dbp->close(dbp, 0);      CKERR(r);
    r = expiredb->close(expiredb, 0); CKERR(r);
    r = dbenv->close(dbenv, 0);  CKERR(r);
}
    

void gettod (struct timestamp *ts) {
    struct timeval tv;
    int r = gettimeofday(&tv, 0);
    assert(r==0);
    ts->tv_sec  = htonl(tv.tv_sec);
    ts->tv_usec = htonl(tv.tv_usec);
}

void insert_person (void) {
    int namelen = 5+random()%245;
    struct primary_key  pk;
    struct primary_data *pd=malloc(namelen+sizeof(*pd));;
    pk.rand = random();
    gettod(&pk.ts);
    pd->creationtime = pk.ts;
    pd->expiretime   = pk.ts;
    pd->expiretime.tv_sec += 24*60*60*366;
    pd->doesexpire = (random()%1==0);
    pd->namelen = namelen;
    int i;
    pd->name[0] = 'A'+random()%26;
    for (i=1; i<namelen; i++) {
	pd->name[i] = 'a'+random()%26;
    }
    DBT key,data;
    memset(&key,0,sizeof(DBT));
    memset(&data,0,sizeof(DBT));
    key.size = sizeof(pk);
    key.data = &pk;
    data.size = namelen+sizeof(*pd);
    data.data = pd;
    int r=dbp->put(dbp, null_txn, &key, &data,0);  assert(r==0);
    free(pd);
}

int main () {
    system("rm -rf " DIR);
    mkdir(DIR, 0777); 

    create_databases();

//    insert_person();

    close_databases();

    return 0;
}

