#define LOGGER_BUF_SIZE (1<<20)
typedef struct tokulogger *TOKULOGGER;
struct tokulogger {
    enum typ_tag tag;
    char *directory;
    FILE *f;
    int next_log_file_number;
    char buf[LOGGER_BUF_SIZE];
    int  n_in_buf;
};
