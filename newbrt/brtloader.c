#include <toku_portability.h>

#if !TOKU_WINDOWS
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <toku_assert.h>
#include <string.h>
#include "zlib.h"
#include <fcntl.h>
#include "x1764.h"

#include "brtloader-internal.h"
#include "brt-internal.h"

int brtloader_open_temp_file (BRTLOADER bl, FILE **filep, char **fnamep)
/* Effect: Open a temporary file in read-write mode.  Save enough information to close and delete the file later.
 * Return value: 0 on success, an error number otherwise.
 */
{
    char *fname = toku_strdup(bl->temp_file_template);
    int fd = mkstemp(fname);
    if (fd<0) { int r = errno; toku_free(fname); return r; }
    FILE *f = fdopen(fd, "r+");
    if (f==NULL) { int r = errno; toku_free(fname); close(fd); return r; }
    *filep = f;
    *fnamep = fname;
    return 0;
}

int toku_brt_loader_open (/* out */ BRTLOADER *blp,
			  generate_row_for_put_func g,
			  DB *src_db,
			  int N, DB*dbs[/*N*/],
			  const struct descriptor *descriptors[/*N*/],
			  const char *new_fnames_in_env[/*N*/],
			  const char *new_fnames_in_cwd[/*N*/],
			  brt_compare_func bt_compare_functions[/*N*/],
			  const char *temp_file_template)
/* Effect: called by DB_ENV->create_loader to create a brt loader.
 * Arguments:
 *   blp                  Return the brt loader here.
 *   g                    The function for generating a row
 *   src_db               The source database.  Needed by g.  May be NULL if that's ok with g.
 *   N                    The number of dbs to create.
 *   dbs                  An array of open databases.  Used by g.  The data will be put in these database.
 *   new_fnames           The file names (these strings are owned by the caller: we make a copy for our own purposes).
 *   temp_file_template   A template suitable for mkstemp()
 * Return value: 0 on success, an error number otherwise.
 */
{
    BRTLOADER XCALLOC(bl);
    bl->panic = 0;
    bl->panic_errno = 0;

    bl->generate_row_for_put = g;

    bl->src_db = src_db;
    bl->N = N;
    MALLOC_N(N, bl->dbs);
    for (int i=0; i<N; i++) bl->dbs[i]=dbs[i];
    MALLOC_N(N, bl->descriptors);
    for (int i=0; i<N; i++) bl->descriptors[i]=descriptors[i];
    MALLOC_N(N, bl->new_fnames_in_env);
    for (int i=0; i<N; i++) bl->new_fnames_in_env[i] = toku_strdup(new_fnames_in_env[i]);
    MALLOC_N(N, bl->new_fnames_in_cwd);
    for (int i=0; i<N; i++) bl->new_fnames_in_cwd[i] = toku_strdup(new_fnames_in_cwd[i]);
    MALLOC_N(N, bl->bt_compare_funs);
    for (int i=0; i<N; i++) bl->bt_compare_funs[i] = bt_compare_functions[i];

    bl->temp_file_template = toku_strdup(temp_file_template);
    bl->fprimary_rows = bl->fprimary_idx = NULL;
    { int r = brtloader_open_temp_file(bl, &bl->fprimary_rows, &bl->fprimary_rows_name); if (r!=0) return r; }
    { int r = brtloader_open_temp_file(bl, &bl->fprimary_idx,  &bl->fprimary_idx_name);  if (r!=0) return r; }
    bl->fprimary_offset = 0;
    *blp = bl;
    return 0;
}


static int bl_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream, BRTLOADER bl)
/* Effect: this is a wrapper for fwrite that returns 0 on success, otherwise returns an error number.
 * Arguments:
 *   ptr    the data to be writen.
 *   size   the amount of data to be written.
 *   nmemb  the number of units of size to be written.
 *   stream write the data here.
 *   bl     passed so we can panic the brtloader if something goes wrong (recording the error number).
 * Return value: 0 on success, an error number otherwise.
 */
{
    size_t r = fwrite(ptr, size, nmemb, stream);
    if (r!=nmemb) {
	int e = ferror(stream);
	assert(e!=0);
	bl->panic       = 1;
	bl->panic_errno = e;
	return e;
    }
    return 0;
}

static int bl_fread (void *ptr, size_t size, size_t nmemb, FILE *stream, BRTLOADER bl)
/* Effect: this is a wrapper for fread that returns 0 on success, otherwise returns an error number.
 * Arguments:
 *  ptr      read data into here.
 *  size     size of data element to be read.
 *  nmemb    number of data elements to be read.
 *  stream   where to read the data from.
 *  bl       passed so we can panic the brtloader if something goes wrong (recording the error number.
 * Return value: 0 on success, an error number otherwise.
 */
{
    size_t r = fread(ptr, size, nmemb, stream);
    if (r==0) {
	if (feof(stream)) return EOF;
	else {
	do_error: ;
	    int e = ferror(stream);
	    bl->panic       = 1;
	    bl->panic_errno = e;
	    return e;
	}
    } else if (r<nmemb) {
	goto do_error;
    } else {
	return 0;
    }
}

static int bl_write_dbt (DBT *dbt, FILE *datafile, uint64_t *dataoff, BRTLOADER bl)
{
    int r;
    int dlen = dbt->size;
    if ((r=bl_fwrite(&dlen,     sizeof(dlen), 1,    datafile, bl))) return r;
    if ((r=bl_fwrite(dbt->data, 1,            dlen, datafile, bl))) return r;
    if (dataoff)
	*dataoff += dlen + sizeof(dlen);
    return 0;
}

static int bl_read_dbt (/*in*/DBT *dbt, FILE *datafile, BRTLOADER bl)
{
    int len;
    {
	int r;
	if ((r = bl_fread(&len, sizeof(len), 1, datafile, bl))) return r;
	assert(len>=0);
    }
    if ((int)dbt->ulen<len) { dbt->ulen=len; dbt->data=toku_xrealloc(dbt->data, len); }
    {
	int r;
	if ((r = bl_fread(dbt->data, 1, len, datafile, bl)))     return r;
    }
    dbt->size = len;
    return 0;
}

int loader_write_row(DBT *key, DBT *val, FILE *data, FILE *idx, u_int64_t *dataoff, BRTLOADER bl)
/* Effect: Given a key and a val (both DBTs), write them to a file.  Increment *dataoff so that it's up to date.
 * Arguments:
 *   key, val   write these.
 *   data       the file to write them to
 *   dataoff    a pointer to a counter that keeps track of the amount of data written so far.
 *   bl         the brtloader (passed so we can panic if needed).
 * Return value: 0 on success, an error number otherwise.
 */
{
    int klen = key->size;
    int vlen = val->size;
    int r;
    if ((r=bl_fwrite(dataoff,   sizeof(*dataoff), 1,  idx, bl))) return r;
    int sum = klen+vlen+sizeof(klen)+sizeof(vlen);
    if ((r=bl_fwrite(&sum,      sizeof(sum),      1,  idx, bl))) return r;
    // we have a chance to handle the errors because when we close we can delete all the files.
    if ((r=bl_write_dbt(key, data, dataoff, bl))) return r;
    if ((r=bl_write_dbt(val, data, dataoff, bl))) return r;
    return 0;
}

int toku_brt_loader_put (BRTLOADER bl, DBT *key, DBT *val)
/* Effect: Put a key-value pair into the brt loader.  Called by DB_LOADER->put().
 * Return value: 0 on success, an error number otherwise.
 */
{
    if (bl->panic) return EINVAL; // previous panic
    return loader_write_row(key, val, bl->fprimary_rows, bl->fprimary_idx, &bl->fprimary_offset, bl);
}

int loader_read_row (FILE *f, DBT *key, DBT *val, BRTLOADER bl)
/* Effect: Read a key value pair from a file.  The DBTs must have DB_DBT_REALLOC set.
 * Arguments:
 *    f         where to read it from.
 *    key, val  read it into these.
 *    bl        passed so we can panic if needed.
 * Return value: 0 on success, an error number otherwise.
 * Requires:   The DBTs must have DB_DBT_REALLOC
 */
{
    {
	int r = bl_read_dbt(key, f, bl);
	if (r!=0) return r;
    }
    {
	int r = bl_read_dbt(val, f, bl);
	if (r!=0) return r;
    }
    return 0;
}


void init_rowset (struct rowset *rows)
/* Effect: Initialize a collection of rows to be empty. */
{
    rows->n_rows = 0;
    rows->n_rows_limit = 100;
    MALLOC_N(rows->n_rows_limit, rows->rows);
    rows->n_bytes = 0;
    rows->n_bytes_limit = 1024*1024*16;
    rows->data = toku_malloc(rows->n_bytes_limit);
}
void destroy_rowset (struct rowset *rows) {
    toku_free(rows->data);
    toku_free(rows->rows);
}
const size_t data_buffer_limit = 1024*1024*16;
static int row_wont_fit (struct rowset *rows, size_t size)
/* Effect: Return nonzero if adding a row of size SIZE would be too big (bigger than the buffer limit) */ 
{
    return (data_buffer_limit < rows->n_bytes + size);
}
static void reset_rows (struct rowset *rows)
/* Effect: Reset the rows to an empty collection (but reuse any allocated space) */
{
    rows->n_bytes = 0;
    rows->n_rows = 0;
}
void add_row (struct rowset *rows, DBT *key, DBT *val)
/* Effect: add a row to a collection. */
{
    if (rows->n_rows >= rows->n_rows_limit) {
	rows->n_rows_limit *= 2;
	REALLOC_N(rows->n_rows_limit, rows->rows);
    }
    size_t off      = rows->n_bytes;
    size_t next_off = off + key->size + val->size;
    struct row newrow = {.data = rows->data+off,
			 .klen = key->size,
			 .vlen = val->size};
    rows->rows[rows->n_rows++] = newrow;
    if (next_off > rows->n_bytes_limit) {
	while (next_off > rows->n_bytes_limit) {
	    rows->n_bytes_limit = rows->n_bytes_limit*2; 
	}
	REALLOC_N(rows->n_bytes_limit, rows->data);
    }
    memcpy(rows->data+off,           key->data, key->size);
    memcpy(rows->data+off+key->size, val->data, val->size);
    rows->n_bytes = next_off;
}

void merge (struct row dest[/*an+bn*/], struct row a[/*an*/], int an, struct row b[/*bn*/], int bn,
	    DB *dest_db, brt_compare_func compare)
/* Effect: Given two arrays of rows, a and b, merge them using the comparison function, and writ them into dest.
 *   This function is suitable for use in a mergesort.
 * Arguments:
 *   dest    write the rows here
 *   a,b     the rows being merged
 *   an,bn   the lenth of a and b respectively.
 *   dest_db We need the dest_db to run the comparison function.
 *   compare We need the compare function for the dest_db.
 */
{
    while (an>0 && bn>0) {
	DBT akey = {.data=a->data, .size=a->klen};
	DBT bkey = {.data=b->data, .size=b->klen};
	if (compare(dest_db, &akey, &bkey)<0) {
	    // a is smaller
	    *dest = *a;
	    dest++; a++; an--;
	} else {
	    *dest = *b;
	    dest++; b++; bn--;
	}
    }
    while (an>0) {
	*dest = *a;
	dest++; a++; an--;
    }
    while (bn>0) {
	*dest = *b;
	dest++; b++; bn--;
    }
}

void mergesort_row_array (struct row rows[/*n*/], int n, DB *dest_db, brt_compare_func compare)
/* Sort an array of rows (using mergesort).
 * Arguments:
 *   rows   sort this array of rows.
 *   n      the length of the array.
 *   dest_db  used by the comparison function.
 *   compare  the compare function
 */
{
    if (n<=1) return; // base case is sorted
    int mid = n/2;
    mergesort_row_array (rows,     mid,   dest_db, compare);
    mergesort_row_array (rows+mid, n-mid, dest_db, compare);
    struct row *MALLOC_N(n, tmp);
    merge(tmp, rows, mid, rows+mid, n-mid, dest_db, compare);
    memcpy(rows, tmp, sizeof(*tmp)*n);
    toku_free(tmp);
}

static void sort_rows (struct rowset *rows, DB *dest_db, brt_compare_func compare)
/* Effect: Sort a collection of rows.
 * Arguments:
 *   rowset    the */
{
    mergesort_row_array(rows->rows, rows->n_rows, dest_db, compare);
}

/* filesets Maintain a collection of files.  Typically these files are each individually sorted, and we will merge them.
 * These files have two parts, one is for the data rows, and the other is a collection of offsets so we an more easily parallelize the manipulation (e.g., by allowing us to find the offset of the ith row quickly). */

void init_fileset (struct fileset *fs)
/* Effect: Initialize a fileset */ 
{
    fs->n_temp_files = 0;
    fs->n_temp_files_limit = 0;
    fs->temp_data_names = NULL;
    fs->temp_idx_names = NULL;
}

static int extend_fileset (BRTLOADER bl, struct fileset *fs, FILE **ffile, FILE **fidx)
/* Effect: Add two files (one for data and one for idx) to the fileset.
 * Arguments:
 *   bl   the brtloader (needed to panic if anything goes wrong, and also to get the temp_file_template.
 *   fs   the fileset
 *   ffile  the data file (which will be open)
 *   fidx   the index file (which will be open)
 */
{
    char *sfilename, *sidxname;
    FILE *sfile, *sidx;
    int r;
    r = brtloader_open_temp_file(bl, &sfile, &sfilename); if (r!=0) return r;
    r = brtloader_open_temp_file(bl, &sidx,  &sidxname);  if (r!=0) return r;

    if (fs->n_temp_files+1 > fs->n_temp_files_limit) {
	fs->n_temp_files_limit = (fs->n_temp_files+1)*2;
	REALLOC_N(fs->n_temp_files_limit, fs->temp_data_names);
	REALLOC_N(fs->n_temp_files_limit, fs->temp_idx_names);
    }
    fs->temp_data_names[fs->n_temp_files] = sfilename;
    fs->temp_idx_names [fs->n_temp_files] = sidxname;
    fs->n_temp_files++;

    *ffile = sfile;
    *fidx  = sidx;
    return 0;
}

int sort_and_write_rows (struct rowset *rows, struct fileset *fs, BRTLOADER bl, DB *dest_db, brt_compare_func compare)
/* Effect: Given a rowset, sort it and write it to a temporary file.
 * Arguments:
 *   rows    the rowset
 *   fs      the fileset into which the sorted data will be added
 *   bl      the brtloader
 *   dest_db the DB, needed for the comparison function.
 *   compare The comparison function.
 * Returns 0 on success, otherwise an error number.
 */
{
    FILE *sfile, *sidx;
    u_int64_t soffset=0;
    // TODO: erase the files, and deal with all the cleanup on error paths
    int r;
    sort_rows(rows, dest_db, compare);
    r = extend_fileset(bl, fs, &sfile, &sidx);
    if (r!=0) return r;
    for (size_t i=0; i<rows->n_rows; i++) {
	DBT skey = {.data = rows->rows[i].data,                      .size=rows->rows[i].klen};
	DBT sval = {.data = rows->rows[i].data + rows->rows[i].klen, .size=rows->rows[i].vlen};
	r = loader_write_row(&skey, &sval, sfile, sidx, &soffset, bl);
	if (r!=0) return r;
    }
    r = fclose(sfile);  if (r!=0) return errno;
    r = fclose(sidx);   if (r!=0) return errno;
    
    return 0;
}

static int merge_some_files (FILE *dest_data, FILE *dest_idx, int n_sources, FILE *srcs_data[/*n_sources*/], FILE *srcs_idx[/*n_sources*/], BRTLOADER bl, DB *dest_db, brt_compare_func compare)
/* Effect: Given an array of FILE*'s each containing sorted, merge the data and write it to dest.  All the files remain open after the merge.
 *   This merge is performed in one pass, so don't pass too many files in.  If you need a tree of merges do it elsewhere.
 * Modifies:  May modify the arrays of files (but if modified, it must be a permutation so the caller can use that array to close everything.)
 * Requires: The number of sources is at least one, and each of the input files must have at least one row in it.
 * Implementation note: Currently this code uses a really stupid heap O(n) time per pop instead of O(log n), but we'll fix that soon.
 * Arguments:
 *   dest_data    where to write the sorted data
 *   dest_idx     where to write the sorted indexes (the offsets of the rows in dest_data)
 *   n_sources    how many source files.
 *   srcs_data    the array of source data files.
 *   srcs_idx     the array of source index files
 *   bl           the brtloader.
 *   dest_db      the destination DB (used in the comparison function).
 * Return value: 0 on success, otherwise an error number.
 */
{
    // We'll use a really stupid heap:  O(n) time per pop instead of O(log n), because we need to get this working soon. ???
    FILE *datas[n_sources];
    FILE *idxs [n_sources];
    DBT keys[n_sources];
    DBT vals[n_sources];
    u_int64_t dataoff[n_sources];
    DBT zero = {.data=0, .flags=DB_DBT_REALLOC, .size=0, .ulen=0};
    for (int i=0; i<n_sources; i++) {
	datas[i] = srcs_data[i];
	idxs [i] = srcs_idx[i];
	keys[i] = zero;
	vals[i] = zero;
	int r = loader_read_row(datas[i], &keys[i], &vals[i], bl);
	if (r!=0) return r;
	dataoff[i] = 0;
    }
    while (n_sources>0) {
	int mini=0;
	for (int j=1; j<n_sources; j++) {
	    if (compare(dest_db, &keys[mini], &keys[j])>0) {
		mini=j;
	    }
	}
	{
	    int r = loader_write_row(&keys[mini], &vals[mini], dest_data, dest_idx, &dataoff[mini], bl);
	    if (r!=0) return r;
	}
	{
	    int r = loader_read_row(datas[mini], &keys[mini], &vals[mini], bl);
	    if (r!=0) {
		if (feof(datas[mini])) {
		    toku_free(keys[mini].data);
		    toku_free(vals[mini].data);
		    datas[mini] = datas[n_sources-1];
		    idxs [mini] = idxs [n_sources-1];
		    keys[mini] = keys[n_sources-1];
		    vals[mini] = vals[n_sources-1];
		    n_sources--;
		} else {
		    r = ferror(datas[mini]);
		    assert(r!=0);
		    return r;
		}
	    }
	}
    }
    return 0;
}

static int int_min (int a, int b)
{
    if (a<b) return a;
    else return b;
}

int merge_files (struct fileset *fs, BRTLOADER bl, DB *dest_db, brt_compare_func compare)
/* Effect:  Given a fileset, merge all the files into one file.  At the end the fileset will have one file in it.
 *   All the other files will be closed and unlinked.
 * Return value: 0 on success, otherwise an error number.
 */
{
    while (fs->n_temp_files!=1) {
	assert(fs->n_temp_files>0);
	struct fileset next_file_set;
	init_fileset(&next_file_set);
	while (fs->n_temp_files>0) {
	    // grab some files and merge them.
	    const int mergelimit = 256;
	    int n_to_merge = int_min(mergelimit, fs->n_temp_files);
	    FILE **MALLOC_N(n_to_merge, datafiles);
	    FILE **MALLOC_N(n_to_merge, idxfiles);
	    for (int i=0; i<n_to_merge; i++) {
		int idx = fs->n_temp_files -1 -i;
		datafiles[i] = fopen(fs->temp_data_names[idx], "r");    if (datafiles[i]==NULL) return errno;
		idxfiles[i] = fopen(fs->temp_idx_names  [idx], "r");    if (idxfiles[i]==NULL) return errno;
	    }
	    FILE *merged_data, *merged_idx;
	    int r;
	    r = extend_fileset(bl, &next_file_set,  &merged_data, &merged_idx);                                    if (r!=0) return r;
	    r = merge_some_files(merged_data, merged_idx, n_to_merge, datafiles, idxfiles, bl, dest_db, compare);  if (r!=0) return r;
	    for (int i=0; i<n_to_merge; i++) {
		int idx = fs->n_temp_files -1 -i;
		r = fclose(datafiles[i]);              if (r!=0) return errno;
		r = fclose(idxfiles[i]);               if (r!=0) return errno;
		r = unlink(fs->temp_data_names[idx]);  if (r!=0) return errno;
		r = unlink(fs->temp_idx_names [idx]);  if (r!=0) return errno;
		toku_free(fs->temp_data_names[idx]);
		toku_free(fs->temp_idx_names[idx]);
	    }
	    fs->n_temp_files -= n_to_merge;
	    r = fclose(merged_data); assert(r==0);
	    r = fclose(merged_idx);  assert(r==0);
	    toku_free(datafiles);
	    toku_free(idxfiles);
	}
	assert(fs->n_temp_files==0);
	toku_free(fs->temp_data_names);
	toku_free(fs->temp_idx_names);
	*fs = next_file_set;
    }
    return 0;
}

static int loader_do_i (BRTLOADER bl,
			DB *dest_db,
			brt_compare_func compare,
			const struct descriptor *descriptor,
			const char *new_fname)
/* Effect: Handle the file creating for one particular DB in the bulk loader. */
{
    int r = fseek(bl->fprimary_rows, 0, SEEK_SET);
    assert(r==0);
    DBT pkey={.data=0, .flags=DB_DBT_REALLOC, .size=0, .ulen=0};
    DBT pval=pkey;
    DBT skey=pkey;
    DBT sval=pkey;
    struct rowset rows;
    init_rowset(&rows);
    struct fileset fs;
    init_fileset(&fs);
    while (0==(r=loader_read_row(bl->fprimary_rows, &pkey, &pval, bl))) {
	r = bl->generate_row_for_put(dest_db, bl->src_db, &skey, &sval, &pkey, &pval, NULL);
	assert(r==0);

	if (row_wont_fit(&rows, skey.size + sval.size)) {
	    r = sort_and_write_rows(&rows, &fs, bl, dest_db, compare);
	    if (r!=0) return r;
	    reset_rows(&rows);
	}
	add_row(&rows, &skey, &sval);

        //flags==0 means generate_row_for_put callback changed it
        //(and freed any memory necessary to do so) so that values are now stored
        //in temporary memory that does not need to be freed.  We need to continue
        //using DB_DBT_REALLOC however.
        if (skey.flags == 0) {
            toku_init_dbt(&skey);
            skey.flags = DB_DBT_REALLOC;
        }
        if (sval.flags == 0) {
            toku_init_dbt(&sval);
            sval.flags = DB_DBT_REALLOC;
        }
    }
    toku_free (pkey.data);
    toku_free (pval.data);
    //Clean up memory in skey, sval
    if (skey.data) toku_free(skey.data);
    if (sval.data) toku_free(sval.data);
    if (rows.n_rows > 0) {
	r = sort_and_write_rows(&rows, &fs, bl, dest_db, compare);
	if (r!=0) return r;
    }
    toku_free(rows.data);
    toku_free(rows.rows);
    r = merge_files(&fs, bl, dest_db, compare);
    if (r!=0) return r;

    // Now it's down to one file.  Need to write the data out.  The file is in fs.
    mode_t mode = S_IRWXU|S_IRWXG|S_IRWXO;
    int fd = open(new_fname, O_RDWR| O_CREAT | O_BINARY, mode);
    assert(fd>=0);
    assert(fs.n_temp_files==1);
    FILE *inf = fopen(fs.temp_data_names[0], "r");
    r = write_file_to_dbfile(fd, inf, bl, descriptor);
    assert(r==0);
    r = close(fd);
    assert(r==0);
    r = fclose(inf);
    assert(r==0);

    toku_free(fs.temp_data_names[0]);
    toku_free(fs.temp_idx_names[0]);
    toku_free(fs.temp_data_names);
    toku_free(fs.temp_idx_names);

    return 0;
}

int toku_brt_loader_close (BRTLOADER bl)
/* Effect: Close the bulk loader.
 * Return all the file descriptors in the array fds. */
{
    for (int i=0; i<bl->N; i++) {
	int r = loader_do_i(bl, bl->dbs[i], bl->bt_compare_funs[i], bl->descriptors[i], bl->new_fnames_in_cwd[i]);
	if (r!=0) return r;
        toku_free((void*)bl->new_fnames_in_env[i]);
        toku_free((void*)bl->new_fnames_in_cwd[i]);
	bl->new_fnames_in_env[i] = NULL;
	bl->new_fnames_in_cwd[i] = NULL;
    }
    toku_free(bl->dbs);
    toku_free(bl->descriptors);
    toku_free(bl->new_fnames_in_env);
    toku_free(bl->new_fnames_in_cwd);
    toku_free(bl->bt_compare_funs);
    toku_free((void*)bl->temp_file_template);
    { int r = fclose(bl->fprimary_rows); assert (r==0); }
    toku_free(bl->fprimary_rows_name);
    { int r = fclose(bl->fprimary_idx); assert (r==0); }    
    toku_free(bl->fprimary_idx_name);
    return 0;
}

struct dbuf {
    unsigned char *buf;
    int buflen;
    int off;
};

static void dbuf_init (struct dbuf *dbuf) {
    dbuf->buf=0;
    dbuf->buflen=0;
    dbuf->off=0;
}
static void dbuf_destroy (struct dbuf *dbuf) {
    toku_free(dbuf->buf);
}

static void putbuf_bytes (struct dbuf *dbuf, const void *bytes, int nbytes) {
    if (dbuf->off + nbytes > dbuf->buflen) {
	dbuf->buflen += dbuf->off + nbytes;
	dbuf->buflen *= 2;
	REALLOC_N(dbuf->buflen, dbuf->buf);
	assert(dbuf->buf);
    }
    memcpy(dbuf->buf + dbuf->off, bytes, nbytes);
    dbuf->off += nbytes;
}

static void putbuf_int8  (struct dbuf *dbuf, unsigned char v) {
    putbuf_bytes(dbuf, &v, 1);
}

static void putbuf_int32 (struct dbuf *dbuf, int v) {
    putbuf_bytes(dbuf, &v, 4);
}
static void putbuf_int64 (struct dbuf *dbuf, unsigned long long v) {
    putbuf_int32(dbuf, v>>32);
    putbuf_int32(dbuf, v&0xFFFFFFFF);
}

static void putbuf_int32_at(struct dbuf *dbuf, int off, int v) {
    memcpy(dbuf->buf + off, &v, 4);
}
static void putbuf_int64_at(struct dbuf *dbuf, int off, unsigned long long v) {
    unsigned int a = v>>32;
    unsigned int b = v&0xFFFFFFFF;
    putbuf_int32_at(dbuf, off,   a);
    putbuf_int32_at(dbuf, off+4, b);
}

struct leaf_buf {
    int64_t blocknum;
    struct dbuf dbuf;
    unsigned int rand4fingerprint;
    unsigned int local_fingerprint;
    int local_fingerprint_p;
    int nkeys, ndata, dsize, n_in_buf;
    int nkeys_p, ndata_p, dsize_p, n_in_buf_p;
};

const int nodesize = 1<<22;

struct translation {
    int64_t off, size;
};

struct dbout {
    int fd;
    toku_off_t current_off;

    int64_t n_translations;
    int64_t n_translations_limit;
    struct translation *translation;
};

static int64_t allocate_block (struct dbout *out)
// Return the new block number
{
    int64_t result = out->n_translations;
    if (result >= out->n_translations_limit) {
	if (out->n_translations_limit==0) {
	    out->n_translations_limit = 1;
	} else {
	    out->n_translations_limit *= 2;
	}
	REALLOC_N(out->n_translations_limit, out->translation);
    }
    out->n_translations++;
    return result;
}

//#ifndef CILK_STUB
//cilk::mutex *ttable_and_write_lock = new cilk::mutex;
//#endif

static struct leaf_buf *start_leaf (struct dbout *out, const struct descriptor *desc, int64_t lblocknum) {
//#ifndef CILK_STUB
//    ttable_and_write_lock->lock();
//#endif
//#ifndef CILK_STUB
//    ttable_and_write_lock->unlock();
//#endif
    assert(lblocknum < out->n_translations_limit);
    struct leaf_buf *MALLOC(lbuf);
    assert(lbuf);
    lbuf->blocknum = lblocknum;
    dbuf_init(&lbuf->dbuf);
    int height=0;
    int flags=0;
    int layout_version=11;
    putbuf_bytes(&lbuf->dbuf, "tokuleaf", 8);
    putbuf_int32(&lbuf->dbuf, layout_version);
    putbuf_int32(&lbuf->dbuf, layout_version); // layout_version original

    putbuf_int32(&lbuf->dbuf, desc->version); // desc version
    putbuf_int32(&lbuf->dbuf, desc->dbt.size); // desc size
    putbuf_bytes(&lbuf->dbuf, desc->dbt.data, desc->dbt.size);

    putbuf_int32(&lbuf->dbuf, nodesize);
    putbuf_int32(&lbuf->dbuf, flags);
    putbuf_int32(&lbuf->dbuf, height);
    lbuf->rand4fingerprint = random();
    putbuf_int32(&lbuf->dbuf, lbuf->rand4fingerprint);
    lbuf->local_fingerprint = 0;
    lbuf->nkeys = lbuf->ndata = lbuf->dsize = 0;
    lbuf->n_in_buf = 0;
    
    // leave these uninitialized for now.
    lbuf->local_fingerprint_p = lbuf->dbuf.off;    lbuf->dbuf.off+=4;
    lbuf->nkeys_p             = lbuf->dbuf.off;    lbuf->dbuf.off+=8;
    lbuf->ndata_p             = lbuf->dbuf.off;    lbuf->dbuf.off+=8;
    lbuf->dsize_p             = lbuf->dbuf.off;    lbuf->dbuf.off+=8;
    lbuf->n_in_buf_p          = lbuf->dbuf.off;    lbuf->dbuf.off+=4;

    return lbuf;
}

static void add_pair_to_leafnode (struct leaf_buf *lbuf, unsigned char *key, int keylen, unsigned char *val, int vallen) {
    lbuf->n_in_buf++;
    lbuf->nkeys++; // assume NODUP
    lbuf->ndata++;
    lbuf->dsize+= keylen + vallen;
    
    int le_off = lbuf->dbuf.off;
    putbuf_int8(&lbuf->dbuf, 1);
    putbuf_int32(&lbuf->dbuf, keylen);
    putbuf_int32(&lbuf->dbuf, vallen);
    putbuf_bytes(&lbuf->dbuf, key, keylen);
    putbuf_bytes(&lbuf->dbuf, val, vallen);
    int le_len = 1+4+4+keylen+vallen;
    assert(le_off + le_len == lbuf->dbuf.off);
    u_int32_t this_x = x1764_memory(lbuf->dbuf.buf + le_off, le_len);
    u_int32_t this_prod = lbuf->rand4fingerprint * this_x;
    lbuf->local_fingerprint += this_prod;
    if (0) {
	printf("%s:%d x1764(buf+%d, %d)=%8x\n", __FILE__, __LINE__, le_off, le_len, this_x);
	printf("%s:%d rand4fingerprint=%8x\n", __FILE__, __LINE__, lbuf->rand4fingerprint);
	printf("%s:%d this_prod=%8x\n", __FILE__, __LINE__, this_prod);
	printf("%s:%d local_fingerprint=%8x\n", __FILE__, __LINE__, lbuf->local_fingerprint);
    }
}

static void write_literal(struct dbout *out, void*data,  size_t len) {
    assert(out->current_off%4096==0);
    int r = toku_os_write(out->fd, data, len);
    assert(r==0);
    out->current_off+=len;
}
static void seek_align(struct dbout *out) {
    toku_off_t old_current_off = out->current_off;
    int alignment = 4096;
    out->current_off += alignment-1;
    out->current_off &= ~(alignment-1);
    toku_off_t r = lseek(out->fd, out->current_off, SEEK_SET);
    if (r!=out->current_off) {
	fprintf(stderr, "Seek failed %s (errno=%d)\n", strerror(errno), errno);
    }
    assert(r==out->current_off);
    assert(out->current_off >= old_current_off);
    assert(out->current_off < old_current_off+alignment);
    assert(out->current_off % alignment == 0);
}

static void finish_leafnode (struct dbout *out, struct leaf_buf *lbuf) {
    //printf("local_fingerprint=%8x\n", lbuf->local_fingerprint);
    putbuf_int32_at(&lbuf->dbuf, lbuf->local_fingerprint_p, lbuf->local_fingerprint);
    putbuf_int64_at(&lbuf->dbuf, lbuf->nkeys_p,             lbuf->nkeys);
    putbuf_int64_at(&lbuf->dbuf, lbuf->ndata_p,             lbuf->ndata);
    putbuf_int64_at(&lbuf->dbuf, lbuf->dsize_p,             lbuf->dsize);
    putbuf_int32_at(&lbuf->dbuf, lbuf->n_in_buf_p,          lbuf->n_in_buf);

    u_int32_t checksum = x1764_memory(lbuf->dbuf.buf, lbuf->dbuf.off);
    putbuf_int32(&lbuf->dbuf, checksum);

    //print_bytestring(lbuf->dbuf.buf, lbuf->dbuf.off, 200);

    int n_uncompressed_bytes_at_beginning = (8 // tokuleaf
					     +4 // layout version
					     +4 // layout version original
					     );
    int n_extra_bytes_for_compression = (+4 // n_sub blocks
					 +4 // compressed size
					 +4 // compressed size
					 );
    int compression_level = 1;
    int uncompressed_len = lbuf->dbuf.off - n_uncompressed_bytes_at_beginning;
    int bound = compressBound(uncompressed_len);
    unsigned char *MALLOC_N(bound + n_uncompressed_bytes_at_beginning  + n_extra_bytes_for_compression, compressed_buf);
    uLongf real_compressed_len = bound;
    {
	int r = compress2((Bytef*)(compressed_buf + n_uncompressed_bytes_at_beginning + n_extra_bytes_for_compression), &real_compressed_len,
			  (Bytef*)(lbuf->dbuf.buf + n_uncompressed_bytes_at_beginning), uncompressed_len,
			  compression_level);
	assert(r==Z_OK);
    }
    memcpy(compressed_buf, lbuf->dbuf.buf, n_uncompressed_bytes_at_beginning);
    int compressed_len = real_compressed_len;
    int n_compressed_blocks = 1;
    memcpy(compressed_buf+16, &n_compressed_blocks, 4);
    memcpy(compressed_buf+20, &compressed_len, 4);
    memcpy(compressed_buf+24, &uncompressed_len, 4);

//#ifndef CILK_STUB
//    ttable_and_write_lock->lock();
//#endif
    long long off_of_leaf = out->current_off;
    int size = real_compressed_len + n_uncompressed_bytes_at_beginning + n_extra_bytes_for_compression;
    if (0) {
	fprintf(stderr, "uncompressed buf size=%d (amount of data compressed)\n", uncompressed_len);
	fprintf(stderr, "compressed buf size=%lu, off=%lld\n", real_compressed_len, off_of_leaf);
	fprintf(stderr, "compressed bytes are:");
	//for (int i=0; i<compressed_len; i++) {
	//    unsigned char c = compressed_buf[28+i];
	//    if (isprint(c)) fprintf(stderr, "%c", c);
	//    else fprintf(stderr, "\\%03o", compressed_buf[28+i]);
	//}
	fprintf(stderr, "\ntotal bytes written = %d, last byte is \\%o\n", size, compressed_buf[size-1]);
    }
    write_literal(out, compressed_buf, size);
    //printf("translation[%lld].off = %lld\n", lbuf->blocknum, off_of_leaf);
    out->translation[lbuf->blocknum].off  = off_of_leaf;
    out->translation[lbuf->blocknum].size = size;
    seek_align(out);
//#ifndef CILK_STUB
//    ttable_and_write_lock->unlock();
//#endif

    toku_free(compressed_buf);
    dbuf_destroy(&lbuf->dbuf);
    toku_free(lbuf);
}

static int write_translation_table (struct dbout *out, long long *off_of_translation_p) {
    seek_align(out);
    struct dbuf ttable;
    dbuf_init(&ttable);
    long long off_of_translation = out->current_off;
    long long bt_size_on_disk = out->n_translations * 16 + 20;
    putbuf_int64(&ttable, out->n_translations);    // number of records
    putbuf_int64(&ttable, -1LL); // the linked list
    out->translation[1].off = off_of_translation;
    out->translation[1].size = bt_size_on_disk;
    for (int i=0; i<out->n_translations; i++) {
	putbuf_int64(&ttable, out->translation[i].off);
	putbuf_int64(&ttable, out->translation[i].size);
    }
    unsigned int checksum = x1764_memory(ttable.buf, ttable.off);
    putbuf_int32(&ttable, checksum);
    assert(bt_size_on_disk==ttable.off);
    toku_os_full_pwrite(out->fd, ttable.buf, ttable.off, off_of_translation); /* use a bare pwrite and check error codes. ??? */
    dbuf_destroy(&ttable);
    *off_of_translation_p = off_of_translation;
    return 0;
}


static void write_header (struct dbout *out, long long translation_location_on_disk, long long translation_size_on_disk, BLOCKNUM root_blocknum_on_disk) {
    struct brt_header h = {.layout_version   = 11,
			   .checkpoint_count = 1,
			   .checkpoint_lsn   = (LSN){0xFFFFFFFFFFFFFFFF}, // (max_uint_long means that this doesn't need any kind of recovery
			   .nodesize         = nodesize,
			   .root             = root_blocknum_on_disk,
			   .flags            = 0,
			   .layout_version_original = 11
    };
    unsigned int size = toku_serialize_brt_header_size (&h);
    struct wbuf wbuf;
    char *MALLOC_N(size, buf);
    wbuf_init(&wbuf, buf, size);
    toku_serialize_brt_header_to_wbuf(&wbuf, &h, translation_location_on_disk, translation_size_on_disk);
    assert(wbuf.ndone==size);
    toku_os_full_pwrite(out->fd, wbuf.buf, wbuf.ndone, 0); // ??? use the version that returns error codes?
    toku_free(buf);
}


struct subtree_info {
    int64_t block;
    struct subtree_estimates subtree_estimates;
    int32_t fingerprint;
};

struct subtrees_info {
    int64_t next_free_block;
    int64_t n_subtrees;       // was n_blocks
    int64_t n_subtrees_limit;
    struct subtree_info *subtrees;
};

static void allocate_node (struct subtrees_info *sts, int64_t b, const struct subtree_estimates est, const int fingerprint) {
    if (sts->n_subtrees >= sts->n_subtrees_limit) {
	sts->n_subtrees_limit *= 2;
	XREALLOC_N(sts->n_subtrees_limit, sts->subtrees);
    }
    sts->subtrees[sts->n_subtrees].subtree_estimates = est;
    sts->subtrees[sts->n_subtrees].block = b;
    sts->subtrees[sts->n_subtrees].fingerprint = fingerprint;
    sts->n_subtrees++;
}

static int read_some_pivots (FILE *pivots_file, int n_to_read, BRTLOADER bl,
		      /*out*/ DBT pivots[/*n_to_read*/])
// pivots is an array to be filled in.  The pivots array is uninitialized.
{
    for (int i=0; i<n_to_read; i++) {
	pivots[i] = (DBT){.ulen=0, .data=0};
	int r = bl_read_dbt(&pivots[i], pivots_file, bl);
	if (r!=0) return r;
    };
    return 0;
}

static int setup_nonleaf_block (int n_children,
				struct subtrees_info *subtrees,         FILE *pivots_file,        int64_t first_child_offset_in_subtrees,
				struct subtrees_info *next_subtrees,    FILE *next_pivots_file,
				struct dbout *out, BRTLOADER bl,
				/*out*/int64_t *blocknum,
				/*out*/struct subtree_info **subtrees_info_p,
				/*out*/DBT **pivots_p)
// Do the serial part of setting up a non leaf block.
//   Read the pivots out of the file, and store them in a newly allocated array of DBTs (returned in *pivots_p)  There are (n_blocks_to_use-1) of these.
//   Copy the final pivot into the next_pivots file instead of returning it.
//   Copy the subtree_info from the subtrees structure, and store them in a newly allocated array of subtree_infos (return in *subtrees_info_p).  There are n_blocks_to_use of these.
//   Allocate a block number and return it in *blocknum.
//   Store the blocknum in the next_blocks structure, so it can be combined with the pivots at the next level of the tree.
//   Update n_blocks_used and n_translations.
// This code cannot be called in parallel because of all the race conditions.
// The actual creation of the node can be called in parallel after this work is done.
{
    //printf("Nonleaf has children :"); for(int i=0; i<n_children; i++) printf(" %ld", subtrees->subtrees[i].block); printf("\n");

    DBT *MALLOC_N(n_children, pivots);
    int r = read_some_pivots(pivots_file, n_children, bl, pivots);
    assert(r==0);

    if ((r=bl_write_dbt(&pivots[n_children-1], next_pivots_file, NULL, bl))) return r;
    // The last pivot was written to the next_pivots file, so we free it now instead of returning it.
    toku_free(pivots[n_children-1].data);
    memset(&pivots[n_children-1], sizeof(DBT), 0);

    struct subtree_estimates new_subtree_estimates = {.nkeys=0, .ndata=0, .dsize=0, .exact=TRUE};

    struct subtree_info *MALLOC_N(n_children, subtrees_array);
    int32_t fingerprint = 0;
    for (int i=0; i<n_children; i++) {
	int64_t from_blocknum = first_child_offset_in_subtrees + i;
	subtrees_array[i] = subtrees->subtrees[from_blocknum];
	add_estimates(&new_subtree_estimates, &subtrees->subtrees[from_blocknum].subtree_estimates);
	fingerprint += subtrees->subtrees[from_blocknum].fingerprint;
    }

    *blocknum = allocate_block(out);
    allocate_node(next_subtrees, *blocknum, new_subtree_estimates, fingerprint);

    *pivots_p = pivots;
    *subtrees_info_p = subtrees_array;
    return 0;
}

static
int write_nonleaf_node (struct dbout *out, int64_t blocknum_of_new_node, int n_children,
			DBT *pivots, /* must free this array, as well as the things it points t */
			struct subtree_info *subtree_info, int height, const struct descriptor *desc)
{
    assert(height>0);
    BRTNODE XMALLOC(node);
    node->desc  =(struct descriptor *)desc;
    node->nodesize = nodesize;
    node->thisnodename = make_blocknum(blocknum_of_new_node);
    node->layout_version = BRT_LAYOUT_VERSION;
    node->layout_version_original = BRT_LAYOUT_VERSION;
    node->height=height;
    node->u.n.n_children = n_children;
    node->flags = 0;
    node->local_fingerprint = 0;
    node->rand4fingerprint = random();
    XMALLOC_N(n_children-1, node->u.n.childkeys);
    unsigned int totalchildkeylens = 0;
    for (int i=0; i<n_children-1; i++) {
	struct kv_pair *childkey = kv_pair_malloc(pivots[i].data, pivots[i].size, NULL, 0);
	assert(childkey);
	node->u.n.childkeys[i] = childkey;
	totalchildkeylens += kv_pair_keylen(childkey);
    }
    node->u.n.n_bytes_in_buffers = 0;
    node->u.n.totalchildkeylens = totalchildkeylens;
    XMALLOC_N(n_children, node->u.n.childinfos);
    for (int i=0; i<n_children; i++) {
	struct brtnode_nonleaf_childinfo *ci = &node->u.n.childinfos[i];
	ci->subtree_fingerprint = subtree_info[i].fingerprint;
	ci->subtree_estimates   = subtree_info[i].subtree_estimates;
	ci->blocknum            = make_blocknum(subtree_info[i].block);
	ci->have_fullhash       = FALSE;
	ci->fullhash            = 0;
	int r = toku_fifo_create(&ci->buffer);
	assert(r==0);
	ci->n_bytes_in_buffer = 0;
    }

    size_t n_bytes;
    char *bytes;
    int r = toku_serialize_brtnode_to_memory(node, 1, 1, &n_bytes, &bytes);
    assert(r==0);

//#ifndef CILK_STUB
//    ttable_and_write_lock->lock();
//#endif
    out->translation[blocknum_of_new_node].off = out->current_off;
    out->translation[blocknum_of_new_node].size = n_bytes;
    //fprintf(stderr, "Wrote internal node at %ld (%ld bytes)\n", out->current_off, n_bytes);
    //for (uint32_t i=0; i<n_bytes; i++) { unsigned char b = bytes[i]; printf("%d:%02x (%d) ('%c')\n", i, b, b, (b>=' ' && b<128) ? b : '*'); }
    write_literal(out, bytes, n_bytes);
    seek_align(out);
//#ifndef CILK_STUB
//    ttable_and_write_lock->unlock();
//#endif

    toku_free(bytes);
    for (int i=0; i<n_children-1; i++) {
	toku_free(pivots[i].data);
	toku_free(node->u.n.childkeys[i]);
    }
    for (int i=0; i<n_children; i++) {
	toku_fifo_free(&node->u.n.childinfos[i].buffer);
    }
    toku_free(pivots);
    toku_free(node->u.n.childinfos);
    toku_free(node->u.n.childkeys);
    toku_free(node);
    toku_free(subtree_info);

    blocknum_of_new_node = blocknum_of_new_node;
    return 0;
}

static
int write_nonleaves (BRTLOADER bl, FILE *pivots_file, char *pivots_fname, struct dbout *out, struct subtrees_info *sts, const struct descriptor *descriptor) {

    int height=1;
    // Watch out for the case where we saved the last pivot but didn't write any more nodes out.
    // The trick is not to look at n_pivots, but to look at blocks.n_blocks
    while (sts->n_subtrees > 1) {
	// If there is more than one block in blocks, then we must build another level of the tree.

	// we need to create a pivots file for the pivots of the next level.
	// and a blocks_array
	// So for example.
	//  1) we grab 16 pivots and 16 blocks.
	//  2) We put the 15 pivots and 16 blocks into an non-leaf node.
	//  3) We put the 16th pivot into the next pivots file.
	{
	    int r = fseek(pivots_file, 0, SEEK_SET);
	    if (r!=0) { assert(errno!=0); return errno; }
	}

	FILE *next_pivots_file;
	char *next_pivots_name;
	brtloader_open_temp_file (bl, &next_pivots_file, &next_pivots_name);

	struct subtrees_info next_sts = {.n_subtrees = 0,
					 .n_subtrees_limit = 1};
	XMALLOC_N(next_sts.n_subtrees_limit, next_sts.subtrees);

	const int n_per_block = 16;
	int64_t n_subtrees_used = 0;
	while (sts->n_subtrees - n_subtrees_used >= n_per_block*2) {
	    // grab the first N_PER_BLOCK and build a node.
	    DBT *pivots;
	    int64_t blocknum_of_new_node;
	    struct subtree_info *subtree_info;
	    int r = setup_nonleaf_block (n_per_block,
					 sts, pivots_file, n_subtrees_used,
					 &next_sts, next_pivots_file,
					 out, bl,
					 &blocknum_of_new_node, &subtree_info, &pivots);
	    assert(r==0);
	    r = /*spawn*/write_nonleaf_node(out, blocknum_of_new_node, n_per_block, pivots, subtree_info, height, descriptor); // frees all the data structures that go into making the node.
	    assert(r==0);
	    n_subtrees_used += n_per_block;
	}
	// Now we have a one or two blocks at the end to handle.
	int64_t n_blocks_left = sts->n_subtrees - n_subtrees_used;
	assert(n_blocks_left>=2);
	if (n_blocks_left > n_per_block) {
	    // Write half the remaining blocks
	    int64_t n_first = n_blocks_left/2;
	    DBT *pivots;
	    int64_t blocknum_of_new_node;
	    struct subtree_info *subtree_info;
	    int r = setup_nonleaf_block(n_first,
					sts, pivots_file, n_subtrees_used,
					&next_sts, next_pivots_file,
					out, bl,
					&blocknum_of_new_node, &subtree_info, &pivots);
	    assert(r==0);
	    r = /*spawn*/write_nonleaf_node(out, blocknum_of_new_node, n_first, pivots, subtree_info, height, descriptor);
	    assert(r==0);
	    n_blocks_left -= n_first;
	    n_subtrees_used += n_first;
	}
	{
	    // Write the last block. 
	    DBT *pivots;
	    int64_t blocknum_of_new_node;
	    struct subtree_info *subtree_info;
	    int r = setup_nonleaf_block(n_blocks_left,
					sts, pivots_file, n_subtrees_used,
					&next_sts, next_pivots_file,
					out, bl,
					&blocknum_of_new_node, &subtree_info, &pivots);
	    assert(r==0);
	    r = /*spawn*/write_nonleaf_node(out, blocknum_of_new_node, n_blocks_left, pivots, subtree_info, height, descriptor);
	    assert(r==0);
	    n_subtrees_used += n_blocks_left;
	}
	assert(n_subtrees_used == sts->n_subtrees);
	// Now set things up for the next iteration.
	int r = fclose(pivots_file); assert(r==0);
	pivots_file = next_pivots_file;
	r = unlink(pivots_fname);    assert(r==0);
	toku_free(pivots_fname);
	pivots_fname = next_pivots_name;
	toku_free(sts->subtrees);
	*sts = next_sts;
	height++;
    }
    { int r = fclose(pivots_file); assert(r==0); }
    toku_free(pivots_fname);
    return 0;
}

int write_file_to_dbfile (int outfile, FILE *infile, BRTLOADER bl, const struct descriptor *descriptor) {
    // The pivots file will contain all the pivot strings (in the form <size(32bits)> <data>)
    // The pivots_fname is the name of the pivots file.
    // Note that the pivots file will have one extra pivot in it (the last key in the dictionary) which will not appear in the tree.
    int64_t n_pivots=0; // number of pivots in pivots_file
    FILE *pivots_file;  // the file
    char *pivots_fname; // the filename
    brtloader_open_temp_file (bl, &pivots_file, &pivots_fname);

    // The blocks_array will contain all the block numbers that correspond to the pivots.  Generally there should be one more block than pivot.
    struct subtrees_info sts = {.next_free_block = 3,
				.n_subtrees       = 0,
				.n_subtrees_limit = 1};
    XMALLOC_N(sts.n_subtrees_limit, sts.subtrees);

    DBT key={.data=0, .flags=DB_DBT_REALLOC, .size=0, .ulen=0};
    DBT val=key;
    struct dbout out = {.fd = outfile,
			.current_off = 8192, // leave 8K reserved at beginning
			.n_translations = 3, // 3 translations reserved at the beginning
			.n_translations_limit = 4};
    MALLOC_N(out.n_translations_limit, out.translation);
    out.translation[0].off = -2LL; out.translation[0].size = 0; // block 0 is NULL
    assert(1==RESERVED_BLOCKNUM_TRANSLATION);
    assert(2==RESERVED_BLOCKNUM_DESCRIPTOR);
    out.translation[1].off = -1;                                // block 1 is the block translation, filled in later
    out.translation[2].off = -1;                                 // block 2 is the descriptor
    seek_align(&out);
    int64_t lblock = allocate_block(&out);
    struct leaf_buf *lbuf = start_leaf(&out, descriptor, lblock);
    struct subtree_estimates est = zero_estimates;
    est.exact = TRUE;
    while (0==loader_read_row(infile, &key, &val, bl)) {
	if (lbuf->dbuf.off >= nodesize) {

	    allocate_node(&sts, lblock, est, lbuf->local_fingerprint);

	    n_pivots++;
	    int r;
	    if ((r=bl_write_dbt(&key, pivots_file, NULL, bl))) return r;

	    struct leaf_buf *writeit = lbuf;
	    /*cilk_spawn*/ finish_leafnode(&out, writeit);

	    lblock = allocate_block(&out);
	    lbuf = start_leaf(&out, descriptor, lblock);
	}
	add_pair_to_leafnode(lbuf, key.data, key.size, val.data, val.size);
	est.nkeys++;
	est.ndata++;
	est.dsize+=key.size + val.size;
    }
    
    allocate_node(&sts, lblock, est, lbuf->local_fingerprint);

    n_pivots++;
    {
	int r;
	if ((r=bl_write_dbt(&key, pivots_file, NULL, bl))) return r;
    }

    finish_leafnode(&out, lbuf);

    {
	int r = write_nonleaves(bl, pivots_file, pivots_fname, &out, &sts, descriptor);
	assert(r==0);
    }

    assert(sts.n_subtrees=1);
    BLOCKNUM root_block = make_blocknum(sts.subtrees[0].block);
    toku_free(sts.subtrees);

    // write the descriptor
    {
	seek_align(&out);
	assert(out.n_translations >= RESERVED_BLOCKNUM_DESCRIPTOR);
	assert(out.translation[RESERVED_BLOCKNUM_DESCRIPTOR].off == -1);
	out.translation[RESERVED_BLOCKNUM_DESCRIPTOR].off = out.current_off;
	size_t desc_size = 4+toku_serialize_descriptor_size(descriptor);
	assert(desc_size>0);
	out.translation[RESERVED_BLOCKNUM_DESCRIPTOR].size = desc_size;
	struct wbuf wbuf;
	char *MALLOC_N(desc_size, buf);
	wbuf_init(&wbuf, buf, desc_size);
	toku_serialize_descriptor_contents_to_wbuf(&wbuf, descriptor);
	u_int32_t checksum = x1764_finish(&wbuf.checksum);
	wbuf_int(&wbuf, checksum);
	assert(wbuf.ndone==desc_size);
	int r = toku_os_write(out.fd, wbuf.buf, wbuf.ndone);
	assert(r==0);
	out.current_off += desc_size;
	toku_free(buf);
    }
    
    long long off_of_translation;
    int r = write_translation_table(&out, &off_of_translation);
    assert(r==0);
    write_header(&out, off_of_translation, (out.n_translations+1)*16+4, root_block);
    if (key.data) toku_free(key.data);
    if (val.data) toku_free(val.data);
    if (out.translation) toku_free(out.translation);
    return 0;
}
