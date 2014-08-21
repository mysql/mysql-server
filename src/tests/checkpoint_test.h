/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */ 
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#pragma once

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

DB_ENV *env;

enum {MAX_NAME=128};

enum {NUM_FIXED_ROWS=1025};   // 4K + 1

typedef struct {
    DB*       db;
    uint32_t flags;
    char      filename[MAX_NAME]; //Relative to envdir/
    int       num;
} DICTIONARY_S, *DICTIONARY;


static inline int64_t
generate_val(int64_t key) {
    int64_t val = key + 314;
    return val;
}

// return 0 if same
static int
verify_identical_dbts(const DBT *dbt1, const DBT *dbt2) {
    int r = 0;
    if (dbt1->size != dbt2->size) r = 1;
    else if (memcmp(dbt1->data, dbt2->data, dbt1->size)!=0) r = 1;
    return r;
}

// return 0 if same
static int UU()
compare_dbs(DB *compare_db1, DB *compare_db2) {
    //This does not lock the dbs/grab table locks.
    //This means that you CANNOT CALL THIS while another thread is modifying the db.
    //You CAN call it while a txn is open however.
    int rval = 0;
    DB_TXN *compare_txn;
    int r, r1, r2;
    r = env->txn_begin(env, NULL, &compare_txn, DB_READ_UNCOMMITTED);
        CKERR(r);
    DBC *c1;
    DBC *c2;
    r = compare_db1->cursor(compare_db1, compare_txn, &c1, 0);
        CKERR(r);
    r = compare_db2->cursor(compare_db2, compare_txn, &c2, 0);
        CKERR(r);

    DBT key1, val1;
    DBT key2, val2;

    dbt_init_realloc(&key1);
    dbt_init_realloc(&val1);
    dbt_init_realloc(&key2);
    dbt_init_realloc(&val2);

    do {
        r1 = c1->c_get(c1, &key1, &val1, DB_NEXT);
        r2 = c2->c_get(c2, &key2, &val2, DB_NEXT);
        assert(r1==0 || r1==DB_NOTFOUND);
        assert(r2==0 || r2==DB_NOTFOUND);
	if (r1!=r2) rval = 1;
        else if (r1==0 && r2==0) {
            //Both found
            rval = verify_identical_dbts(&key1, &key2) |
		   verify_identical_dbts(&val1, &val2);
        }
    } while (r1==0 && r2==0 && rval==0);
    c1->c_close(c1);
    c2->c_close(c2);
    if (key1.data) toku_free(key1.data);
    if (val1.data) toku_free(val1.data);
    if (key2.data) toku_free(key2.data);
    if (val2.data) toku_free(val2.data);
    compare_txn->commit(compare_txn, 0);
    return rval;
}


static void UU()
dir_create(const char *envdir) {
    int r;
    toku_os_recursive_delete(envdir);
    r = toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
}

// pass in zeroes for default cachesize
static void  UU()
env_startup(const char *envdir, int64_t bytes, int recovery_flags) {
    int r;
    r = db_env_create(&env, 0);
        CKERR(r);
    r = env->set_redzone(env, 0); CKERR(r);
    r = env->set_default_bt_compare(env, int64_dbt_cmp);
        CKERR(r);
    if (bytes) {
	r = env->set_cachesize(env, bytes >> 30, bytes % (1<<30), 1);
        CKERR(r);
    }
    int envflags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | recovery_flags;
    r = env->open(env, envdir, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    env->set_errfile(env, stderr);
    r = env->checkpointing_set_period(env, 0); //Disable auto-checkpointing.
        CKERR(r);
}

static void UU()
env_shutdown(void) {
    int r;
    r = env->close(env, 0);
    CKERR(r);
}

static void UU()
fill_name(DICTIONARY d, char *buf, int bufsize) {
    int bytes;
    bytes = snprintf(buf, bufsize, "%s_%08x", d->filename, d->num);
        assert(bytes>0);
        assert(bytes>(int)strlen(d->filename));
        assert(bytes<bufsize);
	assert(buf[bytes] == 0);
}

static void UU()
fill_full_name(const char *envdir, DICTIONARY d, char *buf, int bufsize) {
    int bytes;
    bytes = snprintf(buf, bufsize, "%s/%s_%08x", envdir, d->filename, d->num);
        assert(bytes>0);
        assert(bytes>(int)strlen(d->filename));
        assert(bytes<bufsize);
	assert(buf[bytes] == 0);
}

static void UU()
db_startup(DICTIONARY d, DB_TXN *open_txn) {
    int r;
    r = db_create(&d->db, env, 0);
        CKERR(r);
    DB *db = d->db;
    if (d->flags) {
        r = db->set_flags(db, d->flags);
            CKERR(r);
    }
    //Want to simulate much larger test.
    //Small nodesize means many nodes.
    db->set_pagesize(db, 1<<10);
    {
        char name[MAX_NAME*2];
        fill_name(d, name, sizeof(name));
        r = db->open(db, open_txn, name, NULL, DB_BTREE, DB_CREATE, 0666);
            CKERR(r);
    }
    {
        DBT desc;
        IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
            dbt_init(&desc, "foo", sizeof("foo"));
            { int chk_r = db->change_descriptor(db, txn_desc, &desc,0); CKERR(chk_r); }
            });
    }
}

static void UU()
db_shutdown(DICTIONARY d) {
    int r;
    r = d->db->close(d->db, 0);
        CKERR(r);
    d->db = NULL;
}

static void UU()
null_dictionary(DICTIONARY d) {
    memset(d, 0, sizeof(*d));
}

static void UU()
init_dictionary(DICTIONARY d, uint32_t flags, const char *name) {
    null_dictionary(d);
    d->flags = flags;
    strcpy(d->filename, name);
}


static void UU()
db_delete(DICTIONARY d) {
    db_shutdown(d);
    int r;
    {
        char name[MAX_NAME*2];
        fill_name(d, name, sizeof(name));
        r = env->dbremove(env, NULL, name, NULL, 0);
            CKERR(r);
    }
    null_dictionary(d);
}

// Create a new dictionary (dest) with a new dname that has same contents as given dictionary (src).
// Method:
//  create new dictionary
//  close new dictionary
//  get inames of both dictionaries
//  copy file (by iname) of src to dest
//  open dest dictionary
static void UU()
dbcpy(const char *envdir, DICTIONARY dest, DICTIONARY src, DB_TXN *open_txn) {
    int r;

    assert(dest->db == NULL);
    *dest = *src;
    dest->db = NULL;
    dest->num++;

    db_startup(dest, open_txn);
    db_shutdown(dest);

    char dest_dname[MAX_NAME*2];
    fill_name(dest, dest_dname, sizeof(dest_dname));

    char src_dname[MAX_NAME*2];
    fill_name(src, src_dname, sizeof(src_dname));

    DBT dest_dname_dbt;
    DBT dest_iname_dbt;
    DBT src_dname_dbt;
    DBT src_iname_dbt;

    dbt_init(&dest_dname_dbt, dest_dname, strlen(dest_dname)+1);
    dbt_init(&dest_iname_dbt, NULL, 0);
    dest_iname_dbt.flags |= DB_DBT_MALLOC;
    r = env->get_iname(env, &dest_dname_dbt, &dest_iname_dbt);
    CKERR(r);

    dbt_init(&src_dname_dbt, src_dname, strlen(src_dname)+1);
    dbt_init(&src_iname_dbt, NULL, 0);
    src_iname_dbt.flags |= DB_DBT_MALLOC;
    r = env->get_iname(env, &src_dname_dbt, &src_iname_dbt);
    CKERR(r);

    char * CAST_FROM_VOIDP(src_iname, src_iname_dbt.data);
    char * CAST_FROM_VOIDP(dest_iname, dest_iname_dbt.data);

    int bytes;

    char command[sizeof("cp -f ") + strlen(src_iname)+ 2 * (strlen(envdir) + strlen("/ ")) + strlen(dest_iname)];
    bytes = snprintf(command, sizeof(command), "cp -f %s/%s %s/%s", envdir, src_iname, envdir, dest_iname);
    assert(bytes<(int)sizeof(command));

    toku_free(src_iname);
    toku_free(dest_iname);

    r = system(command);
        CKERR(r);
    db_startup(dest, open_txn);
}

static void UU()
db_replace(const char *envdir, DICTIONARY d, DB_TXN *open_txn) {
    //Replaces a dictionary with a physical copy that is reopened.
    //Filename is changed by incrementing the number.
    //This should be equivalent to 'rollback to checkpoint'.
    //The DB* disappears.
    DICTIONARY_S temp;
    null_dictionary(&temp);
    dbcpy(envdir, &temp, d, open_txn);
    db_delete(d);
    *d = temp;
}

static void UU()
insert_random(DB *db1, DB *db2, DB_TXN *txn) {
    int64_t v = random();
    int64_t k = ((int64_t)(random()) << 32) + v;
    int r;
    DBT key;
    DBT val;
    dbt_init(&key, &k, sizeof(k));
    dbt_init(&val, &v, sizeof(v));

    if (db1) {
        r = db1->put(db1, txn, &key, &val, 0);
            CKERR(r);
    }
    if (db2) {
        r = db2->put(db2, txn, &key, &val, 0);
            CKERR(r);
    }
}

static void UU()
delete_both_random(DB *db1, DB *db2, DB_TXN *txn, uint32_t flags) {
    int64_t k = random64();
    int r;
    DBT key;
    dbt_init(&key, &k, sizeof(k));

    if (db1) {
        r = db1->del(db1, txn, &key, flags);
	CKERR2s(r, 0, DB_NOTFOUND);
    }
    if (db2) {
        r = db2->del(db2, txn, &key, flags);
	CKERR2s(r, 0, DB_NOTFOUND);
    }
}



static void UU()
delete_fixed(DB *db1, DB *db2, DB_TXN *txn, int64_t k, uint32_t flags) {
    int r;
    DBT key;

    dbt_init(&key, &k, sizeof(k));

    if (db1) {
        r = db1->del(db1, txn, &key, flags);
	CKERR2s(r, 0, DB_NOTFOUND);
    }
    if (db2) {
        r = db2->del(db2, txn, &key, flags);
	CKERR2s(r, 0, DB_NOTFOUND);
    }
}

static void UU()
delete_n(DB *db1, DB *db2, DB_TXN *txn, int firstkey, int n, uint32_t flags) {
    int i;
    for (i=0;i<n;i++) {
        delete_fixed(db1, db2, txn, firstkey+i, flags);
    }
}

static void
insert_n(DB *db1, DB *db2, DB_TXN *txn, int firstkey, int n, int offset) {
    int64_t k;
    int64_t v;
    int r;
    DBT key;
    DBT val;
    int i;

    //    printf("enter %s, iter = %d\n", __FUNCTION__, iter);
    //    printf("db1 = 0x%08lx, db2 = 0x%08lx, *txn = 0x%08lx, firstkey = %d, n = %d\n",
    //	   (unsigned long) db1, (unsigned long) db2, (unsigned long) txn, firstkey, n);

    fflush(stdout);

    for (i = 0; i<n; i++) {
	int64_t kk = firstkey+i;
	v = generate_val(kk) + offset;
	k = (kk<<32) + v;
	//printf("I(%32lx,%32lx)\n", k, v);
	dbt_init(&key, &k, sizeof(k));
	dbt_init(&val, &v, sizeof(v));
	if (db1) {
	    r = db1->put(db1, txn, &key, &val, 0);
            CKERR(r);
	}
	if (db2) {
	    r = db2->put(db2, txn, &key, &val, 0);
            CKERR(r);
	}
    }
}


static void UU()
insert_n_broken(DB *db1, DB *db2, DB_TXN *txn, int firstkey, int n) {
    insert_n(db1, db2, txn, firstkey, n, 2718);
}


static void UU()
insert_n_fixed(DB *db1, DB *db2, DB_TXN *txn, int firstkey, int n) {
    insert_n(db1, db2, txn, firstkey, n, 0);
}


// assert that correct values are in expected rows
static void  UU()
verify_sequential_rows(DB* compare_db, int64_t firstkey, int64_t numkeys) {
    //This does not lock the dbs/grab table locks.
    //This means that you CANNOT CALL THIS while another thread is modifying the db.
    //You CAN call it while a txn is open however.
    DB_TXN *compare_txn;
    int r, r1;

    assert(numkeys >= 1);
    r = env->txn_begin(env, NULL, &compare_txn, DB_READ_UNCOMMITTED);
        CKERR(r);
    DBC *c1;

    r = compare_db->cursor(compare_db, compare_txn, &c1, 0);
        CKERR(r);


    DBT key1, val1;
    DBT key2, val2;

    int64_t k, v;

    dbt_init_realloc(&key1);
    dbt_init_realloc(&val1);

    dbt_init(&key2, &k, sizeof(k));
    dbt_init(&val2, &v, sizeof(v));

    v = generate_val(firstkey);
    k = (firstkey<<32) + v;
    r1 = c1->c_get(c1, &key2, &val2, DB_SET);
    CKERR(r1);

    int64_t i;
    for (i = 1; i<numkeys; i++) {
	int64_t kk = firstkey+i;
	v = generate_val(kk);
	k = (kk<<32) + v;
        r1 = c1->c_get(c1, &key1, &val1, DB_NEXT);
        assert(r1==0);
	assert(key1.size==8 && val1.size==8 && *(int64_t*)key1.data==k && *(int64_t*)val1.data==v);
    }
    // now verify that there are no rows after the last expected 
    r1 = c1->c_get(c1, &key1, &val1, DB_NEXT);
    assert(r1 == DB_NOTFOUND);

    c1->c_close(c1);
    if (key1.data) toku_free(key1.data);
    if (val1.data) toku_free(val1.data);
    compare_txn->commit(compare_txn, 0);
}



static void UU()
snapshot(DICTIONARY d, int do_checkpoint) {
    if (do_checkpoint) {
        int r = env->txn_checkpoint(env, 0, 0, 0);
        CKERR(r);
    }
    else {
        db_shutdown(d);
        db_startup(d, NULL);
    }
}
