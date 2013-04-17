#include <db.h>
#include "brttypes.h"
#include "brtloader.h"

/* These functions are exported to allow the tests to compile. */

/* These structures maintain a collection of all the open temporary files used by the loader. */
struct file_info {
    BOOL is_open;
    BOOL is_extant; // if true, the file must be unlinked.
    char *fname;
    FILE *file;
    u_int64_t n_rows; // how many rows were written into that file
};
struct file_infos {
    int n_files;
    int n_files_limit;
    struct file_info *file_infos;
    int n_files_open, n_files_extant;
};
typedef struct fidx { int idx; } FIDX;
static const FIDX FIDX_NULL __attribute__((__unused__)) = {-1};

int brtloader_open_temp_file (BRTLOADER bl, FIDX*file_idx);


struct brtloader_s {
    int panic;
    int panic_errno;

    generate_row_for_put_func generate_row_for_put;
    brt_compare_func *bt_compare_funs;

    DB *src_db;
    int N;
    DB **dbs;
    const struct descriptor **descriptors; // N of these
    const char **new_fnames_in_env; // the file names that the final data will be written to (relative to env).

    u_int64_t n_rows; // how many rows have been put?

    const char *temp_file_template;
    FIDX fprimary_rows; // the file index (in the file_infos) for the data
    u_int64_t fprimary_offset;
    CACHETABLE cachetable;
    /* To make it easier to recover from errors, we don't use FILE*, instead we use an index into the file_infos. */
    struct file_infos file_infos;

#define PROGRESS_MAX (1<<16)
    int progress;       // Progress runs from 0 to PROGRESS_MAX.  When we call the poll function we convert to a float from 0.0 to 1.0
    // We use an integer so that we can add to the progress using a fetch-and-add instruction.

    // These two are set in the close function, and used while running close
    int (*poll_function)(void *extra, float progress);
    void *poll_extra;

    int user_said_stop; // 0 if the poll_function always returned zero.  If it ever returns nonzero, then store that value here.
};

/* These data structures are used for manipulating a collection of rows in main memory. */
struct row {
    size_t off; // the offset in the data array.
    int   klen,vlen;
};
struct rowset {
    size_t n_rows, n_rows_limit;
    struct row *rows;
    size_t n_bytes, n_bytes_limit;
    char *data;
};

int init_rowset (struct rowset *rows);
void destroy_rowset (struct rowset *rows);
void add_row (struct rowset *rows, DBT *key, DBT *val);

int loader_write_row(DBT *key, DBT *val, FIDX data, u_int64_t *dataoff, BRTLOADER bl);
int loader_read_row (FIDX f, DBT *key, DBT *val, BRTLOADER bl);

struct error_callback_s {
    void (*error_callback)(DB *, int which_db, int err, DBT *key, DBT *val, void *extra);
    DB *db;
    int which_db;
    void *extra;
};

int merge (struct row dest[/*an+bn*/], struct row a[/*an*/], int an, struct row b[/*bn*/], int bn,
	   DB *dest_db, brt_compare_func,
	   struct error_callback_s *,
	   struct rowset *);
int mergesort_row_array (struct row rows[/*n*/], int n, DB *dest_db, brt_compare_func, struct error_callback_s *, struct rowset *);

struct merge_fileset {
    int n_temp_files, n_temp_files_limit;
    FIDX *data_fidxs;
};

void init_merge_fileset (struct merge_fileset *fs);
void destroy_merge_fileset (struct merge_fileset *fs);

int sort_and_write_rows (struct rowset *rows, struct merge_fileset *fs, BRTLOADER bl, DB *dest_db, brt_compare_func,
			 struct error_callback_s *error_callback, int progress_allocation);
int merge_files (struct merge_fileset *fs, BRTLOADER bl, DB *dest_db, brt_compare_func, struct error_callback_s *, int progress_allocation);
int write_file_to_dbfile (int outfile, FIDX infile, BRTLOADER bl, const struct descriptor *descriptor, int progress_allocation);

int brtloader_init_file_infos (struct file_infos *fi);
void brtloader_fi_destroy (struct file_infos *fi, BOOL is_error);
int brtloader_fi_close (struct file_infos *fi, FIDX idx);
int brtloader_fi_reopen (struct file_infos *fi, FIDX idx, const char *mode);
int brtloader_fi_unlink (struct file_infos *fi, FIDX idx);
