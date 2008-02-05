#include <assert.h>
#include <db_cxx.h>

int dbcreate(char *dbfile, char *dbname, int dbflags, int argc, char *argv[]) {
    int r;
#if USE_ENV
    DbEnv *env = new DbEnv(DB_CXX_NO_EXCEPTIONS);
    r = env->open(".", DB_INIT_MPOOL + DB_CREATE + DB_PRIVATE, 0777); assert(r == 0);
#else
    DbEnv *env = 0;
#endif
    Db *db = new Db(env, DB_CXX_NO_EXCEPTIONS);
    r = db->set_flags(dbflags); 
    assert(r == 0);
    r = db->open(0, dbfile, dbname, DB_BTREE, DB_CREATE, 0777);
    assert(r == 0);

    int i = 0;
    while (i < argc) {
        char *k = argv[i++];
        if (i < argc) {
            char *v = argv[i++];
            Dbt key(k, strlen(k)); Dbt val(v, strlen(v));
            r = db->put(0, &key, &val, 0); assert(r == 0);
        }
    }
            
    r = db->close(0); assert(r == 0);
    if (env) {
        r = env->close(0); assert(r == 0);
        delete env;
    }
    delete db;
    return 0;
}

int usage() {
    printf("db_create [-s DBNAME] [-D] [-S] DBFILE [KEY VAL]*\n");
    return 1;
}

int main(int argc, char *argv[]) {
    char *dbname = 0;
    int dbflags = 0;

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
        if (0 == strcmp(arg, "-D")) {
            dbflags += DB_DUP;
            continue;
        }
        if (0 == strcmp(arg, "-S")) {
            dbflags += DB_DUPSORT;
            continue;
        }
        break;
    }

    if (i >= argc)
        return usage();
    char *dbfile = argv[i++];
    return dbcreate(dbfile, dbname, dbflags, argc-i, &argv[i]);
}

