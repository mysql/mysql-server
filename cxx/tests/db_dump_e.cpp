/* Just like db_dump.cpp except use exceptions. */
#include <stdlib.h>
#include <assert.h>
#include <db_cxx.h>

void hexdump(Dbt *d) {
    unsigned char *cp = (unsigned char *) d->get_data();
    int n = d->get_size();
    printf(" ");
    for (int i=0; i<n; i++)
        printf("%2.2x", cp[i]);
    printf("\n");
}

int dbdump(char *dbfile, char *dbname) {
    int r;

#if USE_ENV
    DbEnv env(0);
    r = env.open(".", DB_INIT_MPOOL + DB_CREATE + DB_PRIVATE, 0777); assert(r == 0);
    Db db(&env, 0);
#else
    Db db(0, 0);
#endif
    try {
	r = db.open(0, dbfile, dbname, DB_BTREE, 0, 0777);
	assert(r==0);
    } catch (DbException e) {
	printf("Cannot open %s:%s due to error %d\n", dbfile, dbname, e.get_errno());
#if USE_ENV
        r = env.close(0); assert(r == 0);
#endif
        return 1;
    }

    Dbc *cursor;
    r = db.cursor(0, &cursor, 0); assert(r == 0);

    Dbt key; key.set_flags(DB_DBT_REALLOC);
    Dbt val; val.set_flags(DB_DBT_REALLOC);
    try {
	for (;;) {
	    r = cursor->get(&key, &val, DB_NEXT);
            if (r == DB_NOTFOUND) break;
	    assert(r==0);
	    // printf("%.*s\n", key.get_size(), (char *)key.get_data());
	    hexdump(&key);
	    // printf("%.*s\n", val.get_size(), (char *)val.get_data());
	    hexdump(&val);
        }
    } catch (DbException ) {
	/* Nothing, that's just how we got out of the loop. */
    }
    free(key.get_data());
    free(val.get_data());

    r = cursor->close(); assert(r == 0);
    r = db.close(0); assert(r == 0);
#if USE_ENV
    r = env.close(0); assert(r == 0);
#endif
    return 0;
}

int usage() {
    printf("db_dump [-s DBNAME] DBFILE\n");
    return 1;
}

int main(int argc, char *argv[]) {
    int i;

    char *dbname = 0;
    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (0 == strcmp(arg, "-h") || 0 == strcmp(arg, "--help")) 
            return usage();
        if (0 == strcmp(arg, "-s")) {
            i++;
            if (i >= argc)
                return usage();
            dbname = argv[i];
            continue;
        }
        break;
    }

    if (i >= argc)
        return usage();
    return dbdump(argv[i], dbname);
}

