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
    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    r = env.open(".", DB_INIT_MPOOL + DB_CREATE + DB_PRIVATE, 0777); assert(r == 0);
    Db db(&env, DB_CXX_NO_EXCEPTIONS);
#else
    Db db(0, DB_CXX_NO_EXCEPTIONS);
#endif
    r = db.open(0, dbfile, dbname, DB_UNKNOWN, 0, 0777); 
    if (r != 0) {
        printf("cant open %s:%s\n", dbfile, dbname);
#if USE_ENV
        r = env.close(0); assert(r == 0);
#endif
        return 1;
    }

    u_int32_t dbflags;
    r = db.get_flags(&dbflags); assert(r == 0);
    if (dbflags & DB_DUP)
        printf("duplicates=1\n");
    if (dbflags & DB_DUPSORT)
        printf("dupsort=1\n");
#if 0
    u_int32_t nodesize;
    r = db.get_nodesize(&nodesize); assert(r == 0);
    printf("nodesize=%d\n", nodesize);
#endif

    Dbc *cursor;
    r = db.cursor(0, &cursor, 0); assert(r == 0);

    Dbt key; key.set_flags(DB_DBT_REALLOC);
    Dbt val; val.set_flags(DB_DBT_REALLOC);
    for (;;) {
        r = cursor->get(&key, &val, DB_NEXT);
        if (r != 0) break;
        // printf("%.*s\n", key.get_size(), (char *)key.get_data());
        hexdump(&key);
        // printf("%.*s\n", val.get_size(), (char *)val.get_data());
        hexdump(&val);
    }
    if (key.get_data()) free(key.get_data());
    if (val.get_data()) free(val.get_data());

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
    char *dbname = 0;

    int i;
    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (0 == strcmp(arg, "-h") || 0 == strcmp(arg, "--help")) 
            return usage();
        if (0 == strcmp(arg, "-s")) {
            if (i+1 >= argc)
                return usage();
            dbname = argv[++i];
            continue;
        }
        break;
    }
    if (i >= argc)
        return usage();

    return dbdump(argv[i], dbname);
}

