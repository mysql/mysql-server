#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <toku_assert.h>
#include <db.h>
#include "brttypes.h"

typedef struct brtloader_s *BRTLOADER;
int toku_brt_loader_open (BRTLOADER *bl, generate_row_for_put_func g, DB *src_db, int N, DB *dbs[], const char *temp_file_template);
int toku_brt_loader_put (BRTLOADER bl, DBT *key, DBT *val);
int toku_brt_loader_close (BRTLOADER bl);

struct brtloader_s {
    int panic;

    generate_row_for_put_func generate_row_for_put;

    DB *src_db;
    int N;
    DB **dbs;

    const char *temp_file_template;
    FILE *fprimary_rows;  char *fprimary_rows_name;
    FILE *fprimary_idx;   char *fprimary_idx_name;
    u_int64_t fprimary_offset;
};

static int open_temp_file (BRTLOADER bl, FILE **filep, char **fnamep) {
    char *fname = toku_strdup(bl->temp_file_template);
    int fd = mkstemp(fname);
    if (fd<0) { int r = errno; toku_free(fname); return r; }
    FILE *f = fdopen(fd, "r+");
    if (f==NULL) { int r = errno; toku_free(fname); close(fd); return r; }
    *filep = f;
    *fnamep = fname;
    return 0;
}

int toku_brt_loader_open (BRTLOADER *blp, generate_row_for_put_func g, DB *src_db, int N, DB*dbs[], const char *temp_file_template) {
    BRTLOADER MALLOC(bl);
    bl->panic = 0;

    bl->generate_row_for_put = g;

    bl->src_db = src_db;
    bl->N = N;
    MALLOC_N(N, bl->dbs);
    for (int i=0; i<N; i++) bl->dbs[i]=dbs[i];

    bl->temp_file_template = toku_strdup(temp_file_template);
    bl->fprimary_rows = bl->fprimary_idx = NULL;
    { int r = open_temp_file(bl, &bl->fprimary_rows, &bl->fprimary_rows_name); if (r!=0) return r; }
    { int r = open_temp_file(bl, &bl->fprimary_idx,  &bl->fprimary_idx_name);  if (r!=0) return r; }
    bl->fprimary_offset = 0;
    *blp = bl;
    return 0;
}

#define handle_ferror(ok, file) if (!(ok)) { bl->panic=1; bl->panic_errno = ferror(file); return bl->panic_errno; }

static int bl_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream, BRTLOADER bl) {
    size_t r = fwrite(ptr, size, nmemb, stream);
    if (r!=nmemb) {
	int e = ferror(stream);
	assert(e!=0);
	bl->panic = 1;
	return e;
    }
    return 0;
}

static int bl_fread (void *ptr, size_t size, size_t nmemb, FILE *stream, BRTLOADER bl) {
    size_t r = fread(ptr, size, nmemb, stream);
    if (r==0) {
	if (feof(stream)) return EOF;
	else {
	    bl->panic=1;
	    return ferror(stream);
	}
    } else if (r<nmemb) {
	bl->panic=1;
	return ferror(stream);
    } else {
	return 0;
    }
}

static int loader_write_row(DBT *key, DBT *val, FILE *data, FILE *idx, u_int64_t *dataoff, BRTLOADER bl) {
    int klen = key->size;
    int vlen = val->size;
    int r;
    // we have a chance to handle the errors because when we close we can delete all the files.
    if ((r=bl_fwrite(&klen,     sizeof(klen),     1, data, bl))) return r;
    if ((r=bl_fwrite(key->data,            1,  klen, data, bl))) return r;
    if ((r=bl_fwrite(&vlen,     sizeof(vlen),     1, data, bl))) return r;
    if ((r=bl_fwrite(val->data,            1,  vlen, data, bl))) return r;
    if ((r=bl_fwrite(dataoff,   sizeof(*dataoff), 1,  idx, bl))) return r;
    int sum = klen+vlen+sizeof(klen)+sizeof(vlen);
    if ((r=bl_fwrite(&sum,      sizeof(sum),      1,  idx, bl))) return r;
    (*dataoff) += sum;
    return 0;
}

int toku_brt_loader_put (BRTLOADER bl, DBT *key, DBT *val) {
    if (bl->panic) return EINVAL; // previous panic
    return loader_write_row(key, val, bl->fprimary_rows, bl->fprimary_idx, &bl->fprimary_offset, bl);
}

static int loader_read_row (FILE *f, DBT *key, DBT *val, BRTLOADER bl) {
    int r;
    int klen,vlen;
    if ((r = bl_fread(&klen, sizeof(klen), 1, f, bl))) return r;
    assert(klen>=0);
    if ((int)key->ulen<klen) { key->ulen=klen; key->data=toku_xrealloc(key->data, klen); }
    if ((r = bl_fread(key->data, 1, klen, f, bl)))     return r;

    if ((r = bl_fread(&vlen, sizeof(vlen), 1, f, bl))) return r;
    assert(vlen>=0);
    if ((int)val->ulen<vlen) { val->ulen=vlen; val->data=toku_xrealloc(val->data, vlen); }
    if ((r = bl_fread(val->data, 1, vlen, f, bl)))     return r;

    return 0;
}


static int loader_do_i (BRTLOADER bl, DB *dest_db) {
    int r = fseek(bl->fprimary_rows, 0, SEEK_SET);
    assert(r==0);
    DBT pkey={.data=0, .flags=DB_DBT_REALLOC, .size=0, .ulen=0};
    DBT pval=pkey;
    DBT skey=pkey;
    DBT sval=pkey;
    FILE *sfile, *sidx;
    char *sfilename, *sidxname;
    u_int64_t soffset=0;
    r = open_temp_file(bl, &sfile, &sfilename); if (r!=0) return r;
    r = open_temp_file(bl, &sidx,  &sidxname);  if (r!=0) return r;
    while (0==(r=loader_read_row(bl->fprimary_rows, &pkey, &pval, bl))) {
	r = bl->generate_row_for_put(bl->src_db, dest_db, &skey, &sval, &pkey, &pval, NULL);
	assert(r==0);
	r = loader_write_row(&skey, &sval, sfile, sidx, &soffset, bl);
	if (r!=0) return r; // TODO: erase the files
    }
    return 0;
}

int toku_brt_loader_close (BRTLOADER bl) {
    for (int i=0; i<bl->N; i++) {
	int r = loader_do_i(bl, bl->dbs[i]);
	if (r!=0) return r;
    }
    return 0;
}
