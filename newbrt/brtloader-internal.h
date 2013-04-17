#include <db.h>
#include "brttypes.h"
#include "brtloader.h"

/* These functions are exported to allow the tests to compile. */

int brtloader_open_temp_file (BRTLOADER bl, FILE **filep, char **fnamep);

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

    const char *temp_file_template;
    FILE *fprimary_rows;  char *fprimary_rows_name;
    FILE *fprimary_idx;   char *fprimary_idx_name;
    u_int64_t fprimary_offset;
    CACHETABLE cachetable;
};

/* These data structures are used for manipulating a collection of rows in main memory. */
struct row {
    char *data;
    int   klen,vlen;
};
struct rowset {
    size_t n_rows, n_rows_limit;
    struct row *rows;
    size_t n_bytes, n_bytes_limit;
    char *data;
};

void init_rowset (struct rowset *rows);
void destroy_rowset (struct rowset *rows);
void add_row (struct rowset *rows, DBT *key, DBT *val);

int loader_write_row(DBT *key, DBT *val, FILE *data, FILE *idx, u_int64_t *dataoff, BRTLOADER bl);
int loader_read_row (FILE *f, DBT *key, DBT *val, BRTLOADER bl);

void merge (struct row dest[/*an+bn*/], struct row a[/*an*/], int an, struct row b[/*bn*/], int bn,
	    DB *dest_db, brt_compare_func);
void mergesort_row_array (struct row rows[/*n*/], int n, DB *dest_db, brt_compare_func);

struct fileset {
    int n_temp_files, n_temp_files_limit;
    char **temp_data_names;
    char **temp_idx_names;
};

void init_fileset (struct fileset *fs);

int sort_and_write_rows (struct rowset *rows, struct fileset *fs, BRTLOADER bl, DB *dest_db, brt_compare_func);
int merge_files (struct fileset *fs, BRTLOADER bl, DB *dest_db, brt_compare_func);
int write_file_to_dbfile (int outfile, FILE *infile, BRTLOADER bl, const struct descriptor *descriptor);
