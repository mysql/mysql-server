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
#include "trace.h"

static enum mode {
    MODE_DEFAULT, MODE_MORE
} mode;



/* Primary is a map from a UID which consists of a random number followed by the current time. */

typedef unsigned char TIMESTAMP;

struct primary_key {
    TIMESTAMP ts;
};

struct name_key {
    unsigned char* name;
};

struct primary_data {
    TIMESTAMP expiretime; /* not valid if doesexpire==0 */
    unsigned char doesexpire;
    struct name_key name;
};

static void write_uchar_to_dbt (DBT *dbt, const unsigned char c) {
    assert(dbt->size+1 <= dbt->ulen);
    ((char*)dbt->data)[dbt->size++]=c;
}

static void write_timestamp_to_dbt (DBT *dbt, const TIMESTAMP ts) {
    write_uchar_to_dbt(dbt, ts);
}

static void write_pk_to_dbt (DBT *dbt, const struct primary_key *pk) {
    write_timestamp_to_dbt(dbt, pk->ts);
}

static void write_name_to_dbt (DBT *dbt, const struct name_key *nk) {
    int i;
    for (i=0; 1; i++) {
	write_uchar_to_dbt(dbt, nk->name[i]);
	if (nk->name[i]==0) break;
    }
}

static void write_pd_to_dbt (DBT *dbt, const struct primary_data *pd) {
    write_timestamp_to_dbt(dbt, pd->expiretime);
    write_uchar_to_dbt(dbt, pd->doesexpire);
    write_name_to_dbt(dbt, &pd->name);
}

static int name_offset_in_pd_dbt (void) {
    assert(sizeof(TIMESTAMP)==1);
    return 1+sizeof(TIMESTAMP);
}

static int name_callback (DB *secondary __attribute__((__unused__)), const DBT *key, const DBT *data, DBT *result) {
    /* This one does work. */
    static char buf[1000];
    char *rdata = ((char*)data->data)+name_offset_in_pd_dbt();
    int   rsize = data->size-name_offset_in_pd_dbt();
    memset(result, 0, sizeof(*result));
    result->size = rsize;
    result->data = buf;
    memcpy(buf, rdata, rsize);
    //result->data=rdata; /* This breaks bdb */
    return 0;
}


static int expire_callback (DB *secondary __attribute__((__unused__)), const DBT *key, const DBT *data, DBT *result) {
    struct primary_data *d = data->data;
    if (d->doesexpire) {
	result->flags=0;
	result->size=sizeof(TIMESTAMP);
	result->data=&d->expiretime;
	return 0;
    } else {
	return DB_DONOTINDEX;
    }
}

// The expire_key is simply a timestamp.

static DB_ENV *dbenv;
static DB *dbp,*namedb,*expiredb;

static DB_TXN * const null_txn=0;

static DBC *delete_cursor=0, *name_cursor=0;

// We use a cursor to count the names.
static int cursor_count_n_items=0; // The number of items the cursor saw as it scanned over.
static int calc_n_items=0;        // The number of items we expect the cursor to acount
static int count_all_items=0;      // The total number of items
static DBT nc_key,nc_data;

static void create_databases (void) {
    int r;

    r = db_env_create(&dbenv, 0);                                                            CKERR(r);
    r = dbenv->open(dbenv, DIR, DB_PRIVATE|DB_INIT_MPOOL|DB_CREATE, 0);                      CKERR(r);

#ifdef USE_BDB
    dbenv->set_errfile(dbenv, stderr);
#endif

    r = db_create(&dbp, dbenv, 0);                                                           CKERR(r);
    r = dbp->open(dbp, null_txn, "primary.db", NULL, DB_BTREE, DB_CREATE, 0600);             CKERR(r);

    r = db_create(&namedb, dbenv, 0);                                                        CKERR(r);
    r = namedb->open(namedb, null_txn, "name.db", NULL, DB_BTREE, DB_CREATE, 0600);          CKERR(r);

    r = db_create(&expiredb, dbenv, 0);                                                      CKERR(r);
    r = expiredb->open(expiredb, null_txn, "expire.db", NULL, DB_BTREE, DB_CREATE, 0600);    CKERR(r);
    
    r = dbp->associate(dbp, NULL, namedb, name_callback, 0);                                 CKERR(r);
    r = dbp->associate(dbp, NULL, expiredb, expire_callback, 0);                             CKERR(r);
}

static void close_databases (void) {
    int r;
    if (delete_cursor) {
	r = delete_cursor->c_close(delete_cursor); CKERR(r);
    }
    delete_cursor=0;
    if (name_cursor) {
	r = name_cursor->c_close(name_cursor);     CKERR(r);
    }
    name_cursor=0;
    if (nc_key.data) free(nc_key.data);
    if (nc_data.data) free(nc_data.data);
    r = namedb->close(namedb, 0);     CKERR(r);
    r = dbp->close(dbp, 0);           CKERR(r);
    r = expiredb->close(expiredb, 0); CKERR(r);
    r = dbenv->close(dbenv, 0);       CKERR(r);
}
    

static void gettod (TIMESTAMP *ts) {
    static int counter=0;
    assert(counter<127);
    *ts = counter++;
}

static int oppass=0, opnum=0;

static void insert_person (void) {
    struct primary_key  pk;
    struct primary_data pd;
    char keyarray[1000], dataarray[1000]; 
    const char *namearray;
    gettod(&pk.ts);
    pd.expiretime   = pk.ts;
    pd.doesexpire = oppass==1 && (opnum==2 || opnum==10 || opnum==22);
    if (oppass==1 && opnum==1)       namearray="Hc"; // If we shorten this string we get corrupt secondary errors in BDB 4.3.29.
    else if (oppass==1 && opnum==2)  namearray="K";
    else if (oppass==1 && opnum==5)  namearray="V";
    else if (oppass==1 && opnum==6)  namearray="T";
    else if (oppass==1 && opnum==9)  namearray="C";
    else if (oppass==1 && opnum==10) namearray="O";
    else if (oppass==1 && opnum==13) namearray="Q";
    else if (oppass==1 && opnum==14) namearray="U";
    else if (oppass==1 && opnum==15) namearray="P";
    else if (oppass==1 && opnum==16) namearray="S";
    else if (oppass==1 && opnum==22) namearray="E";
    else if (oppass==1 && opnum==24) namearray="M";
    else if (oppass==1 && opnum==25) namearray="R";
    else if (oppass==1 && opnum==26) namearray="W";
    else if (oppass==1 && opnum==30) namearray="B";
    else if (oppass==2 && opnum==9)  namearray="Dd"; // If we shorten this string we get corrupt secondary errors in BDB.
    else if (oppass==2 && opnum==15) namearray="A";
    else assert(0);
    DBT key,data;
    pd.name.name = (unsigned char*)namearray;
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
    int r=do_put("dbp", dbp, &key, &data);   CKERR(r);
    // If the cursor is to the left of the current item, then increment count_items
    {
	int compare=strcmp(namearray, nc_key.data);
	// Verify that the first byte matches the strcmp function.
	if (compare<0) assert(namearray[0]< ((char*)nc_key.data)[0]);
	if (compare==0) assert(namearray[0]== ((char*)nc_key.data)[0]);
	if (compare>0) assert(namearray[0]> ((char*)nc_key.data)[0]);
	//printf("%s:%d compare=%d insert %s, cursor at %s\n", __FILE__, __LINE__, compare, namearray, (char*)nc_key.data);

	// Update the counters
	if (compare>0) calc_n_items++;
	count_all_items++;
    }
}

static void delete_oldest_expired (void) {
    int r;
    if (delete_cursor==0) {
	r = expiredb->cursor(expiredb, null_txn, &delete_cursor, 0); CKERR(r);
	
    }
    DBT key,pkey,data, savepkey;
    memset(&key, 0, sizeof(key));
    memset(&pkey, 0, sizeof(pkey));
    memset(&data, 0, sizeof(data));
    r = do_cpget("delete_cursor", delete_cursor, &key, &pkey, &data, DB_FIRST);
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
    r = do_del("dbp", dbp, &pkey);   CKERR(r);
    // Make sure it's really gone.
    r = do_cget("delete_cursor", delete_cursor, &key, &data, DB_CURRENT);
    assert(r==DB_KEYEMPTY);
    r = do_get("dbp", dbp, &savepkey, &data);
    assert(r==DB_NOTFOUND);
    free(savepkey.data);
}

// Use a cursor to step through the names.
static void step_name (void) {
    int r;
    if (name_cursor==0) {
	r = namedb->cursor(namedb, null_txn, &name_cursor, 0); CKERR(r);
    }
    r = do_cget("name_cursor", name_cursor, &nc_key, &nc_data, DB_NEXT); // an uninitialized cursor should do a DB_FIRST.
    if (r==0) {
	cursor_count_n_items++;
    } else if (r==DB_NOTFOUND) {
	// Got to the end.
	//printf("%s:%d Got to end count=%d curscount=%d\n", __FILE__, __LINE__, calc_n_items, cursor_count_n_items);
	assert(cursor_count_n_items==calc_n_items);
	r = do_cget("name_cursor", name_cursor, &nc_key, &nc_data, DB_FIRST);
	//r = name_cursor->c_get(name_cursor, &nc_key, &nc_data, DB_FIRST);
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

static void activity (void) {
    int do_delete = (oppass==1 && opnum==32) || (oppass==2 && opnum==8);
    if (do_delete) {
	// Delete the oldest expired one.  Keep the cursor open
	delete_oldest_expired();
    } else {
	int do_insert = ( (oppass==1 && opnum==1) 
			  || (oppass==1 && opnum==2)
			  || (oppass==1 && opnum==5)
			  || (oppass==1 && opnum==6)
			  || (oppass==1 && opnum==9)
			  || (oppass==1 && opnum==10)
			  || (oppass==1 && opnum==13)
			  || (oppass==1 && opnum==14)
			  || (oppass==1 && opnum==15)
			  || (oppass==1 && opnum==16)
			  || (oppass==1 && opnum==22)
			  || (oppass==1 && opnum==24)
			  || (oppass==1 && opnum==25)
			  || (oppass==1 && opnum==26)
			  || (oppass==1 && opnum==30)
			  || (oppass==2 && opnum==9)
			  || (oppass==2 && opnum==15)
			  );
	if (do_insert) {
	    insert_person();
	} else {
	    step_name();
	}
    }
    //assert(count_all_items==count_entries(dbp));
}
		       

static void usage (const char *argv1) {
    fprintf(stderr, "Usage:\n %s [ --more ]\n", argv1);
    exit(1);
}

int main (int argc, const char *argv[]) {
    const char *progname=argv[0];

    memset(&nc_key, 0, sizeof(nc_key));
    memset(&nc_data, 0, sizeof(nc_data));
    nc_key.flags = DB_DBT_REALLOC;
    nc_key.data = malloc(1); // Initalize it.
    ((char*)nc_key.data)[0]=0;
    nc_data.flags = DB_DBT_REALLOC;
    nc_data.data = malloc(1); // Initalize it.


    mode = MODE_DEFAULT;
    argv++; argc--;
    while (argc>0) {
	if (strcmp(argv[0], "--more")==0) {
	    mode = MODE_MORE;
	} else {
	    usage(progname);
	}
	argc--; argv++;
    }

    switch (mode) {
    case MODE_DEFAULT:
	oppass=1;
	system("rm -rf " DIR);
	mkdir(DIR, 0777); 
	create_databases();
	{
	    int i;
	    for (i=0; i<33; i++) {
		opnum=i;
		activity();
	    }
	}
	close_databases();
	
	break;
    case MODE_MORE:
	oppass=2;
	create_databases();
	calc_n_items = count_all_items = 14;//count_entries("dbc", dbp);
	//fprintf(stderr, "%s:%d n_items initially=%d\n", __FILE__, __LINE__, count_all_items);
	{
	    const int n_activities = 32;
	    int i;
	    //printf("%s:%d count=%d cursor_load=%d\n", __FILE__, __LINE__, count_all_items, cursor_load);
	    for (i=0; i<n_activities; i++) {
		opnum=i;
		activity();
	    }
	}
	close_databases();
	break;
    }

    return 0;
}

