#include "yerror.h"
#include <stdio.h>
#include "log.h"
#include <sys/types.h>


#define LOGGER_BUF_SIZE (1<<24)
struct tokulogger {
    enum typ_tag tag;
    char *directory;
    int fd;
    int n_in_file;
    long long next_log_file_number;
    LSN lsn;
    char buf[LOGGER_BUF_SIZE];
    int  n_in_buf;
};

int tokulogger_find_next_unused_log_file(const char *directory, long long *result);

enum {
    LT_COMMIT                   = 'C',
    LT_DELETE                   = 'D',
    LT_FCREATE                  = 'F',
    LT_INSERT_WITH_NO_OVERWRITE = 'I',
    LT_FOPEN                    = 'O',
    LT_CHECKPOINT               = 'P',
    LT_BLOCK_RENAME             = 'R',
    LT_UNLINK                   = 'U'
};

struct tokutxn {
    u_int64_t txnid64;
    TOKULOGGER logger;
    TOKUTXN    parent;
};
