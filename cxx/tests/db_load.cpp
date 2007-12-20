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

void hexput(Dbt *d, unsigned char c) {
    unsigned char *cp = (unsigned char *) d->get_data();
    int n = d->get_size();
    int ulen = d->get_ulen();
    if (n+1 >= ulen) {
        int newulen = ulen == 0 ? 1 : ulen*2;
        cp = (unsigned char *) realloc(cp, newulen); assert(cp);
        d->set_data(cp);
        d->set_ulen(newulen);
    }
    cp[n++] = c;
    d->set_size(n);
}

int hextrans(int c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return 0;
}

int hexload(Dbt *d) {
    d->set_size(0);
    int c = getchar();
    if (c == EOF || c != ' ') return 0;
    for (;;) {
        int c0 = getchar();
        if (c0 == EOF) return 0;
        if (c0 == '\n') break;
        int c1 = getchar();
        if (c1 == EOF) return 0;
        if (c1 == '\n') break;
        hexput(d, (hextrans(c0) << 4) + hextrans(c1));
    }

    return 1;
}

int dbload(char *dbfile, char *dbname) {
    int r;

#if USE_ENV
    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    r = env.open(".", DB_INIT_MPOOL + DB_CREATE + DB_PRIVATE, 0777); assert(r == 0);
    Db db(&env, DB_CXX_NO_EXCEPTIONS);
#else
    Db db(0, DB_CXX_NO_EXCEPTIONS);
#endif
    r = db.open(0, dbfile, dbname, DB_BTREE, DB_CREATE, 0777); 
    if (r != 0) {
        printf("cant open %s:%s\n", dbfile, dbname);
#if USE_ENV
        r = env.close(0); assert(r == 0);
#endif
        return 1;
    }
    
    Dbt key, val;
    for (;;) {
        if (!hexload(&key)) break;
        // hexdump(&key);
        if (!hexload(&val)) break;
        // hexdump(&val);
        r = db.put(0, &key, &val, 0);
        assert(r == 0);
    }
    if (key.get_data()) free(key.get_data());
    if (val.get_data()) free(val.get_data());

    r = db.close(0); assert(r == 0);
#if USE_ENV
    r = env.close(0); assert(r == 0);
#endif
    return 0;
}

int usage() {
    printf("db_load [-s DBNAME] DBFILE\n");
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
    return dbload(argv[i], dbname);
}

