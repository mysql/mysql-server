/* Primary with two associated things. */

#include <arpa/inet.h>
#include <assert.h>
#include <db.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "test.h"

enum mode {
    MODE_DEFAULT, MODE_DB_CREATE, MODE_MORE
} mode;



/* Primary is a map from a UID which consists of a random number followed by the current time. */

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

void free_pd (struct primary_data *pd) {
    free(pd->name.name);
    free(pd);
}

void write_uchar_to_dbt (DBT *dbt, const unsigned char c) {
    assert(dbt->size+1 <= dbt->ulen);
    ((char*)dbt->data)[dbt->size++]=c;
}

void write_uint_to_dbt (DBT *dbt, const unsigned int v) {
    write_uchar_to_dbt(dbt, (v>>24)&0xff);
    write_uchar_to_dbt(dbt, (v>>16)&0xff);
    write_uchar_to_dbt(dbt, (v>> 8)&0xff);
    write_uchar_to_dbt(dbt, (v>> 0)&0xff);
}

void write_timestamp_to_dbt (DBT *dbt, const struct timestamp *ts) {
    write_uint_to_dbt(dbt, ts->tv_sec);
    write_uint_to_dbt(dbt, ts->tv_usec);
}

void write_pk_to_dbt (DBT *dbt, const struct primary_key *pk) {
    write_uint_to_dbt(dbt, pk->rand);
    write_timestamp_to_dbt(dbt, &pk->ts);
}

void write_name_to_dbt (DBT *dbt, const struct name_key *nk) {
    int i;
    for (i=0; 1; i++) {
	write_uchar_to_dbt(dbt, nk->name[i]);
	if (nk->name[i]==0) break;
    }
}

void write_pd_to_dbt (DBT *dbt, const struct primary_data *pd) {
    write_timestamp_to_dbt(dbt, &pd->creationtime);
    write_timestamp_to_dbt(dbt, &pd->expiretime);
    write_uchar_to_dbt(dbt, pd->doesexpire);
    write_name_to_dbt(dbt, &pd->name);
}

void read_uchar_from_dbt (const DBT *dbt, int *off, unsigned char *uchar) {
    assert(*off < dbt->size);
    *uchar = ((unsigned char *)dbt->data)[(*off)++];
}

void read_uint_from_dbt (const DBT *dbt, int *off, unsigned int *uint) {
    unsigned char a,b,c,d;
    read_uchar_from_dbt(dbt, off, &a);
    read_uchar_from_dbt(dbt, off, &b);
    read_uchar_from_dbt(dbt, off, &c);
    read_uchar_from_dbt(dbt, off, &d);
    *uint = (a<<24)+(b<<16)+(c<<8)+d;
}

void read_timestamp_from_dbt (const DBT *dbt, int *off, struct timestamp *ts) {
    read_uint_from_dbt(dbt, off, &ts->tv_sec);
    read_uint_from_dbt(dbt, off, &ts->tv_usec);
}

void read_name_from_dbt (const DBT *dbt, int *off, struct name_key *nk) {
    unsigned char buf[1000];
    int i;
    for (i=0; 1; i++) {
	read_uchar_from_dbt(dbt, off, &buf[i]);
	if (buf[i]==0) break;
    }
    nk->name=(unsigned char*)(strdup((char*)buf));
}

void read_pd_from_dbt (const DBT *dbt, int *off, struct primary_data *pd) {
    read_timestamp_from_dbt(dbt, off, &pd->creationtime);
    read_timestamp_from_dbt(dbt, off, &pd->expiretime);
    read_uchar_from_dbt(dbt, off, &pd->doesexpire);
    read_name_from_dbt(dbt, off, &pd->name);
}

int name_offset_in_pd_dbt (void) {
    return 17;
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

// The expire_key is simply a timestamp.

DB_ENV *dbenv;
DB *dbp,*namedb,*expiredb;

DB_TXN * const null_txn=0;

DBC *delete_cursor=0, *name_cursor=0;

// We use a cursor to count the names.
int cursor_count_n_items=0; // The number of items the cursor saw as it scanned over.
int calc_n_items=0;        // The number of items we expect the cursor to acount
int count_all_items=0;      // The total number of items
DBT nc_key,nc_data;


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
    if (delete_cursor) {
	r = delete_cursor->c_close(delete_cursor); CKERR(r);
    }
    if (name_cursor) {
	r = name_cursor->c_close(name_cursor);     CKERR(r);
    }
    if (nc_key.data) free(nc_key.data);
    if (nc_data.data) free(nc_data.data);
    r = namedb->close(namedb, 0);     CKERR(r);
    r = dbp->close(dbp, 0);           CKERR(r);
    r = expiredb->close(expiredb, 0); CKERR(r);
    r = dbenv->close(dbenv, 0);       CKERR(r);
}
    

void gettod (struct timestamp *ts) {
    struct timeval tv;
    int r = gettimeofday(&tv, 0);
    assert(r==0);
    ts->tv_sec  = htonl(tv.tv_sec);
    ts->tv_usec = htonl(tv.tv_usec);
}

void setup_for_db_create (void) {

    // Remove name.db and then rebuild it with associate(... DB_CREATE)

    int r=unlink(DIR "/name.db");
    assert(r==0);

    r = db_env_create(&dbenv, 0);                                                    CKERR(r);
    r = dbenv->open(dbenv, DIR, DB_PRIVATE|DB_INIT_MPOOL, 0);                        CKERR(r);

    r = db_create(&dbp, dbenv, 0);                                                   CKERR(r);
    r = dbp->open(dbp, null_txn, "primary.db", NULL, DB_BTREE, 0, 0600);             CKERR(r);

    r = db_create(&namedb, dbenv, 0);                                                CKERR(r);
    r = namedb->open(namedb, null_txn, "name.db", NULL, DB_BTREE, DB_CREATE, 0600);  CKERR(r);

    r = db_create(&expiredb, dbenv, 0);                                              CKERR(r);
    r = expiredb->open(expiredb, null_txn, "expire.db", NULL, DB_BTREE, 0, 0600);    CKERR(r);
    
    r = dbp->associate(dbp, NULL, expiredb, expire_callback, 0);                     CKERR(r);
    r = dbp->associate(dbp, NULL, namedb, name_callback, DB_CREATE);                 CKERR(r);

}

int count_entries (DB *db) {
    DBC *dbc;
    int r = db->cursor(db, null_txn, &dbc, 0);                                       CKERR(r);
    DBT key,data;
    memset(&key,  0, sizeof(key));    
    memset(&data, 0, sizeof(data));
    int n_found=0;
    for (r = dbc->c_get(dbc, &key, &data, DB_FIRST);
	 r==0;
	 r = dbc->c_get(dbc, &key, &data, DB_NEXT)) {
	n_found++;
    }
    assert(r==DB_NOTFOUND);
    r=dbc->c_close(dbc);                                                             CKERR(r);
    return n_found;
}

void do_create (void) {
    setup_for_db_create();
    // Now check to see if the number of names matches the number of associated things.
    int n_named = count_entries(namedb);
    int n_prim  = count_entries(dbp);
    assert(n_named==n_prim);
}

void insert_person (void) {
    int namelen = 5+random()%245;
    struct primary_key  pk;
    struct primary_data pd;
    char keyarray[1000], dataarray[1000]; 
    unsigned char namearray[1000];
    pk.rand = random();
    gettod(&pk.ts);
    pd.creationtime = pk.ts;
    pd.expiretime   = pk.ts;
    pd.expiretime.tv_sec += 24*60*60*366;
    pd.doesexpire = (random()%10==0);
    int i;
    pd.name.name = namearray;
    pd.name.name[0] = 'A'+random()%26;
    for (i=1; i<namelen; i++) {
	pd.name.name[i] = 'a'+random()%26;
    }
    pd.name.name[i]=0;
    DBT key,data;
    memset(&key,0,sizeof(DBT));
    memset(&data,0,sizeof(DBT));
    key.data = keyarray;
    key.ulen = 1000;
    key.size = 0;
    data.data = dataarray;
    data.ulen = 1000;
    data.size = 0;
    write_pk_to_dbt(&key, &pk);
    write_pd_to_dbt(&data, &pd);
    int r=dbp->put(dbp, null_txn, &key, &data,0);   CKERR(r);
    // If the cursor is to the left of the current item, then increment count_items
    {
	int compare=strcmp((char*)namearray, nc_key.data);
	//printf("%s:%d compare=%d insert %s, cursor at %s\n", __FILE__, __LINE__, compare, namearray, (char*)nc_key.data);
	if (compare>0) calc_n_items++;
	count_all_items++;
    }
}

void delete_oldest_expired (void) {
    int r;
    int r3=random()%3;
    if (delete_cursor==0) {
	r = expiredb->cursor(expiredb, null_txn, &delete_cursor, 0); CKERR(r);
	
    }
    DBT key,pkey,data, savepkey;
    memset(&key, 0, sizeof(key));
    memset(&pkey, 0, sizeof(pkey));
    memset(&data, 0, sizeof(data));
    r = delete_cursor->c_pget(delete_cursor, &key, &pkey, &data, DB_FIRST);
    if (r==DB_NOTFOUND) return;
    CKERR(r);
    {
	char *deleted_key = ((char*)data.data)+name_offset_in_pd_dbt();
	int compare=strcmp(deleted_key, nc_key.data);
	if (compare>0) {
	    //printf("%s:%d r3=%d compare=%d count=%d cacount=%d cucount=%d deleting %s cursor=%s\n", __FILE__, __LINE__, r3, compare, count_all_items, calc_n_items, cursor_count_n_items, deleted_key, (char*)nc_key.data);
	    calc_n_items--;
	}
	count_all_items--;
    }
    savepkey = pkey;
    savepkey.data = malloc(pkey.size);
    memcpy(savepkey.data, pkey.data, pkey.size);
    switch (r3) {
    case 0:
	r = delete_cursor->c_del(delete_cursor, 0);  CKERR(r);
	break;
    case 1:
	r = expiredb->del(expiredb, null_txn, &key, 0); CKERR(r);
	break;
    case 2:
	r = dbp->del(dbp, null_txn, &pkey, 0);   CKERR(r);
	break;
    default:
	assert(0);
    }
    // Make sure it's really gone.
    r = delete_cursor->c_get(delete_cursor, &key, &data, DB_CURRENT);
    assert(r==DB_KEYEMPTY);
    r = dbp->get(dbp, null_txn, &savepkey, &data, 0);
    assert(r==DB_NOTFOUND);
    free(savepkey.data);
}

// Use a cursor to step through the names.
void step_name (void) {
    int r;
    if (name_cursor==0) {
	r = namedb->cursor(namedb, null_txn, &name_cursor, 0); CKERR(r);
    }
    r = name_cursor->c_get(name_cursor, &nc_key, &nc_data, DB_NEXT); // an uninitialized cursor should do a DB_FIRST.
    if (r==0) {
	cursor_count_n_items++;
    } else if (r==DB_NOTFOUND) {
	// Got to the end.
	printf("%s:%d Got to end count=%d curscount=%d\n", __FILE__, __LINE__, calc_n_items, cursor_count_n_items);
	assert(cursor_count_n_items==calc_n_items);
	r = name_cursor->c_get(name_cursor, &nc_key, &nc_data, DB_FIRST);
	if (r==DB_NOTFOUND) {
	    nc_key.data = realloc(nc_key.data, 1);
	    ((char*)nc_key.data)[0]=0;
	    cursor_count_n_items=0;
	} else {
	    cursor_count_n_items=1;
	}
	calc_n_items = count_all_items;
    }
}

int cursor_load=2; /* Set this to a higher number to do more cursor work for every insertion.   Needed to get to the end. */

void activity (void) {
    if (random()%20==0) {
	// Delete the oldest expired one.  Keep the cursor open
	delete_oldest_expired();
    } else if (random()%cursor_load==0) {
	insert_person();
    } else {
	step_name();
    }
    //assert(count_all_items==count_entries(dbp));
}
		       

void usage (const char *argv1) {
    fprintf(stderr, "Usage:\n %s [ --DB-CREATE | --more ] seed ", argv1);
    exit(1);
}

int main (int argc, const char *argv[]) {
    const char *progname=argv[0];
    int useseed;

    {
	struct timeval tv;
	gettimeofday(&tv, 0);
	useseed = tv.tv_sec+tv.tv_usec*997;  // magic:  997 is a prime, and a million (microseconds/second) times 997 is still 32 bits.
    }

    memset(&nc_key, 0, sizeof(nc_key));
    memset(&nc_data, 0, sizeof(nc_data));
    nc_key.flags = DB_DBT_REALLOC;
    nc_key.data = malloc(1); // Iniitalize it.
    ((char*)nc_key.data)[0]=0;
    nc_data.flags = DB_DBT_REALLOC;
    nc_data.data = malloc(1); // Iniitalize it.


    mode = MODE_DEFAULT;
    argv++; argc--;
    while (argc>0) {
	if (strcmp(argv[0], "--DB_CREATE")==0) {
	    mode = MODE_DB_CREATE;
	} else if (strcmp(argv[0], "--more")==0) {
	    mode = MODE_MORE;
	} else {
	    errno=0;
	    char *endptr;
	    useseed = strtoul(argv[0], &endptr, 10);
	    printf("Got %d\n", useseed);
	    if (errno!=0 || *endptr!=0 || endptr==argv[0]) {
		usage(progname);
	    }
	}
	argc--; argv++;
    }

    switch (mode) {
    case MODE_DEFAULT:
	system("rm -rf " DIR);
	mkdir(DIR, 0777); 
	create_databases();
	{
	    int i;
	    for (i=0; i<100; i++)
		activity();
	}
	break;
    case MODE_MORE:
	create_databases();
	printf("seed=%d\n", useseed);
	srandom(useseed);
	calc_n_items = count_all_items = count_entries(dbp);
	//printf("%s:%d n_items initially=%d\n", __FILE__, __LINE__, count_all_items);
	{
	    const int n_activities = 100000;
	    int i;
	    cursor_load = 8*(1+2*count_all_items/n_activities);
	    printf("%s:%d count=%d cursor_load=%d\n", __FILE__, __LINE__, count_all_items, cursor_load);
	    for (i=0; i<n_activities; i++)
		activity();
	}
	break;
    case MODE_DB_CREATE:
	do_create();
	break;
    }

    close_databases();

    return 0;
}

