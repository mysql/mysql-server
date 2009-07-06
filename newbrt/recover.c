/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* Recover an env.  The logs are in argv[1].  The new database is created in the cwd. */

// Test:
//    cd ../src/tests/tmpdir
//    ../../../newbrt/recover ../dir.test_log2.c.tdb

#include "includes.h"

//#define DO_VERIFY_COUNTS
#ifdef DO_VERIFY_COUNTS
#define VERIFY_COUNTS(n) toku_verify_counts(n)
#else
#define VERIFY_COUNTS(n) ((void)0)
#endif

static DB * const null_db=0;
static TOKULOGGER const null_tokulogger = 0;

// These data structures really should be part of a recovery data structure.  Recovery could be multithreaded (on different environments...)  But this is OK since recovery can only happen in one
static CACHETABLE ct;
static struct cf_pair {
    FILENUM filenum;
    CACHEFILE cf;
    BRT       brt; // set to zero on an fopen, but filled in when an fheader is seen.
} *cf_pairs;
static int n_cf_pairs=0, max_cf_pairs=0;

int toku_recover_init (void) {
    int r = toku_create_cachetable(&ct, 1<<25, (LSN){0}, 0);
    return r;
}

void toku_recover_cleanup (void) {
    int i;
    for (i=0; i<n_cf_pairs; i++) {
	if (cf_pairs[i].brt) {
	    int r = toku_close_brt(cf_pairs[i].brt, 0, 0);
	    //r = toku_cachefile_close(&cf_pairs[i].cf);
	    assert(r==0);
	}
    }
    toku_free(cf_pairs);
    {
	int r = toku_cachetable_close(&ct);
	assert(r==0);
    }
}

enum backward_state { BS_INIT, BS_SAW_CKPT_END, BS_SAW_CKPT };
struct backward_scan_state {
    enum backward_state bs;
    LSN checkpoint_lsn;
    int n_live_txns;
    LSN min_live_txn;
};
static struct backward_scan_state initial_bss = {BS_INIT,{0},0,{0}};

static void
toku_recover_commit (LSN UU(lsn), TXNID UU(txnid)) {
}
static int toku_recover_backward_commit (struct logtype_commit *UU(l), struct backward_scan_state *UU(bs)) {
    return 0;
}

static void
toku_recover_xabort (LSN UU(lsn), TXNID UU(txnid)) {
}
static int toku_recover_backward_xabort (struct logtype_xabort *UU(l), struct backward_scan_state *UU(bs)) {
    return 0;
}


static void
create_dir_from_file (const char *fname) {
    int i;
    char *tmp=toku_strdup(fname);
    char ch;
    for (i=0; (ch=fname[i]); i++) {
        //
        // TODO: this may fail in windows, double check the absolute path names
        // and '/' as the directory delimiter or something
        //
	if (ch=='/') {
	    if (i>0) {
		tmp[i]=0;
		mode_t oldu = umask(0);
		int r = toku_os_mkdir(tmp, S_IRWXU);
		if (r!=0 && errno!=EEXIST) {
		    printf("error: %s\n", strerror(errno));
		}
		assert (r==0 || (errno==EEXIST));
		umask(oldu);
		tmp[i]=ch;
	    }
	}
    }
    toku_free(tmp);
}

static int
toku_recover_note_cachefile (FILENUM fnum, CACHEFILE cf, BRT brt) {
    if (max_cf_pairs==0) {
	n_cf_pairs=1;
	max_cf_pairs=2;
	MALLOC_N(max_cf_pairs, cf_pairs);
	if (cf_pairs==0) return errno;
    } else {
	if (n_cf_pairs>=max_cf_pairs) {
	    max_cf_pairs*=2;
	    cf_pairs = toku_realloc(cf_pairs, max_cf_pairs*sizeof(*cf_pairs));
	}
	n_cf_pairs++;
    }
    cf_pairs[n_cf_pairs-1].filenum = fnum;
    cf_pairs[n_cf_pairs-1].cf      = cf;
    cf_pairs[n_cf_pairs-1].brt     = brt;
    return 0;
}

static int find_cachefile (FILENUM fnum, struct cf_pair **cf_pair) {
    int i;
    for (i=0; i<n_cf_pairs; i++) {
	if (fnum.fileid==cf_pairs[i].filenum.fileid) {
	    *cf_pair = cf_pairs+i;
	    return 0;
	}
    }
    return 1;
}

static void
internal_toku_recover_fopen_or_fcreate (int flags, int mode, char *fixedfname, FILENUM filenum)
// Open the file if it is not already open.  If it is already open, then do nothing.
{
    {
	struct cf_pair *pair = NULL;
	int r = find_cachefile(filenum, &pair);
	if (0==r) {
	    toku_free(fixedfname);
	    return;
	}
    }

    CACHEFILE cf;
    int fd = open(fixedfname, O_RDWR|O_BINARY|flags, mode);
    if (fd<0) {
	char org_wd[1000];
	char *wd=getcwd(org_wd, sizeof(org_wd));
	fprintf(stderr, "%s:%d Could not open file %s, cwd=%s, errno=%d (%s)\n",
		__FILE__, __LINE__, fixedfname, wd, errno, strerror(errno));
    }
    assert(fd>=0);
    BRT brt=0;
    int r = toku_brt_create(&brt);
    assert(r==0);
    brt->fname = fixedfname;
    brt->h=0;
    brt->compare_fun = toku_default_compare_fun; // we'll need to set these to the right comparison function, or do without them.
    brt->dup_compare = toku_default_compare_fun;
    brt->db = 0;
    r = toku_cachetable_openfd(&cf, ct, fd, fixedfname);
    assert(r==0);
    brt->cf=cf;
    r = toku_read_brt_header_and_store_in_cachefile(brt->cf, &brt->h);
    if (r==TOKUDB_DICTIONARY_NO_HEADER) {
	r = toku_brt_alloc_init_header(brt);
    }
    toku_recover_note_cachefile(filenum, cf, brt);
}

static void
toku_recover_fopen (LSN UU(lsn), TXNID UU(txnid), BYTESTRING fname, FILENUM filenum) {
    char *fixedfname = fixup_fname(&fname);
    toku_free_BYTESTRING(fname);
    internal_toku_recover_fopen_or_fcreate(0, 0, fixedfname, filenum);
}

static int toku_recover_backward_fopen (struct logtype_fopen *l, struct backward_scan_state *UU(bs)) {
    toku_free_BYTESTRING(l->fname);
    return 0;
}


// fcreate is like fopen except that the file must be created. Also creates the dir if needed.
static void
toku_recover_fcreate (LSN UU(lsn), TXNID UU(txnid), FILENUM filenum, BYTESTRING fname,u_int32_t mode) {
    char *fixedfname = fixup_fname(&fname);
    toku_free_BYTESTRING(fname);
    create_dir_from_file(fixedfname);
    internal_toku_recover_fopen_or_fcreate(O_CREAT|O_TRUNC, mode, fixedfname, filenum);
}

static int toku_recover_backward_fcreate (struct logtype_fcreate *l, struct backward_scan_state *UU(bs)) {
    toku_free_BYTESTRING(l->fname);
    return 0;
}

static void toku_recover_fheader (LSN UU(lsn), TXNID UU(txnid),FILENUM filenum, LOGGEDBRTHEADER header) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    struct brt_header *MALLOC(h);
    assert(h);
    h->dirty=0;
    h->panic=0;
    h->panic_string=0;
    h->flags = header.flags;
    h->nodesize = header.nodesize;
    //toku_blocktable_create_from_loggedheader(&h->blocktable, header);
    assert(0); //create from loggedheader disabled for now. //TODO: #1605
    assert(h->blocktable);
    h->root = header.root;
    h->root_hash.valid= FALSE;
    //toku_cachetable_put(pair->cf, header_blocknum, fullhash, h, 0, toku_brtheader_flush_callback, toku_brtheader_fetch_callback, 0);
    if (pair->brt) {
	toku_free(pair->brt->h);
    }  else {
	r = toku_brt_create(&pair->brt);
	assert(r==0);
	pair->brt->cf = pair->cf;
	list_init(&pair->brt->cursors);
	pair->brt->compare_fun = 0;
	pair->brt->dup_compare = 0;
	pair->brt->db = 0;
    }
    pair->brt->h = h;
    pair->brt->nodesize = h->nodesize;
    pair->brt->flags    = h->nodesize;
    toku_cachefile_set_userdata(pair->cf, pair->brt->h, toku_brtheader_close, toku_brtheader_checkpoint, toku_brtheader_begin_checkpoint, toku_brtheader_end_checkpoint);
}

static int toku_recover_backward_fheader (struct logtype_fheader *UU(l), struct backward_scan_state *UU(bs)) {
    return 0;
}

static void
toku_recover_enqrootentry (LSN lsn __attribute__((__unused__)), FILENUM filenum, TXNID xid, u_int32_t typ, BYTESTRING key, BYTESTRING val) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return;
    }    
    struct brt_cmd cmd;
    DBT keydbt, valdbt;
    cmd.type=(enum brt_cmd_type) typ;
    cmd.xid =xid;
    cmd.u.id.key = toku_fill_dbt(&keydbt, key.data, key.len);
    cmd.u.id.val = toku_fill_dbt(&valdbt, val.data, val.len);
    r = toku_brt_root_put_cmd(pair->brt, &cmd, null_tokulogger);
    assert(r==0);
    toku_free(key.data);
    toku_free(val.data);
}

static int toku_recover_backward_enqrootentry (struct logtype_enqrootentry *l, struct backward_scan_state *UU(bs)) {
    toku_free_BYTESTRING(l->key);
    toku_free_BYTESTRING(l->data);
    return 0;
}


static void
toku_recover_brtclose (LSN UU(lsn), BYTESTRING UU(fname), FILENUM filenum) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    r = toku_close_brt(pair->brt, 0, 0);
    assert(r==0);
    pair->brt=0;
    toku_free_BYTESTRING(fname);
}

static int toku_recover_backward_brtclose (struct logtype_brtclose *l, struct backward_scan_state *UU(bs)) {
    toku_free_BYTESTRING(l->fname);
    return 0;
}

static void
toku_recover_cfclose (LSN UU(lsn), BYTESTRING UU(fname), FILENUM filenum) {
    int i;
    for (i=0; i<n_cf_pairs; i++) {
	if (filenum.fileid==cf_pairs[i].filenum.fileid) {
	    int r = toku_cachefile_close(&cf_pairs[i].cf, 0, 0, ZERO_LSN);
	    assert(r==0);
	    cf_pairs[i] = cf_pairs[n_cf_pairs-1];
	    n_cf_pairs--;
	    break;
	}
    }
    toku_free_BYTESTRING(fname);
}

static int toku_recover_backward_cfclose (struct logtype_cfclose *UU(l), struct backward_scan_state *UU(bs)) {
    return 0;
}

static void
toku_recover_changeunnamedroot (LSN UU(lsn), FILENUM filenum, BLOCKNUM UU(oldroot), BLOCKNUM newroot) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    assert(pair->brt);
    assert(pair->brt->h);
    pair->brt->h->root = newroot;
    pair->brt->h->root_hash.valid = FALSE;
}
static int toku_recover_backward_changeunnamedroot (struct logtype_changeunnamedroot *UU(l), struct backward_scan_state *UU(bs)) {
    return 0;
}

static void
toku_recover_changenamedroot (LSN UU(lsn), FILENUM UU(filenum), BYTESTRING UU(name), BLOCKNUM UU(oldroot), BLOCKNUM UU(newroot)) { assert(0); }
static int toku_recover_backward_changenamedroot (struct logtype_changenamedroot *UU(l), struct backward_scan_state *UU(bs)) {
    return 0;
}

static int toku_recover_begin_checkpoint (LSN UU(lsn)) {
    return 0;
}

static int toku_recover_backward_begin_checkpoint (struct logtype_begin_checkpoint *l, struct backward_scan_state *bs) {
    switch (bs->bs) {
    case BS_INIT:
	return 0; // incomplete checkpoint
    case BS_SAW_CKPT_END:
	assert(bs->checkpoint_lsn.lsn == l->lsn.lsn);
	bs->bs = BS_SAW_CKPT;
	if (bs->n_live_txns==0) {
	    fprintf(stderr, "Turning around at begin_checkpoint %" PRIu64 "\n", l->lsn.lsn);
	    return 1;
	}
	else return 0;
    case BS_SAW_CKPT:
	return 0; // ignore it
    }
    fprintf(stderr, "%s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)bs->bs);
    abort();
}


static int toku_recover_end_checkpoint (LSN UU(lsn), TXNID UU(txnid)) {
    return 0;
}

static int toku_recover_backward_end_checkpoint (struct logtype_end_checkpoint *l, struct backward_scan_state *bs) {
    switch (bs->bs) {
    case BS_INIT:
	bs->bs = BS_SAW_CKPT_END;
	bs->checkpoint_lsn.lsn = l->txnid;
	return 0;
    case BS_SAW_CKPT_END:
	fprintf(stderr, "%s:%d Should not see two end_checkpoint log entries without an intervening begin_checkpoint\n", __FILE__, __LINE__);
	abort();
    case BS_SAW_CKPT:
	return 0;
    }
    fprintf(stderr, "%s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)bs->bs);
    abort();
}

// going backward we open it, so going forward we don't have to do anything
static int toku_recover_fassociate (LSN UU(lsn), FILENUM UU(filenum), BYTESTRING UU(fname)) {
    return 0;
}

static int toku_recover_backward_fassociate (struct logtype_fassociate *l, struct backward_scan_state *UU(bs)) {
    char *fixedfname = fixup_fname(&l->fname);
    toku_free_BYTESTRING(l->fname);
    internal_toku_recover_fopen_or_fcreate(0, 0, fixedfname, l->filenum);
    return 0;
}

static int toku_recover_xstillopen (LSN UU(lsn), TXNID UU(txnid)) {
    return 0;
}

static int toku_recover_backward_xstillopen (struct logtype_xstillopen *l, struct backward_scan_state *bs) {
    switch (bs->bs) {
    case BS_INIT:
	return 0; // ignore live txns from incomplete checkpoint
    case BS_SAW_CKPT_END:
	if (bs->n_live_txns==0) {
	    bs->min_live_txn = l->lsn;
	} else {
	    if (bs->min_live_txn.lsn > l->txnid)  bs->min_live_txn = l->lsn;
	}
	bs->n_live_txns++;
	return 0;
    case BS_SAW_CKPT:
	return 0; // ignore live txns from older checkpoints
    }
    fprintf(stderr, "%s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)bs->bs);
    abort();
}


static int toku_recover_xbegin (LSN UU(lsn), TXNID UU(parent)) {
    return 0;
}

static int toku_recover_backward_xbegin (struct logtype_xbegin *l, struct backward_scan_state *bs) {
    switch (bs->bs) {
    case BS_INIT:
	return 0; // ignore txns that began after checkpoint
    case BS_SAW_CKPT_END:
	return 0; // ignore txns that began during the checkpoint
    case BS_SAW_CKPT:
	assert(bs->n_live_txns > 0); // the only thing we are doing here is looking for a live txn, so there better be one
	// If we got to the min, return nonzero
	if (bs->min_live_txn.lsn >= l->lsn.lsn) {
	    fprintf(stderr, "Turning around at xbegin %" PRIu64 "\n", l->lsn.lsn);
	    return 1;
	} else {
	    fprintf(stderr, "scanning back at xbegin %" PRIu64 " (looking for %" PRIu64 "\n", l->lsn.lsn, bs->min_live_txn.lsn);
	    return 0;
	}
    }
    fprintf(stderr, "%s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)bs->bs);
    abort();
}

static int toku_delete_rolltmp_files (const char *log_dir) {
    struct dirent *de;
    DIR *d = opendir(log_dir);
    if (d==0) {
	return errno;
    }
    int result = 0;
    while ((de=readdir(d))) {
	char rolltmp_prefix[] = "__rolltmp.";
	int r = memcmp(de->d_name, rolltmp_prefix, sizeof(rolltmp_prefix) - 1);
	if (r==0) {
	    int fnamelen = strlen(log_dir) + strlen(de->d_name) + 2; // One for the slash and one for the trailing NUL.
	    char fname[fnamelen];
	    int l = snprintf(fname, fnamelen, "%s/%s", log_dir, de->d_name);
	    assert(l+1 == fnamelen);
	    r = unlink(fname);
	    if (r!=0) {
		result = errno;
		perror("Trying to delete a rolltmp file");
	    }
	}
    }
    {
	int r = closedir(d);
	if (r==-1) return errno;
    }
    return result;
}

int tokudb_recover(const char *data_dir, const char *log_dir) {
    int failresult = 0;
    int r;
    int entrycount=0;
    char **logfiles;

    int lockfd;

    {
        const char fname[] = "/__recoverylock_dont_delete_me";
	int namelen=strlen(data_dir);
	char lockfname[namelen+sizeof(fname)];

	int l = snprintf(lockfname, sizeof(lockfname), "%s%s", data_dir, fname);
	assert(l+1 == (signed)(sizeof(lockfname)));
        lockfd = toku_os_lock_file(lockfname);
	if (lockfd<0) {
	    printf("Couldn't run recovery because some other process holds the recovery lock %s\n", lockfname);
	    return errno;
	}
    }

    r = toku_delete_rolltmp_files(log_dir);
    if (r!=0) { failresult=r; goto fail; }

    int n_logfiles;
    r = toku_logger_find_logfiles(log_dir, &logfiles, &n_logfiles);
    if (r!=0) { failresult=r; goto fail; }
    int i;
    toku_recover_init();
    char org_wd[1000];
    {
	char *wd=getcwd(org_wd, sizeof(org_wd));
	assert(wd!=0);
	//printf("%s:%d org_wd=\"%s\"\n", __FILE__, __LINE__, org_wd);
    }
    char data_wd[1000];
    {
	r=chdir(data_dir); assert(r==0);
	char *wd=getcwd(data_wd, sizeof(data_wd));
	assert(wd!=0);
	//printf("%s:%d data_wd=\"%s\"\n", __FILE__, __LINE__, data_wd);
    }

    LSN lastlsn = ZERO_LSN;
    FILE *f = NULL;
    for (i=0; i<n_logfiles; i++) {
	if (f) fclose(f);
	r=chdir(org_wd);
	assert(r==0);
	char *logfile = logfiles[n_logfiles-i-1];
	f = fopen(logfile, "r");
	assert(f);
	printf("Opened %s\n", logfiles[n_logfiles-i-1]);
	r = fseek(f, 0, SEEK_END); assert(r==0);
	struct log_entry le;
	struct backward_scan_state bs = initial_bss;
	r=chdir(data_wd);
	assert(r==0);
	while (1) {
	    r = toku_log_fread_backward(f, &le);
	    if (r==-1) break; // Ran out of file
            LSN thislsn = toku_log_entry_get_lsn(&le);
            if (lastlsn.lsn != 0) {
                if (thislsn.lsn != lastlsn.lsn - 1)
                    printf("bw lastlsn=%"PRId64" lsn=%"PRId64"\n", lastlsn.lsn, thislsn.lsn);
                //assert(thislsn.lsn == lastlsn.lsn - 1);
            }
            lastlsn = thislsn;
	    logtype_dispatch_assign(&le, toku_recover_backward_, r, &bs);
	    if (r!=0) goto go_forward;
	}
    }
    i--;
    // We got to the end of the last log
    if (f) { r=fclose(f); assert(r==0); }

    // Now we go forward from this point
    while (n_logfiles-i-1<n_logfiles) {
	//fprintf(stderr, "Opening %s\n", logfiles[i]);
	int j = n_logfiles-i-1;
	r=chdir(org_wd);
	assert(r==0);
	f = fopen(logfiles[j], "r");
	assert(f);
	struct log_entry le;
	u_int32_t version;
	//printf("Reading file %d: %s\n", j, logfiles[j]);
	r=toku_read_and_print_logmagic(f, &version);
	assert(r==0 && version==0);
    go_forward: // we have an open file, so go forward.
	//printf("Going forward\n");
	r=chdir(data_wd);
	assert(r==0);
	while ((r = toku_log_fread(f, &le))==0) {
	    //printf("doing %c\n", le.cmd);
            LSN thislsn = toku_log_entry_get_lsn(&le);
            if (lastlsn.lsn != thislsn.lsn) {
                printf("fw expectlsn=%"PRId64" lsn=%"PRId64"\n", lastlsn.lsn, thislsn.lsn);
            }
            // assert(lastlsn.lsn == thislsn.lsn);
            lastlsn.lsn += 1;

	    logtype_dispatch_args(&le, toku_recover_);
	    entrycount++;
	}
	if (r!=EOF) {
	    if (r==DB_BADFORMAT) {
		fprintf(stderr, "Bad log format at record %d\n", entrycount);
		return r;
	    } else {
		fprintf(stderr, "Huh? %s\n", strerror(r));
		return r;
	    }
	}
	fclose(f);
	i--;
    }
    toku_recover_cleanup();
    for (i=0; logfiles[i]; i++) {
	toku_free(logfiles[i]);
    }
    toku_free(logfiles);

    r=toku_os_unlock_file(lockfd);
    if (r!=0) return errno;

    r=chdir(org_wd);
    if (r!=0) return errno;

    //printf("%s:%d recovery successful! ls -l says\n", __FILE__, __LINE__);
    //system("ls -l");
    return 0;
 fail:
    toku_os_unlock_file(lockfd);
    chdir(org_wd);
    return failresult;
}
