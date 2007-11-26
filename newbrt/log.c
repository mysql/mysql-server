#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "brt-internal.h"
#include "log-internal.h"
#include "wbuf.h"
#include "memory.h"
#include "../src/ydb-internal.h"
#include "log_header.h"

int tokulogger_find_next_unused_log_file(const char *directory, long long *result) {
    DIR *d=opendir(directory);
    long long max=-1;
    struct dirent *de;
    if (d==0) return errno;
    while ((de=readdir(d))) {
	if (de==0) return errno;
	long long thisl;
	int r = sscanf(de->d_name, "log%llu.tokulog", &thisl);
	if (r==1 && thisl>max) max=thisl;
    }
    *result=max+1;
    int r = closedir(d);
    return r;
}

int tokulogger_find_logfiles (const char *directory, int *n_resultsp, char ***resultp) {
    int result_limit=1;
    int n_results=0;
    char **MALLOC_N(result_limit, result);
    struct dirent *de;
    DIR *d=opendir(directory);
    if (d==0) return errno;
    int dirnamelen = strlen(directory);
    while ((de=readdir(d))) {
	if (de==0) return errno;
	long long thisl;
	int r = sscanf(de->d_name, "log%llu.tokulog", &thisl);
	if (r!=1) continue; // Skip over non-log files.
	if (n_results>=result_limit) {
	    result_limit*=2;
	    result = toku_realloc(result, result_limit*sizeof(*result));
	}
	int fnamelen = dirnamelen + strlen(de->d_name) + 2; // One for the slash and one for the trailing NUL.
	char *fname = toku_malloc(fnamelen);
	snprintf(fname, fnamelen, "%s/%s", directory, de->d_name);
	result[n_results++] = fname;
    }
    *n_resultsp = n_results;
    *resultp    = result;
    return closedir(d);
}

int tokulogger_create_and_open_logger (const char *directory, TOKULOGGER *resultp) {
    TAGMALLOC(TOKULOGGER, result);
    if (result==0) return -1;
    int r;
    long long nexti;
    r = tokulogger_find_next_unused_log_file(directory, &nexti);
    if (r!=0) {
    died0:
	toku_free(result);
	return r;
    }
    result->directory = toku_strdup(directory);
    if (result->directory==0) goto died0;
    result->fd = -1;
    result->next_log_file_number = nexti;
    result->n_in_buf = 0;

    result->lsn.lsn = 0; // WRONG!!!  This should actually be calculated by looking at the log file. 

    *resultp=result;
    return tokulogger_log_bytes(result, 0, "");
}

int log_format_version=0;

int tokulogger_log_bytes(TOKULOGGER logger, int nbytes, void *bytes) {
    int r;
    //fprintf(stderr, "%s:%d logging %d bytes\n", __FILE__, __LINE__, nbytes);
    if (logger->fd==-1) {
	int  fnamelen = strlen(logger->directory)+50;
	char fname[fnamelen];
	snprintf(fname, fnamelen, "%s/log%012llu.tokulog", logger->directory, logger->next_log_file_number);
	//fprintf(stderr, "%s:%d creat(%s, ...)\n", __FILE__, __LINE__, fname);
	logger->fd = creat(fname, O_EXCL | 0700);
	if (logger->fd==-1) return errno;
	logger->next_log_file_number++;
	int version_l = htonl(log_format_version);
	r = write(logger->fd, "tokulogg", 8); if (r!=8) return errno;
	r = write(logger->fd, &version_l, 4); if (r!=4) return errno;
    }
    if (logger->n_in_buf + nbytes > LOGGER_BUF_SIZE) {
	struct iovec v[2];
	v[0].iov_base = logger->buf;
	v[0].iov_len  = logger->n_in_buf;
	v[1].iov_base = bytes;
	v[1].iov_len  = nbytes;
	//fprintf(stderr, "%s:%d flushing log due to buffer overflow\n", __FILE__, __LINE__);
	r=writev(logger->fd, v, 2);
	if (r!=logger->n_in_buf + nbytes) return errno;
	logger->n_in_file += logger->n_in_buf+nbytes;
	logger->n_in_buf=0;
	if (logger->n_in_file > 100<<20) {
	    fprintf(stderr, "%s:%d closing logfile\n", __FILE__, __LINE__);
	    r = close(logger->fd);
	    if (r!=0) return errno;
	    logger->fd=-1;
	    logger->n_in_file = 0;
	}
    } else {
	memcpy(logger->buf+logger->n_in_buf, bytes, nbytes);
	logger->n_in_buf += nbytes;
    }
    return 0;
}

int tokulogger_log_close(TOKULOGGER *loggerp) {
    TOKULOGGER logger = *loggerp;
    int r = 0;
    if (logger->fd!=-1) {
	//printf("%s:%d closing log: n_in_buf=%d\n", __FILE__, __LINE__, logger->n_in_buf);
	if (logger->n_in_buf>0) {
	    r = write(logger->fd, logger->buf, logger->n_in_buf);
	    if (r==-1) return errno;
	}
	r = close(logger->fd);
    }
    toku_free(logger->directory);
    toku_free(logger);
    *loggerp=0;
    return r;
}
#if 0
int tokulogger_log_brt_remove (TOKULOGGER logger,
			       TXNID txnid,
			       diskoff diskoff,
			       unsigned char *key,
			       int keylen,
			       unsigned char *val,
			       int vallen) {
n    
}
#endif

int tokulogger_fsync (TOKULOGGER logger) {
    //return 0;/// NO TXN
    //fprintf(stderr, "%s:%d syncing log\n", __FILE__, __LINE__);
    if (logger->n_in_buf>0) {
	int r = write(logger->fd, logger->buf, logger->n_in_buf);
	if (r==-1) return errno;
	logger->n_in_buf=0;
    }
    {
	int r = fsync(logger->fd);
	if (r!=0) return errno;
    }
    return 0;
}

int tokulogger_finish (TOKULOGGER logger, struct wbuf *wbuf) {
    wbuf_int(wbuf, toku_crc32(0, wbuf->buf, wbuf->ndone));
    wbuf_int(wbuf, 4+wbuf->ndone);
    return tokulogger_log_bytes(logger, wbuf->ndone, wbuf->buf);
}

// Log an insertion of a key-value pair into a particular node of the tree.
int tokulogger_log_brt_insert_with_no_overwrite (TOKULOGGER logger,
						 TXNID txnid,
						 FILENUM fileid,
						 DISKOFF diskoff,
						 unsigned char *key,
						 int keylen,
						 unsigned char *val,
						 int vallen) {
    printf("%s:%d\n", __FILE__, __LINE__);
    return 0;
    int buflen=(keylen+vallen+4+4 // key and value
		+1 // command
		+8 // lsn
		+8 // txnid
		+4 // fileid
		+8 // diskoff
		+8 // crc and len
		);
    unsigned char buf[buflen];
    struct wbuf wbuf;
    wbuf_init(&wbuf, buf, buflen) ;
    wbuf_char(&wbuf, LT_INSERT_WITH_NO_OVERWRITE);
    wbuf_LSN (&wbuf, logger->lsn); logger->lsn.lsn++;
    wbuf_TXNID(&wbuf, txnid);
    wbuf_FILENUM(&wbuf, fileid);
    wbuf_DISKOFF(&wbuf, diskoff);
    wbuf_bytes(&wbuf, key, keylen);
    wbuf_bytes(&wbuf, val, vallen);
    return tokulogger_finish (logger, &wbuf);
}

int tokulogger_log_phys_add_or_delete_in_leaf (DB *db, TOKUTXN txn, DISKOFF diskoff, int is_add, const struct kv_pair *pair) {
    if (is_add && txn) {
	BYTESTRING key  = { pair->keylen, (char*)kv_pair_key_const(pair) };
	BYTESTRING data = { pair->vallen, (char*)kv_pair_val_const(pair) };
	//printf("Logging insertinleaf\n");
	return toku_log_insertinleaf (txn, toku_txn_get_txnid(txn), db->i->fileid, diskoff, key, data);
    }
    assert(0);
    if (txn==0) return 0;
    assert(db);
    int keylen = pair->keylen;
    int vallen = pair->vallen;
    const int buflen=(keylen+vallen+4+4 // the key and value
		      +1 // log command
		      +8 // lsn
		      +8 // txnid
		      +8 // fileid
		      +8 // diskoff
		      +8 // crc & len
		      );
    unsigned char buf[buflen];
    struct wbuf wbuf;
    wbuf_init(&wbuf, buf, buflen) ;
    wbuf_char(&wbuf, is_add ? LT_INSERT_WITH_NO_OVERWRITE : LT_DELETE);
    wbuf_LSN (&wbuf, txn->logger->lsn);
    txn->logger->lsn.lsn++;
    wbuf_TXNID(&wbuf, txn->txnid64);
    wbuf_FILENUM(&wbuf, db->i->fileid);
    wbuf_DISKOFF(&wbuf, diskoff);
    wbuf_bytes(&wbuf, kv_pair_key_const(pair), keylen);
    wbuf_bytes(&wbuf, kv_pair_val_const(pair), vallen);
    return tokulogger_finish(txn->logger, &wbuf);
}

int tokulogger_commit (TOKUTXN txn) {
    int r = toku_log_commit(txn, txn->txnid64);
    toku_free(txn);
    return r;
}

int tokulogger_log_checkpoint (TOKULOGGER logger, LSN *lsn) {
    struct wbuf wbuf;
    const int buflen =10;
    unsigned char buf[buflen];
    wbuf_init(&wbuf, buf, buflen);
    wbuf_char(&wbuf, LT_CHECKPOINT);
    wbuf_LSN (&wbuf, logger->lsn);
    *lsn = logger->lsn;
    logger->lsn.lsn++;
    return tokulogger_log_bytes(logger, wbuf.ndone, wbuf.buf);
    
}

int tokutxn_begin (TOKUTXN parent_tokutxn, TOKUTXN *tokutxn, TXNID txnid64, TOKULOGGER logger) {
    TAGMALLOC(TOKUTXN, result);
    if (result==0) return errno;
    result->txnid64 = txnid64;
    result->logger = logger;
    result->parent = parent_tokutxn;
    *tokutxn = result;
    return 0;
}

int tokulogger_log_block_rename (TOKULOGGER logger, FILENUM fileid, DISKOFF olddiskoff, DISKOFF newdiskoff, DISKOFF parentdiskoff, int childnum) {
    const int buflen=(+1 // log command
		      +8 // lsn
		      +8 // fileid
		      +8 // olddiskoff
		      +8 // newdiskoff
		      +8 // parentdiskoff
		      +4 // childnum
		      +8 // crc & len
		      );
    unsigned char buf[buflen];
    struct wbuf wbuf;
    wbuf_init   (&wbuf, buf, buflen) ;
    wbuf_char   (&wbuf, LT_BLOCK_RENAME);
    wbuf_LSN    (&wbuf, logger->lsn);
    logger->lsn.lsn++;
    wbuf_FILENUM(&wbuf, fileid);
    wbuf_DISKOFF(&wbuf, olddiskoff);
    wbuf_DISKOFF(&wbuf, newdiskoff);
    wbuf_DISKOFF(&wbuf, parentdiskoff);
    wbuf_int    (&wbuf, childnum);
    return tokulogger_finish(logger, &wbuf);
}

int tokulogger_log_fcreate (TOKUTXN txn, const char *fname, int mode) {
    BYTESTRING bs;
    bs.len = strlen(fname);
    bs.data = (char*)fname;
    return toku_log_fcreate (txn, toku_txn_get_txnid(txn), bs, mode);
}

/* fopen isn't really an action.  It's just for bookkeeping.  We need to know the filename that goes with a filenum. */
int tokulogger_log_fopen (TOKUTXN txn, const char * fname, FILENUM filenum) {
    BYTESTRING bs;
    bs.len = strlen(fname);
    bs.data = (char*)fname;
    return toku_log_fopen (txn,toku_txn_get_txnid(txn), bs, filenum);
    
}


int tokulogger_log_unlink (TOKUTXN txn, const char *fname) {
    if (txn==0) return 0;
    const int fnamelen = strlen(fname);
    const int buflen = (+1 // log command
			+4 // length of fname
			+fnamelen
			+8 // crc & len
			);
    unsigned char buf[buflen];
    struct wbuf wbuf;
    wbuf_init (&wbuf, buf, buflen);
    wbuf_char (&wbuf, LT_UNLINK);
    wbuf_bytes(&wbuf, fname, fnamelen);
    return tokulogger_finish(txn->logger, &wbuf);
};

int tokulogger_log_header (TOKUTXN txn, FILENUM filenum, struct brt_header *h) {
#if 0
    LOGGEDBRTHEADER lh;
    lh.size = toku_serialize_brt_header_size(h);
    lh.flags = h->flags;
    lh.nodesize = h->nodesize;
    lh.freelist = h->freelist;
    lh.unused_memory = h->unused_memory;
    lh.n_named_roots = h->n_named_roots;
    if (h->n_named_roots==-1) {
	lh.u.one.root = h->unnamed_root;
    } else {
	int i;
	MALLOC_N(h->n_named_roots, lh.u.many.names);
	MALLOC_N(h->n_named_roots, lh.u.many.roots);
	for (i=0; i<h->n_named_roots; i++) {
	    lh.u.many.names[i]=toku_strdup(h->names[i]);
	    lh.u.many.roots[i]=h->roots[i];
	}
    }
    r = toku_log_fheader(txn, toku_txn_get_txnid(txn), filenum, lh);
    toku_free(all_that_stuff);
    return r;
#else
    if (txn==0) return 0;
    int subsize=toku_serialize_brt_header_size(h);
    int buflen = (4   // firstlen
		  + 1 //cmd
		  + 8 // lsn
		  + 8 // txnid
		  + 4 // filenum
		  + subsize
		  + 8 // crc & len
		  );
    unsigned char *buf=toku_malloc(buflen); // alloc on heap because it might be big
    int r;
    if (buf==0) return errno;
    struct wbuf wbuf;
    wbuf_init(&wbuf, buf, buflen);
    wbuf_int (&wbuf, buflen);
    wbuf_char(&wbuf, LT_FHEADER);
    wbuf_LSN    (&wbuf, txn->logger->lsn);
    txn->logger->lsn.lsn++;
    wbuf_TXNID(&wbuf, txn->txnid64);
    wbuf_FILENUM(&wbuf, filenum);
    r=toku_serialize_brt_header_to_wbuf(&wbuf, h);
    if (r!=0) return r;
    r=tokulogger_finish(txn->logger, &wbuf);
    toku_free(buf);
    return r;
#endif
}

/*
int brtenv_checkpoint (BRTENV env) {
    init the checkpointing lock
    acquire_spinlock(&env->checkpointing);
    release_spinlock(&env->checkpointing);
    return -1;
}
*/

int toku_fread_u_int8_t_nocrclen (FILE *f, u_int8_t *v) {
    int vi=fgetc(f);
    if (vi==EOF) return -1;
    u_int8_t vc=vi;
    *v = vc;
    return 0;
}

int toku_fread_u_int8_t (FILE *f, u_int8_t *v, u_int32_t *crc, u_int32_t *len) {
    int vi=fgetc(f);
    if (vi==EOF) return -1;
    u_int8_t vc=vi;
    (*crc) = toku_crc32(*crc, &vc, 1);
    (*len)++;
    *v = vc;
    return 0;
}

int toku_fread_u_int32_t_nocrclen (FILE *f, u_int32_t *v) {
    u_int8_t c0,c1,c2,c3;
    int r;
    r = toku_fread_u_int8_t_nocrclen (f, &c0); if (r!=0) return r;
    r = toku_fread_u_int8_t_nocrclen (f, &c1); if (r!=0) return r;
    r = toku_fread_u_int8_t_nocrclen (f, &c2); if (r!=0) return r;
    r = toku_fread_u_int8_t_nocrclen (f, &c3); if (r!=0) return r;
    *v = ((c0<<24)|
	  (c1<<16)|
	  (c2<< 8)|
	  (c3<<0));
    return 0;
}
int toku_fread_u_int32_t (FILE *f, u_int32_t *v, u_int32_t *crc, u_int32_t *len) {
    u_int8_t c0,c1,c2,c3;
    int r;
    r = toku_fread_u_int8_t (f, &c0, crc, len); if(r!=0) return r;
    r = toku_fread_u_int8_t (f, &c1, crc, len); if(r!=0) return r;
    r = toku_fread_u_int8_t (f, &c2, crc, len); if(r!=0) return r;
    r = toku_fread_u_int8_t (f, &c3, crc, len); if(r!=0) return r;
    *v = ((c0<<24)|
	  (c1<<16)|
	  (c2<< 8)|
	  (c3<<0));
    return 0;
}

int toku_fread_u_int64_t (FILE *f, u_int64_t *v, u_int32_t *crc, u_int32_t *len) {
    u_int32_t v1,v2;
    int r;
    r=toku_fread_u_int32_t(f, &v1, crc, len);    if (r!=0) return r;
    r=toku_fread_u_int32_t(f, &v2, crc, len);    if (r!=0) return r;
    *v = (((u_int64_t)v1)<<32 ) | ((u_int64_t)v2);
    return 0;
}
int toku_fread_LSN     (FILE *f, LSN *lsn, u_int32_t *crc, u_int32_t *len) {
    return toku_fread_u_int64_t (f, &lsn->lsn, crc, len);
}
int toku_fread_FILENUM (FILE *f, FILENUM *filenum, u_int32_t *crc, u_int32_t *len) {
    return toku_fread_u_int32_t (f, &filenum->fileid, crc, len);
}
int toku_fread_DISKOFF (FILE *f, DISKOFF *diskoff, u_int32_t *crc, u_int32_t *len) {
    int r = toku_fread_u_int64_t (f, (u_int64_t*)diskoff, crc, len); // sign conversion will be OK.
    return r;
}
int toku_fread_TXNID   (FILE *f, TXNID *txnid, u_int32_t *crc, u_int32_t *len) {
    return toku_fread_u_int64_t (f, txnid, crc, len);
}
// fills in the bs with malloced data.
int toku_fread_BYTESTRING (FILE *f, BYTESTRING *bs, u_int32_t *crc, u_int32_t *len) {
    int r=toku_fread_u_int32_t(f, (u_int32_t*)&bs->len, crc, len);
    if (r!=0) return r;
    bs->data = toku_malloc(bs->len);
    int i;
    for (i=0; i<bs->len; i++) {
	r=toku_fread_u_int8_t(f, (u_int8_t*)&bs->data[i], crc, len);
	if (r!=0) {
	    toku_free(bs->data);
	    bs->data=0;
	    return r;
	}
    }
    return 0;
}

int toku_fread_LOGGEDBRTHEADER(FILE *f, LOGGEDBRTHEADER *v, u_int32_t *crc, u_int32_t *len) {
    int r;
    r = toku_fread_u_int32_t(f, &v->size,          crc, len); if (r!=0) return r;
    r = toku_fread_u_int32_t(f, &v->flags,         crc, len); if (r!=0) return r;
    r = toku_fread_u_int32_t(f, &v->nodesize,      crc, len); if (r!=0) return r;
    r = toku_fread_DISKOFF  (f, &v->freelist,      crc, len); if (r!=0) return r;
    r = toku_fread_DISKOFF  (f, &v->unused_memory, crc, len); if (r!=0) return r;
    r = toku_fread_u_int32_t(f, &v->n_named_roots, crc, len); if (r!=0) return r;
    assert((signed)v->n_named_roots==-1);
    r = toku_fread_DISKOFF  (f, &v->u.one.root,     crc, len); if (r!=0) return r;
    return 0;
}

int toku_logprint_LSN (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len) {
    LSN v;
    int r = toku_fread_LSN(inf, &v, crc, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%" PRId64, fieldname, v.lsn);
    return 0;
}
int toku_logprint_TXNID (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len) {
    TXNID v;
    int r = toku_fread_TXNID(inf, &v, crc, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%" PRId64, fieldname, v);
    return 0;
}

int toku_logprint_u_int8_t (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len) {
    u_int8_t v;
    int r = toku_fread_u_int8_t(inf, &v, crc, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%d", fieldname, v);
    if (v=='\'') fprintf(outf, "('\'')");
    else if (isprint(v)) fprintf(outf, "('%c')", v);
    else {}/*nothing*/
    return 0;
    
}

int toku_logprint_u_int32_t (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len) {
    u_int32_t v;
    int r = toku_fread_u_int32_t(inf, &v, crc, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%d", fieldname, v);
    return 0;
    
}
int toku_logprint_BYTESTRING (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len) {
    BYTESTRING bs;
    int r = toku_fread_BYTESTRING(inf, &bs, crc, len);
    if (r!=0) return r;
    fprintf(outf, " %s={len=%d data=\"", fieldname, bs.len);
    int i;
    for (i=0; i<bs.len; i++) {
	switch (bs.data[i]) {
	case '"':  fprintf(outf, "\\\""); break;
	case '\\': fprintf(outf, "\\\\"); break;
	case '\n': fprintf(outf, "\\n");  break;
	default:
	    if (isprint(bs.data[i])) fprintf(outf, "%c", bs.data[i]);
	    else fprintf(outf, "\\0%03o", bs.data[i]);
	}
    }
    fprintf(outf, "\"");
    toku_free(bs.data);
    return 0;
}
int toku_logprint_FILENUM (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len) {
    return toku_logprint_u_int32_t(outf, inf, fieldname, crc, len);
    
}
int toku_logprint_DISKOFF (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len) {
    DISKOFF v;
    int r = toku_fread_DISKOFF(inf, &v, crc, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%lld", fieldname, v);
    return 0;
}
int toku_logprint_LOGGEDBRTHEADER (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len) {
    LOGGEDBRTHEADER v;
    int r = toku_fread_LOGGEDBRTHEADER(inf, &v, crc, len);
    if (r!=0) return r;
    fprintf(outf, " %s={size=%d flags=%d nodesize=%d freelist=%lld unused_memory=%lld n_named_roots=%d", fieldname, v.size, v.flags, v.nodesize, v.freelist, v.unused_memory, v.n_named_roots);
    return 0;
    
}

int read_and_print_logmagic (FILE *f, u_int32_t *versionp) {
    {
	char magic[8];
	int r=fread(magic, 1, 8, f);
	if (r!=8) {
	    return DB_BADFORMAT;
	}
	if (memcmp(magic, "tokulogg", 8)!=0) {
	    return DB_BADFORMAT;
	}
    }
    {
	int version;
    	int r=fread(&version, 1, 4, f);
	if (r!=4) {
	    return DB_BADFORMAT;
	}
	printf("tokulog v.%d\n", ntohl(version));
	*versionp=ntohl(version);
    }
    return 0;
}

TXNID toku_txn_get_txnid (TOKUTXN txn) {
    if (txn==0) return 0;
    else return txn->txnid64;
}

LSN toku_txn_get_last_lsn (TOKUTXN txn) {
    return txn->last_lsn;
}
