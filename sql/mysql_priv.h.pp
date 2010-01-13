#include <my_global.h>
#include <my_config.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <alloca.h>
#include <errno.h>
#include <crypt.h>
#include <assert.h>
#include <my_attribute.h>
int __cxa_pure_virtual () __attribute__ ((weak));
#include <my_dbug.h>
struct _db_code_state_;
extern int _db_keyword_(struct _db_code_state_ *cs, const char *keyword);
extern int _db_strict_keyword_(const char *keyword);
extern int _db_explain_(struct _db_code_state_ *cs, char *buf, size_t len);
extern int _db_explain_init_(char *buf, size_t len);
extern void _db_setjmp_(void);
extern void _db_longjmp_(void);
extern void _db_process_(const char *name);
extern void _db_push_(const char *control);
extern void _db_pop_(void);
extern void _db_set_(struct _db_code_state_ *cs, const char *control);
extern void _db_set_init_(const char *control);
extern void _db_enter_(const char *_func_,const char *_file_,uint _line_,
   const char **_sfunc_,const char **_sfile_,
   uint *_slevel_, char ***);
extern void _db_return_(uint _line_,const char **_sfunc_,const char **_sfile_,
    uint *_slevel_);
extern void _db_pargs_(uint _line_,const char *keyword);
extern void _db_doprnt_ (const char *format,...)
  __attribute__((format(printf, 1, 2)));
extern void _db_dump_(uint _line_,const char *keyword,
                       const unsigned char *memory, size_t length);
extern void _db_end_(void);
extern void _db_lock_file_(void);
extern void _db_unlock_file_(void);
extern FILE *_db_fp_(void);
typedef int File;
typedef int my_socket;
typedef void (*sig_return)();
typedef char pchar;
typedef char puchar;
typedef char pbool;
typedef short pshort;
typedef float pfloat;
typedef int (*qsort_cmp)(const void *,const void *);
typedef int (*qsort_cmp2)(void*, const void *,const void *);
#include <sys/socket.h>
typedef socklen_t size_socket;
typedef long my_ptrdiff_t;
typedef unsigned char uchar;
typedef signed char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef unsigned long long int ulonglong;
typedef long long int longlong;
typedef longlong int64;
typedef ulonglong uint64;
typedef unsigned long long my_ulonglong;
typedef int intptr;
typedef ulonglong my_off_t;
typedef off_t os_off_t;
typedef uint8 int7;
typedef short int15;
typedef int myf;
typedef char my_bool;
typedef char bool;
typedef union {
  double v;
  long m[2];
} doubleget_union;
#include <dlfcn.h>
#include <mysql_version.h>
#include <mysql_embed.h>
#include <my_sys.h>
#include <my_pthread.h>
#include <pthread.h>
#include <sched.h>
extern int my_pthread_getprio(pthread_t thread_id);
typedef void *(* pthread_handler)(void *);
extern void my_pthread_setprio(pthread_t thread_id,int prior);
extern void my_pthread_attr_setprio(pthread_attr_t *attr, int priority);
typedef struct st_safe_mutex_t
{
  pthread_mutex_t global,mutex;
  const char *file;
  uint line,count;
  pthread_t thread;
} safe_mutex_t;
int safe_mutex_init(safe_mutex_t *mp, const pthread_mutexattr_t *attr,
                    const char *file, uint line);
int safe_mutex_lock(safe_mutex_t *mp, my_bool try_lock, const char *file, uint line);
int safe_mutex_unlock(safe_mutex_t *mp,const char *file, uint line);
int safe_mutex_destroy(safe_mutex_t *mp,const char *file, uint line);
int safe_cond_wait(pthread_cond_t *cond, safe_mutex_t *mp,const char *file,
     uint line);
int safe_cond_timedwait(pthread_cond_t *cond, safe_mutex_t *mp,
   struct timespec *abstime, const char *file, uint line);
void safe_mutex_global_init(void);
void safe_mutex_end(FILE *file);
typedef ulong my_thread_id;
extern my_bool my_thread_global_init(void);
extern void my_thread_global_end(void);
extern my_bool my_thread_init(void);
extern void my_thread_end(void);
extern const char *my_thread_name(void);
extern my_thread_id my_thread_dbug_id(void);
extern int pthread_no_free(void *);
extern int pthread_dummy(int);
struct st_my_thread_var
{
  int thr_errno;
  pthread_cond_t suspend;
  pthread_mutex_t mutex;
  pthread_mutex_t * volatile current_mutex;
  pthread_cond_t * volatile current_cond;
  pthread_t pthread_self;
  my_thread_id id;
  int cmp_length;
  int volatile abort;
  my_bool init;
  struct st_my_thread_var *next,**prev;
  void *opt_info;
  void *dbug;
  char name[10 +1];
};
extern struct st_my_thread_var *_my_thread_var(void) __attribute__ ((const));
extern uint my_thread_end_wait_time;
extern uint thd_lib_detected;
#include <m_ctype.h>
#include <my_attribute.h>
typedef struct unicase_info_st
{
  uint16 toupper;
  uint16 tolower;
  uint16 sort;
} MY_UNICASE_INFO;
extern MY_UNICASE_INFO *my_unicase_default[256];
extern MY_UNICASE_INFO *my_unicase_turkish[256];
typedef struct uni_ctype_st
{
  uchar pctype;
  uchar *ctype;
} MY_UNI_CTYPE;
extern MY_UNI_CTYPE my_uni_ctype[256];
typedef struct my_uni_idx_st
{
  uint16 from;
  uint16 to;
  uchar *tab;
} MY_UNI_IDX;
typedef struct
{
  uint beg;
  uint end;
  uint mb_len;
} my_match_t;
enum my_lex_states
{
  MY_LEX_START, MY_LEX_CHAR, MY_LEX_IDENT,
  MY_LEX_IDENT_SEP, MY_LEX_IDENT_START,
  MY_LEX_REAL, MY_LEX_HEX_NUMBER, MY_LEX_BIN_NUMBER,
  MY_LEX_CMP_OP, MY_LEX_LONG_CMP_OP, MY_LEX_STRING, MY_LEX_COMMENT, MY_LEX_END,
  MY_LEX_OPERATOR_OR_IDENT, MY_LEX_NUMBER_IDENT, MY_LEX_INT_OR_REAL,
  MY_LEX_REAL_OR_POINT, MY_LEX_BOOL, MY_LEX_EOL, MY_LEX_ESCAPE,
  MY_LEX_LONG_COMMENT, MY_LEX_END_LONG_COMMENT, MY_LEX_SEMICOLON,
  MY_LEX_SET_VAR, MY_LEX_USER_END, MY_LEX_HOSTNAME, MY_LEX_SKIP,
  MY_LEX_USER_VARIABLE_DELIMITER, MY_LEX_SYSTEM_VAR,
  MY_LEX_IDENT_OR_KEYWORD,
  MY_LEX_IDENT_OR_HEX, MY_LEX_IDENT_OR_BIN, MY_LEX_IDENT_OR_NCHAR,
  MY_LEX_STRING_OR_DELIMITER
};
struct charset_info_st;
typedef struct my_collation_handler_st
{
  my_bool (*init)(struct charset_info_st *, void *(*alloc)(size_t));
  int (*strnncoll)(struct charset_info_st *,
         const uchar *, size_t, const uchar *, size_t, my_bool);
  int (*strnncollsp)(struct charset_info_st *,
                         const uchar *, size_t, const uchar *, size_t,
                         my_bool diff_if_only_endspace_difference);
  size_t (*strnxfrm)(struct charset_info_st *,
                         uchar *, size_t, const uchar *, size_t);
  size_t (*strnxfrmlen)(struct charset_info_st *, size_t);
  my_bool (*like_range)(struct charset_info_st *,
   const char *s, size_t s_length,
   pchar w_prefix, pchar w_one, pchar w_many,
   size_t res_length,
   char *min_str, char *max_str,
   size_t *min_len, size_t *max_len);
  int (*wildcmp)(struct charset_info_st *,
         const char *str,const char *str_end,
                     const char *wildstr,const char *wildend,
                     int escape,int w_one, int w_many);
  int (*strcasecmp)(struct charset_info_st *, const char *, const char *);
  uint (*instr)(struct charset_info_st *,
                const char *b, size_t b_length,
                const char *s, size_t s_length,
                my_match_t *match, uint nmatch);
  void (*hash_sort)(struct charset_info_st *cs, const uchar *key, size_t len,
      ulong *nr1, ulong *nr2);
  my_bool (*propagate)(struct charset_info_st *cs, const uchar *str, size_t len);
} MY_COLLATION_HANDLER;
extern MY_COLLATION_HANDLER my_collation_mb_bin_handler;
extern MY_COLLATION_HANDLER my_collation_8bit_bin_handler;
extern MY_COLLATION_HANDLER my_collation_8bit_simple_ci_handler;
extern MY_COLLATION_HANDLER my_collation_ucs2_uca_handler;
typedef int (*my_charset_conv_mb_wc)(struct charset_info_st *, ulong *,
                                     const uchar *, const uchar *);
typedef int (*my_charset_conv_wc_mb)(struct charset_info_st *, ulong,
                                     uchar *, uchar *);
typedef size_t (*my_charset_conv_case)(struct charset_info_st *,
                                       char *, size_t, char *, size_t);
typedef struct my_charset_handler_st
{
  my_bool (*init)(struct charset_info_st *, void *(*alloc)(size_t));
  uint (*ismbchar)(struct charset_info_st *, const char *, const char *);
  uint (*mbcharlen)(struct charset_info_st *, uint c);
  size_t (*numchars)(struct charset_info_st *, const char *b, const char *e);
  size_t (*charpos)(struct charset_info_st *, const char *b, const char *e,
                     size_t pos);
  size_t (*well_formed_len)(struct charset_info_st *,
                             const char *b,const char *e,
                             size_t nchars, int *error);
  size_t (*lengthsp)(struct charset_info_st *, const char *ptr, size_t length);
  size_t (*numcells)(struct charset_info_st *, const char *b, const char *e);
  my_charset_conv_mb_wc mb_wc;
  my_charset_conv_wc_mb wc_mb;
  int (*ctype)(struct charset_info_st *cs, int *ctype,
               const uchar *s, const uchar *e);
  size_t (*caseup_str)(struct charset_info_st *, char *);
  size_t (*casedn_str)(struct charset_info_st *, char *);
  my_charset_conv_case caseup;
  my_charset_conv_case casedn;
  size_t (*snprintf)(struct charset_info_st *, char *to, size_t n,
                     const char *fmt,
                     ...) __attribute__((format(printf, 4, 5)));
  size_t (*long10_to_str)(struct charset_info_st *, char *to, size_t n,
                          int radix, long int val);
  size_t (*longlong10_to_str)(struct charset_info_st *, char *to, size_t n,
                              int radix, longlong val);
  void (*fill)(struct charset_info_st *, char *to, size_t len, int fill);
  long (*strntol)(struct charset_info_st *, const char *s, size_t l,
    int base, char **e, int *err);
  ulong (*strntoul)(struct charset_info_st *, const char *s, size_t l,
    int base, char **e, int *err);
  longlong (*strntoll)(struct charset_info_st *, const char *s, size_t l,
    int base, char **e, int *err);
  ulonglong (*strntoull)(struct charset_info_st *, const char *s, size_t l,
    int base, char **e, int *err);
  double (*strntod)(struct charset_info_st *, char *s, size_t l, char **e,
    int *err);
  longlong (*strtoll10)(struct charset_info_st *cs,
                           const char *nptr, char **endptr, int *error);
  ulonglong (*strntoull10rnd)(struct charset_info_st *cs,
                                const char *str, size_t length,
                                int unsigned_fl,
                                char **endptr, int *error);
  size_t (*scan)(struct charset_info_st *, const char *b, const char *e,
                        int sq);
} MY_CHARSET_HANDLER;
extern MY_CHARSET_HANDLER my_charset_8bit_handler;
extern MY_CHARSET_HANDLER my_charset_ucs2_handler;
typedef struct charset_info_st
{
  uint number;
  uint primary_number;
  uint binary_number;
  uint state;
  const char *csname;
  const char *name;
  const char *comment;
  const char *tailoring;
  uchar *ctype;
  uchar *to_lower;
  uchar *to_upper;
  uchar *sort_order;
  uint16 *contractions;
  uint16 **sort_order_big;
  uint16 *tab_to_uni;
  MY_UNI_IDX *tab_from_uni;
  MY_UNICASE_INFO **caseinfo;
  uchar *state_map;
  uchar *ident_map;
  uint strxfrm_multiply;
  uchar caseup_multiply;
  uchar casedn_multiply;
  uint mbminlen;
  uint mbmaxlen;
  uint16 min_sort_char;
  uint16 max_sort_char;
  uchar pad_char;
  my_bool escape_with_backslash_is_dangerous;
  MY_CHARSET_HANDLER *cset;
  MY_COLLATION_HANDLER *coll;
} CHARSET_INFO;
extern CHARSET_INFO my_charset_bin;
extern CHARSET_INFO my_charset_big5_chinese_ci;
extern CHARSET_INFO my_charset_big5_bin;
extern CHARSET_INFO my_charset_cp932_japanese_ci;
extern CHARSET_INFO my_charset_cp932_bin;
extern CHARSET_INFO my_charset_eucjpms_japanese_ci;
extern CHARSET_INFO my_charset_eucjpms_bin;
extern CHARSET_INFO my_charset_euckr_korean_ci;
extern CHARSET_INFO my_charset_euckr_bin;
extern CHARSET_INFO my_charset_gb2312_chinese_ci;
extern CHARSET_INFO my_charset_gb2312_bin;
extern CHARSET_INFO my_charset_gbk_chinese_ci;
extern CHARSET_INFO my_charset_gbk_bin;
extern CHARSET_INFO my_charset_latin1;
extern CHARSET_INFO my_charset_latin1_german2_ci;
extern CHARSET_INFO my_charset_latin1_bin;
extern CHARSET_INFO my_charset_latin2_czech_ci;
extern CHARSET_INFO my_charset_sjis_japanese_ci;
extern CHARSET_INFO my_charset_sjis_bin;
extern CHARSET_INFO my_charset_tis620_thai_ci;
extern CHARSET_INFO my_charset_tis620_bin;
extern CHARSET_INFO my_charset_ucs2_general_ci;
extern CHARSET_INFO my_charset_ucs2_bin;
extern CHARSET_INFO my_charset_ucs2_unicode_ci;
extern CHARSET_INFO my_charset_ujis_japanese_ci;
extern CHARSET_INFO my_charset_ujis_bin;
extern CHARSET_INFO my_charset_utf8_general_ci;
extern CHARSET_INFO my_charset_utf8_unicode_ci;
extern CHARSET_INFO my_charset_utf8_bin;
extern CHARSET_INFO my_charset_cp1250_czech_ci;
extern CHARSET_INFO my_charset_filename;
extern size_t my_strnxfrm_simple(CHARSET_INFO *, uchar *, size_t,
                                 const uchar *, size_t);
size_t my_strnxfrmlen_simple(CHARSET_INFO *, size_t);
extern int my_strnncoll_simple(CHARSET_INFO *, const uchar *, size_t,
    const uchar *, size_t, my_bool);
extern int my_strnncollsp_simple(CHARSET_INFO *, const uchar *, size_t,
                                  const uchar *, size_t,
                                  my_bool diff_if_only_endspace_difference);
extern void my_hash_sort_simple(CHARSET_INFO *cs,
    const uchar *key, size_t len,
    ulong *nr1, ulong *nr2);
extern size_t my_lengthsp_8bit(CHARSET_INFO *cs, const char *ptr, size_t length);
extern uint my_instr_simple(struct charset_info_st *,
                            const char *b, size_t b_length,
                            const char *s, size_t s_length,
                            my_match_t *match, uint nmatch);
extern size_t my_caseup_str_8bit(CHARSET_INFO *, char *);
extern size_t my_casedn_str_8bit(CHARSET_INFO *, char *);
extern size_t my_caseup_8bit(CHARSET_INFO *, char *src, size_t srclen,
                             char *dst, size_t dstlen);
extern size_t my_casedn_8bit(CHARSET_INFO *, char *src, size_t srclen,
                             char *dst, size_t dstlen);
extern int my_strcasecmp_8bit(CHARSET_INFO * cs, const char *, const char *);
int my_mb_wc_8bit(CHARSET_INFO *cs,ulong *wc, const uchar *s,const uchar *e);
int my_wc_mb_8bit(CHARSET_INFO *cs,ulong wc, uchar *s, uchar *e);
int my_mb_ctype_8bit(CHARSET_INFO *,int *, const uchar *,const uchar *);
int my_mb_ctype_mb(CHARSET_INFO *,int *, const uchar *,const uchar *);
size_t my_scan_8bit(CHARSET_INFO *cs, const char *b, const char *e, int sq);
size_t my_snprintf_8bit(struct charset_info_st *, char *to, size_t n,
                        const char *fmt, ...)
  __attribute__((format(printf, 4, 5)));
long my_strntol_8bit(CHARSET_INFO *, const char *s, size_t l, int base,
                           char **e, int *err);
ulong my_strntoul_8bit(CHARSET_INFO *, const char *s, size_t l, int base,
       char **e, int *err);
longlong my_strntoll_8bit(CHARSET_INFO *, const char *s, size_t l, int base,
       char **e, int *err);
ulonglong my_strntoull_8bit(CHARSET_INFO *, const char *s, size_t l, int base,
       char **e, int *err);
double my_strntod_8bit(CHARSET_INFO *, char *s, size_t l,char **e,
       int *err);
size_t my_long10_to_str_8bit(CHARSET_INFO *, char *to, size_t l, int radix,
                             long int val);
size_t my_longlong10_to_str_8bit(CHARSET_INFO *, char *to, size_t l, int radix,
                                 longlong val);
longlong my_strtoll10_8bit(CHARSET_INFO *cs,
                           const char *nptr, char **endptr, int *error);
longlong my_strtoll10_ucs2(CHARSET_INFO *cs,
                           const char *nptr, char **endptr, int *error);
ulonglong my_strntoull10rnd_8bit(CHARSET_INFO *cs,
                                 const char *str, size_t length, int
                                 unsigned_fl, char **endptr, int *error);
ulonglong my_strntoull10rnd_ucs2(CHARSET_INFO *cs,
                                 const char *str, size_t length,
                                 int unsigned_fl, char **endptr, int *error);
void my_fill_8bit(CHARSET_INFO *cs, char* to, size_t l, int fill);
my_bool my_like_range_simple(CHARSET_INFO *cs,
         const char *ptr, size_t ptr_length,
         pbool escape, pbool w_one, pbool w_many,
         size_t res_length,
         char *min_str, char *max_str,
         size_t *min_length, size_t *max_length);
my_bool my_like_range_mb(CHARSET_INFO *cs,
     const char *ptr, size_t ptr_length,
     pbool escape, pbool w_one, pbool w_many,
     size_t res_length,
     char *min_str, char *max_str,
     size_t *min_length, size_t *max_length);
my_bool my_like_range_ucs2(CHARSET_INFO *cs,
       const char *ptr, size_t ptr_length,
       pbool escape, pbool w_one, pbool w_many,
       size_t res_length,
       char *min_str, char *max_str,
       size_t *min_length, size_t *max_length);
int my_wildcmp_8bit(CHARSET_INFO *,
      const char *str,const char *str_end,
      const char *wildstr,const char *wildend,
      int escape, int w_one, int w_many);
int my_wildcmp_bin(CHARSET_INFO *,
     const char *str,const char *str_end,
     const char *wildstr,const char *wildend,
     int escape, int w_one, int w_many);
size_t my_numchars_8bit(CHARSET_INFO *, const char *b, const char *e);
size_t my_numcells_8bit(CHARSET_INFO *, const char *b, const char *e);
size_t my_charpos_8bit(CHARSET_INFO *, const char *b, const char *e, size_t pos);
size_t my_well_formed_len_8bit(CHARSET_INFO *, const char *b, const char *e,
                             size_t pos, int *error);
uint my_mbcharlen_8bit(CHARSET_INFO *, uint c);
extern size_t my_caseup_str_mb(CHARSET_INFO *, char *);
extern size_t my_casedn_str_mb(CHARSET_INFO *, char *);
extern size_t my_caseup_mb(CHARSET_INFO *, char *src, size_t srclen,
                                         char *dst, size_t dstlen);
extern size_t my_casedn_mb(CHARSET_INFO *, char *src, size_t srclen,
                                         char *dst, size_t dstlen);
extern int my_strcasecmp_mb(CHARSET_INFO * cs,const char *, const char *);
int my_wildcmp_mb(CHARSET_INFO *,
    const char *str,const char *str_end,
    const char *wildstr,const char *wildend,
    int escape, int w_one, int w_many);
size_t my_numchars_mb(CHARSET_INFO *, const char *b, const char *e);
size_t my_numcells_mb(CHARSET_INFO *, const char *b, const char *e);
size_t my_charpos_mb(CHARSET_INFO *, const char *b, const char *e, size_t pos);
size_t my_well_formed_len_mb(CHARSET_INFO *, const char *b, const char *e,
                             size_t pos, int *error);
uint my_instr_mb(struct charset_info_st *,
                 const char *b, size_t b_length,
                 const char *s, size_t s_length,
                 my_match_t *match, uint nmatch);
int my_wildcmp_unicode(CHARSET_INFO *cs,
                       const char *str, const char *str_end,
                       const char *wildstr, const char *wildend,
                       int escape, int w_one, int w_many,
                       MY_UNICASE_INFO **weights);
extern my_bool my_parse_charset_xml(const char *bug, size_t len,
        int (*add)(CHARSET_INFO *cs));
extern char *my_strchr(CHARSET_INFO *cs, const char *str, const char *end,
                       pchar c);
my_bool my_propagate_simple(CHARSET_INFO *cs, const uchar *str, size_t len);
my_bool my_propagate_complex(CHARSET_INFO *cs, const uchar *str, size_t len);
uint my_string_repertoire(CHARSET_INFO *cs, const char *str, ulong len);
my_bool my_charset_is_ascii_based(CHARSET_INFO *cs);
my_bool my_charset_is_8bit_pure_ascii(CHARSET_INFO *cs);
#include <stdarg.h>
#include <typelib.h>
#include "my_alloc.h"
typedef struct st_used_mem
{
  struct st_used_mem *next;
  unsigned int left;
  unsigned int size;
} USED_MEM;
typedef struct st_mem_root
{
  USED_MEM *free;
  USED_MEM *used;
  USED_MEM *pre_alloc;
  size_t min_malloc;
  size_t block_size;
  unsigned int block_num;
  unsigned int first_block_usage;
  void (*error_handler)(void);
} MEM_ROOT;
typedef struct st_typelib {
  unsigned int count;
  const char *name;
  const char **type_names;
  unsigned int *type_lengths;
} TYPELIB;
extern my_ulonglong find_typeset(char *x, TYPELIB *typelib,int *error_position);
extern int find_type_or_exit(const char *x, TYPELIB *typelib,
                             const char *option);
extern int find_type(char *x, const TYPELIB *typelib, unsigned int full_name);
extern void make_type(char *to,unsigned int nr,TYPELIB *typelib);
extern const char *get_type(TYPELIB *typelib,unsigned int nr);
extern TYPELIB *copy_typelib(MEM_ROOT *root, TYPELIB *from);
extern TYPELIB sql_protocol_typelib;
extern void *my_malloc(size_t Size,myf MyFlags);
extern void *my_realloc(void *oldpoint, size_t Size, myf MyFlags);
extern void my_no_flags_free(void *ptr);
extern void *my_memdup(const void *from,size_t length,myf MyFlags);
extern char *my_strdup(const char *from,myf MyFlags);
extern char *my_strndup(const char *from, size_t length,
       myf MyFlags);
extern uint my_get_large_page_size(void);
extern uchar * my_large_malloc(size_t size, myf my_flags);
extern void my_large_free(uchar * ptr, myf my_flags);
extern int errno;
extern char errbuff[(2)][(256)];
extern char *home_dir;
extern const char *my_progname;
extern char curr_dir[];
extern int (*error_handler_hook)(uint my_err, const char *str,myf MyFlags);
extern int (*fatal_error_handler_hook)(uint my_err, const char *str,
           myf MyFlags);
extern uint my_file_limit;
extern ulong my_thread_stack_size;
extern my_bool my_use_large_pages;
extern uint my_large_page_size;
extern CHARSET_INFO *default_charset_info;
extern CHARSET_INFO *all_charsets[256];
extern CHARSET_INFO compiled_charsets[];
extern ulong my_file_opened,my_stream_opened, my_tmp_file_created;
extern ulong my_file_total_opened;
extern uint mysys_usage_id;
extern my_bool my_init_done;
extern void (*my_sigtstp_cleanup)(void),
     (*my_sigtstp_restart)(void),
     (*my_abort_hook)(int);
extern int my_umask,
    my_umask_dir,
    my_recived_signals,
    my_safe_to_handle_signal,
    my_dont_interrupt;
extern my_bool mysys_uses_curses, my_use_symdir;
extern ulong sf_malloc_cur_memory, sf_malloc_max_memory;
extern ulong my_default_record_cache_size;
extern my_bool my_disable_locking, my_disable_async_io,
               my_disable_flush_key_blocks, my_disable_symlinks;
extern char wild_many,wild_one,wild_prefix;
extern const char *charsets_dir;
extern char *my_defaults_extra_file;
extern const char *my_defaults_group_suffix;
extern const char *my_defaults_file;
extern my_bool timed_mutexes;
typedef struct wild_file_pack
{
  uint wilds;
  uint not_pos;
  char * *wild;
} WF_PACK;
enum loglevel {
   ERROR_LEVEL,
   WARNING_LEVEL,
   INFORMATION_LEVEL
};
enum cache_type
{
  TYPE_NOT_SET= 0, READ_CACHE, WRITE_CACHE,
  SEQ_READ_APPEND ,
  READ_FIFO, READ_NET,WRITE_NET};
enum flush_type
{
  FLUSH_KEEP,
  FLUSH_RELEASE,
  FLUSH_IGNORE_CHANGED,
  FLUSH_FORCE_WRITE
};
typedef struct st_record_cache
{
  File file;
  int rc_seek,error,inited;
  uint rc_length,read_length,reclength;
  my_off_t rc_record_pos,end_of_file;
  uchar *rc_buff,*rc_buff2,*rc_pos,*rc_end,*rc_request_pos;
  enum cache_type type;
} RECORD_CACHE;
enum file_type
{
  UNOPEN = 0, FILE_BY_OPEN, FILE_BY_CREATE, STREAM_BY_FOPEN, STREAM_BY_FDOPEN,
  FILE_BY_MKSTEMP, FILE_BY_DUP
};
struct st_my_file_info
{
  char * name;
  enum file_type type;
};
extern struct st_my_file_info *my_file_info;
typedef struct st_dynamic_array
{
  uchar *buffer;
  uint elements,max_element;
  uint alloc_increment;
  uint size_of_element;
} DYNAMIC_ARRAY;
typedef struct st_my_tmpdir
{
  DYNAMIC_ARRAY full_list;
  char **list;
  uint cur, max;
  pthread_mutex_t mutex;
} MY_TMPDIR;
typedef struct st_dynamic_string
{
  char *str;
  size_t length,max_length,alloc_increment;
} DYNAMIC_STRING;
struct st_io_cache;
typedef int (*IO_CACHE_CALLBACK)(struct st_io_cache*);
typedef struct st_io_cache_share
{
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  pthread_cond_t cond_writer;
  my_off_t pos_in_file;
  struct st_io_cache *source_cache;
  uchar *buffer;
  uchar *read_end;
  int running_threads;
  int total_threads;
  int error;
} IO_CACHE_SHARE;
typedef struct st_io_cache
{
  my_off_t pos_in_file;
  my_off_t end_of_file;
  uchar *read_pos;
  uchar *read_end;
  uchar *buffer;
  uchar *request_pos;
  uchar *write_buffer;
  uchar *append_read_pos;
  uchar *write_pos;
  uchar *write_end;
  uchar **current_pos, **current_end;
  pthread_mutex_t append_buffer_lock;
  IO_CACHE_SHARE *share;
  int (*read_function)(struct st_io_cache *,uchar *,size_t);
  int (*write_function)(struct st_io_cache *,const uchar *,size_t);
  enum cache_type type;
  IO_CACHE_CALLBACK pre_read;
  IO_CACHE_CALLBACK post_read;
  IO_CACHE_CALLBACK pre_close;
  ulong disk_writes;
  void* arg;
  char *file_name;
  char *dir,*prefix;
  File file;
  int seek_not_done,error;
  size_t buffer_length;
  size_t read_length;
  myf myflags;
  my_bool alloced_buffer;
} IO_CACHE;
typedef int (*qsort2_cmp)(const void *, const void *, const void *);
int my_b_copy_to_file(IO_CACHE *cache, FILE *file);
my_off_t my_b_append_tell(IO_CACHE* info);
my_off_t my_b_safe_tell(IO_CACHE* info);
typedef uint32 ha_checksum;
typedef int (*Process_option_func)(void *ctx, const char *group_name,
                                   const char *option);
#include <my_alloc.h>
extern int my_copy(const char *from,const char *to,myf MyFlags);
extern int my_append(const char *from,const char *to,myf MyFlags);
extern int my_delete(const char *name,myf MyFlags);
extern int my_getwd(char * buf,size_t size,myf MyFlags);
extern int my_setwd(const char *dir,myf MyFlags);
extern int my_lock(File fd,int op,my_off_t start, my_off_t length,myf MyFlags);
extern void *my_once_alloc(size_t Size,myf MyFlags);
extern void my_once_free(void);
extern char *my_once_strdup(const char *src,myf myflags);
extern void *my_once_memdup(const void *src, size_t len, myf myflags);
extern File my_open(const char *FileName,int Flags,myf MyFlags);
extern File my_register_filename(File fd, const char *FileName,
     enum file_type type_of_file,
     uint error_message_number, myf MyFlags);
extern File my_create(const char *FileName,int CreateFlags,
        int AccessFlags, myf MyFlags);
extern int my_close(File Filedes,myf MyFlags);
extern File my_dup(File file, myf MyFlags);
extern int my_mkdir(const char *dir, int Flags, myf MyFlags);
extern int my_readlink(char *to, const char *filename, myf MyFlags);
extern int my_realpath(char *to, const char *filename, myf MyFlags);
extern File my_create_with_symlink(const char *linkname, const char *filename,
       int createflags, int access_flags,
       myf MyFlags);
extern int my_delete_with_symlink(const char *name, myf MyFlags);
extern int my_rename_with_symlink(const char *from,const char *to,myf MyFlags);
extern int my_symlink(const char *content, const char *linkname, myf MyFlags);
extern size_t my_read(File Filedes,uchar *Buffer,size_t Count,myf MyFlags);
extern size_t my_pread(File Filedes,uchar *Buffer,size_t Count,my_off_t offset,
       myf MyFlags);
extern int my_rename(const char *from,const char *to,myf MyFlags);
extern my_off_t my_seek(File fd,my_off_t pos,int whence,myf MyFlags);
extern my_off_t my_tell(File fd,myf MyFlags);
extern size_t my_write(File Filedes,const uchar *Buffer,size_t Count,
       myf MyFlags);
extern size_t my_pwrite(File Filedes,const uchar *Buffer,size_t Count,
        my_off_t offset,myf MyFlags);
extern size_t my_fread(FILE *stream,uchar *Buffer,size_t Count,myf MyFlags);
extern size_t my_fwrite(FILE *stream,const uchar *Buffer,size_t Count,
        myf MyFlags);
extern my_off_t my_fseek(FILE *stream,my_off_t pos,int whence,myf MyFlags);
extern my_off_t my_ftell(FILE *stream,myf MyFlags);
extern void *_mymalloc(size_t uSize,const char *sFile,
                       uint uLine, myf MyFlag);
extern void *_myrealloc(void *pPtr,size_t uSize,const char *sFile,
         uint uLine, myf MyFlag);
extern void * my_multi_malloc (myf MyFlags, ...);
extern void _myfree(void *pPtr,const char *sFile,uint uLine, myf MyFlag);
extern int _sanity(const char *sFile, uint uLine);
extern void *_my_memdup(const void *from, size_t length,
                        const char *sFile, uint uLine,myf MyFlag);
extern char * _my_strdup(const char *from, const char *sFile, uint uLine,
                         myf MyFlag);
extern char *_my_strndup(const char *from, size_t length,
                         const char *sFile, uint uLine,
                         myf MyFlag);
extern void *my_memmem(const void *haystack, size_t haystacklen,
                       const void *needle, size_t needlelen);
extern int check_if_legal_filename(const char *path);
extern int check_if_legal_tablename(const char *path);
extern void init_glob_errs(void);
extern FILE *my_fopen(const char *FileName,int Flags,myf MyFlags);
extern FILE *my_fdopen(File Filedes,const char *name, int Flags,myf MyFlags);
extern int my_fclose(FILE *fd,myf MyFlags);
extern int my_chsize(File fd,my_off_t newlength, int filler, myf MyFlags);
extern int my_sync(File fd, myf my_flags);
extern int my_sync_dir(const char *dir_name, myf my_flags);
extern int my_sync_dir_by_file(const char *file_name, myf my_flags);
extern int my_error (int nr,myf MyFlags, ...);
extern int my_printf_error (uint my_err, const char *format, myf MyFlags, ...)
        __attribute__((format(printf, 2, 4)));
extern int my_error_register(const char **errmsgs, int first, int last);
extern const char **my_error_unregister(int first, int last);
extern int my_message(uint my_err, const char *str,myf MyFlags);
extern int my_message_no_curses(uint my_err, const char *str,myf MyFlags);
extern int my_message_curses(uint my_err, const char *str,myf MyFlags);
extern my_bool my_init(void);
extern void my_end(int infoflag);
extern int my_redel(const char *from, const char *to, int MyFlags);
extern int my_copystat(const char *from, const char *to, int MyFlags);
extern char * my_filename(File fd);
extern my_bool init_tmpdir(MY_TMPDIR *tmpdir, const char *pathlist);
extern char *my_tmpdir(MY_TMPDIR *tmpdir);
extern void free_tmpdir(MY_TMPDIR *tmpdir);
extern void my_remember_signal(int signal_number,void (*func)(int));
extern size_t dirname_part(char * to,const char *name, size_t *to_res_length);
extern size_t dirname_length(const char *name);
extern int test_if_hard_path(const char *dir_name);
extern my_bool has_path(const char *name);
extern char *convert_dirname(char *to, const char *from, const char *from_end);
extern void to_unix_path(char * name);
extern char * fn_ext(const char *name);
extern char * fn_same(char * toname,const char *name,int flag);
extern char * fn_format(char * to,const char *name,const char *dir,
      const char *form, uint flag);
extern size_t strlength(const char *str);
extern void pack_dirname(char * to,const char *from);
extern size_t unpack_dirname(char * to,const char *from);
extern size_t cleanup_dirname(char * to,const char *from);
extern size_t system_filename(char * to,const char *from);
extern size_t unpack_filename(char * to,const char *from);
extern char * intern_filename(char * to,const char *from);
extern char * directory_file_name(char * dst, const char *src);
extern int pack_filename(char * to, const char *name, size_t max_length);
extern char * my_path(char * to,const char *progname,
    const char *own_pathname_part);
extern char * my_load_path(char * to, const char *path,
         const char *own_path_prefix);
extern int wild_compare(const char *str,const char *wildstr,
                        pbool str_is_pattern);
extern WF_PACK *wf_comp(char * str);
extern int wf_test(struct wild_file_pack *wf_pack,const char *name);
extern void wf_end(struct wild_file_pack *buffer);
extern my_bool array_append_string_unique(const char *str,
                                          const char **array, size_t size);
extern void get_date(char * to,int timeflag,time_t use_time);
extern void soundex(CHARSET_INFO *, char * out_pntr, char * in_pntr,
                    pbool remove_garbage);
extern int init_record_cache(RECORD_CACHE *info,size_t cachesize,File file,
        size_t reclength,enum cache_type type,
        pbool use_async_io);
extern int read_cache_record(RECORD_CACHE *info,uchar *to);
extern int end_record_cache(RECORD_CACHE *info);
extern int write_cache_record(RECORD_CACHE *info,my_off_t filepos,
         const uchar *record,size_t length);
extern int flush_write_cache(RECORD_CACHE *info);
extern long my_clock(void);
extern void sigtstp_handler(int signal_number);
extern void handle_recived_signals(void);
extern void my_set_alarm_variable(int signo);
extern void my_string_ptr_sort(uchar *base,uint items,size_t size);
extern void radixsort_for_str_ptr(uchar* base[], uint number_of_elements,
      size_t size_of_element,uchar *buffer[]);
extern void my_qsort(void *base_ptr, size_t total_elems, size_t size,
                        qsort_cmp cmp);
extern void my_qsort2(void *base_ptr, size_t total_elems, size_t size,
                         qsort2_cmp cmp, void *cmp_argument);
extern qsort2_cmp get_ptr_compare(size_t);
void my_store_ptr(uchar *buff, size_t pack_length, my_off_t pos);
my_off_t my_get_ptr(uchar *ptr, size_t pack_length);
extern int init_io_cache(IO_CACHE *info,File file,size_t cachesize,
    enum cache_type type,my_off_t seek_offset,
    pbool use_async_io, myf cache_myflags);
extern my_bool reinit_io_cache(IO_CACHE *info,enum cache_type type,
          my_off_t seek_offset,pbool use_async_io,
          pbool clear_cache);
extern void setup_io_cache(IO_CACHE* info);
extern int _my_b_read(IO_CACHE *info,uchar *Buffer,size_t Count);
extern int _my_b_read_r(IO_CACHE *info,uchar *Buffer,size_t Count);
extern void init_io_cache_share(IO_CACHE *read_cache, IO_CACHE_SHARE *cshare,
                                IO_CACHE *write_cache, uint num_threads);
extern void remove_io_thread(IO_CACHE *info);
extern int _my_b_seq_read(IO_CACHE *info,uchar *Buffer,size_t Count);
extern int _my_b_net_read(IO_CACHE *info,uchar *Buffer,size_t Count);
extern int _my_b_get(IO_CACHE *info);
extern int _my_b_async_read(IO_CACHE *info,uchar *Buffer,size_t Count);
extern int _my_b_write(IO_CACHE *info,const uchar *Buffer,size_t Count);
extern int my_b_append(IO_CACHE *info,const uchar *Buffer,size_t Count);
extern int my_b_safe_write(IO_CACHE *info,const uchar *Buffer,size_t Count);
extern int my_block_write(IO_CACHE *info, const uchar *Buffer,
     size_t Count, my_off_t pos);
extern int my_b_flush_io_cache(IO_CACHE *info, int need_append_buffer_lock);
extern int end_io_cache(IO_CACHE *info);
extern size_t my_b_fill(IO_CACHE *info);
extern void my_b_seek(IO_CACHE *info,my_off_t pos);
extern size_t my_b_gets(IO_CACHE *info, char *to, size_t max_length);
extern my_off_t my_b_filelength(IO_CACHE *info);
extern size_t my_b_printf(IO_CACHE *info, const char* fmt, ...);
extern size_t my_b_vprintf(IO_CACHE *info, const char* fmt, va_list ap);
extern my_bool open_cached_file(IO_CACHE *cache,const char *dir,
     const char *prefix, size_t cache_size,
     myf cache_myflags);
extern my_bool real_open_cached_file(IO_CACHE *cache);
extern void close_cached_file(IO_CACHE *cache);
File create_temp_file(char *to, const char *dir, const char *pfx,
        int mode, myf MyFlags);
extern my_bool init_dynamic_array2(DYNAMIC_ARRAY *array,uint element_size,
                                   void *init_buffer, uint init_alloc,
                                   uint alloc_increment
                                   );
extern my_bool init_dynamic_array(DYNAMIC_ARRAY *array,uint element_size,
                                  uint init_alloc,uint alloc_increment
                                  );
extern my_bool insert_dynamic(DYNAMIC_ARRAY *array,uchar * element);
extern uchar *alloc_dynamic(DYNAMIC_ARRAY *array);
extern uchar *pop_dynamic(DYNAMIC_ARRAY*);
extern my_bool set_dynamic(DYNAMIC_ARRAY *array,uchar * element,uint array_index);
extern my_bool allocate_dynamic(DYNAMIC_ARRAY *array, uint max_elements);
extern void get_dynamic(DYNAMIC_ARRAY *array,uchar * element,uint array_index);
extern void delete_dynamic(DYNAMIC_ARRAY *array);
extern void delete_dynamic_element(DYNAMIC_ARRAY *array, uint array_index);
extern void freeze_size(DYNAMIC_ARRAY *array);
extern int get_index_dynamic(DYNAMIC_ARRAY *array, uchar * element);
extern my_bool init_dynamic_string(DYNAMIC_STRING *str, const char *init_str,
       size_t init_alloc,size_t alloc_increment);
extern my_bool dynstr_append(DYNAMIC_STRING *str, const char *append);
my_bool dynstr_append_mem(DYNAMIC_STRING *str, const char *append,
     size_t length);
extern my_bool dynstr_append_os_quoted(DYNAMIC_STRING *str, const char *append,
                                       ...);
extern my_bool dynstr_set(DYNAMIC_STRING *str, const char *init_str);
extern my_bool dynstr_realloc(DYNAMIC_STRING *str, size_t additional_size);
extern my_bool dynstr_trunc(DYNAMIC_STRING *str, size_t n);
extern void dynstr_free(DYNAMIC_STRING *str);
extern void init_alloc_root(MEM_ROOT *mem_root, size_t block_size,
       size_t pre_alloc_size);
extern void *alloc_root(MEM_ROOT *mem_root, size_t Size);
extern void *multi_alloc_root(MEM_ROOT *mem_root, ...);
extern void free_root(MEM_ROOT *root, myf MyFLAGS);
extern void set_prealloc_root(MEM_ROOT *root, char *ptr);
extern void reset_root_defaults(MEM_ROOT *mem_root, size_t block_size,
                                size_t prealloc_size);
extern char *strdup_root(MEM_ROOT *root,const char *str);
extern char *strmake_root(MEM_ROOT *root,const char *str,size_t len);
extern void *memdup_root(MEM_ROOT *root,const void *str, size_t len);
extern int get_defaults_options(int argc, char **argv,
                                char **defaults, char **extra_defaults,
                                char **group_suffix);
extern int load_defaults(const char *conf_file, const char **groups,
    int *argc, char ***argv);
extern int modify_defaults_file(const char *file_location, const char *option,
                                const char *option_value,
                                const char *section_name, int remove_option);
extern int my_search_option_files(const char *conf_file, int *argc,
                                  char ***argv, uint *args_used,
                                  Process_option_func func, void *func_ctx);
extern void free_defaults(char **argv);
extern void my_print_default_files(const char *conf_file);
extern void print_defaults(const char *conf_file, const char **groups);
extern my_bool my_compress(uchar *, size_t *, size_t *);
extern my_bool my_uncompress(uchar *, size_t , size_t *);
extern uchar *my_compress_alloc(const uchar *packet, size_t *len,
                                size_t *complen);
extern int packfrm(uchar *, size_t, uchar **, size_t *);
extern int unpackfrm(uchar **, size_t *, const uchar *);
extern ha_checksum my_checksum(ha_checksum crc, const uchar *mem,
                               size_t count);
extern void my_sleep(ulong m_seconds);
extern ulong crc32(ulong crc, const uchar *buf, uint len);
extern uint my_set_max_open_files(uint files);
void my_free_open_file_info(void);
extern time_t my_time(myf flags);
extern ulonglong my_getsystime(void);
extern ulonglong my_micro_time();
extern ulonglong my_micro_time_and_time(time_t *time_arg);
time_t my_time_possible_from_micro(ulonglong microtime);
extern my_bool my_gethwaddr(uchar *to);
extern int my_getncpus();
#include <sys/mman.h>
int my_msync(int, void *, size_t, int);
extern uint get_charset_number(const char *cs_name, uint cs_flags);
extern uint get_collation_number(const char *name);
extern const char *get_charset_name(uint cs_number);
extern CHARSET_INFO *get_charset(uint cs_number, myf flags);
extern CHARSET_INFO *get_charset_by_name(const char *cs_name, myf flags);
extern CHARSET_INFO *get_charset_by_csname(const char *cs_name,
        uint cs_flags, myf my_flags);
extern my_bool resolve_charset(const char *cs_name,
                               CHARSET_INFO *default_cs,
                               CHARSET_INFO **cs);
extern my_bool resolve_collation(const char *cl_name,
                                 CHARSET_INFO *default_cl,
                                 CHARSET_INFO **cl);
extern void free_charsets(void);
extern char *get_charsets_dir(char *buf);
extern my_bool my_charset_same(CHARSET_INFO *cs1, CHARSET_INFO *cs2);
extern my_bool init_compiled_charsets(myf flags);
extern void add_compiled_collation(CHARSET_INFO *cs);
extern size_t escape_string_for_mysql(CHARSET_INFO *charset_info,
                                      char *to, size_t to_length,
                                      const char *from, size_t length);
extern size_t escape_quotes_for_mysql(CHARSET_INFO *charset_info,
                                      char *to, size_t to_length,
                                      const char *from, size_t length);
extern void thd_increment_bytes_sent(ulong length);
extern void thd_increment_bytes_received(ulong length);
extern void thd_increment_net_big_packet_count(ulong length);
#include <my_time.h>
#include "my_global.h"
#include "mysql_time.h"
enum enum_mysql_timestamp_type
{
  MYSQL_TIMESTAMP_NONE= -2, MYSQL_TIMESTAMP_ERROR= -1,
  MYSQL_TIMESTAMP_DATE= 0, MYSQL_TIMESTAMP_DATETIME= 1, MYSQL_TIMESTAMP_TIME= 2
};
typedef struct st_mysql_time
{
  unsigned int year, month, day, hour, minute, second;
  unsigned long second_part;
  my_bool neg;
  enum enum_mysql_timestamp_type time_type;
} MYSQL_TIME;
extern ulonglong log_10_int[20];
extern uchar days_in_month[];
typedef long my_time_t;
my_bool check_date(const MYSQL_TIME *ltime, my_bool not_zero_date,
                   ulong flags, int *was_cut);
enum enum_mysql_timestamp_type
str_to_datetime(const char *str, uint length, MYSQL_TIME *l_time,
                uint flags, int *was_cut);
longlong number_to_datetime(longlong nr, MYSQL_TIME *time_res,
                            uint flags, int *was_cut);
ulonglong TIME_to_ulonglong_datetime(const MYSQL_TIME *);
ulonglong TIME_to_ulonglong_date(const MYSQL_TIME *);
ulonglong TIME_to_ulonglong_time(const MYSQL_TIME *);
ulonglong TIME_to_ulonglong(const MYSQL_TIME *);
my_bool str_to_time(const char *str,uint length, MYSQL_TIME *l_time,
                    int *warning);
int check_time_range(struct st_mysql_time *, int *warning);
long calc_daynr(uint year,uint month,uint day);
uint calc_days_in_year(uint year);
uint year_2000_handling(uint year);
void my_init_time(void);
static inline my_bool validate_timestamp_range(const MYSQL_TIME *t)
{
  if ((t->year > 2038 || t->year < (1900 + 70 - 1)) ||
      (t->year == 2038 && (t->month > 1 || t->day > 19)) ||
      (t->year == (1900 + 70 - 1) && (t->month < 12 || t->day < 31)))
    return (0);
  return (1);
}
my_time_t
my_system_gmt_sec(const MYSQL_TIME *t, long *my_timezone,
                  my_bool *in_dst_time_gap);
void set_zero_time(MYSQL_TIME *tm, enum enum_mysql_timestamp_type time_type);
int my_time_to_str(const MYSQL_TIME *l_time, char *to);
int my_date_to_str(const MYSQL_TIME *l_time, char *to);
int my_datetime_to_str(const MYSQL_TIME *l_time, char *to);
int my_TIME_to_str(const MYSQL_TIME *l_time, char *to);
enum interval_type
{
  INTERVAL_YEAR, INTERVAL_QUARTER, INTERVAL_MONTH, INTERVAL_WEEK, INTERVAL_DAY,
  INTERVAL_HOUR, INTERVAL_MINUTE, INTERVAL_SECOND, INTERVAL_MICROSECOND,
  INTERVAL_YEAR_MONTH, INTERVAL_DAY_HOUR, INTERVAL_DAY_MINUTE,
  INTERVAL_DAY_SECOND, INTERVAL_HOUR_MINUTE, INTERVAL_HOUR_SECOND,
  INTERVAL_MINUTE_SECOND, INTERVAL_DAY_MICROSECOND, INTERVAL_HOUR_MICROSECOND,
  INTERVAL_MINUTE_MICROSECOND, INTERVAL_SECOND_MICROSECOND, INTERVAL_LAST
};
#include <m_string.h>
#include <strings.h>
#include <string.h>
#include <stdarg.h>
#include <strings.h>
#include <memory.h>
extern void *(*my_str_malloc)(size_t);
extern void (*my_str_free)(void *);
extern char *stpcpy(char *, const char *);
extern char _dig_vec_upper[];
extern char _dig_vec_lower[];
extern const double log_10[309];
extern void bmove512(uchar *dst,const uchar *src,size_t len);
extern void bmove_upp(uchar *dst,const uchar *src,size_t len);
extern void bchange(uchar *dst,size_t old_len,const uchar *src,
       size_t new_len,size_t tot_len);
extern void strappend(char *s,size_t len,pchar fill);
extern char *strend(const char *s);
extern char *strcend(const char *, pchar);
extern char *strfield(char *src,int fields,int chars,int blanks,
      int tabch);
extern char *strfill(char * s,size_t len,pchar fill);
extern size_t strinstr(const char *str,const char *search);
extern size_t r_strinstr(const char *str, size_t from, const char *search);
extern char *strkey(char *dst,char *head,char *tail,char *flags);
extern char *strmake(char *dst,const char *src,size_t length);
extern char *strnmov(char *dst,const char *src,size_t n);
extern char *strsuff(const char *src,const char *suffix);
extern char *strcont(const char *src,const char *set);
extern char *strxcat (char *dst,const char *src, ...);
extern char *strxmov (char *dst,const char *src, ...);
extern char *strxcpy (char *dst,const char *src, ...);
extern char *strxncat (char *dst,size_t len, const char *src, ...);
extern char *strxnmov (char *dst,size_t len, const char *src, ...);
extern char *strxncpy (char *dst,size_t len, const char *src, ...);
extern int is_prefix(const char *, const char *);
double my_strtod(const char *str, char **end, int *error);
double my_atof(const char *nptr);
extern char *llstr(longlong value,char *buff);
extern char *ullstr(longlong value,char *buff);
extern char *int2str(long val, char *dst, int radix, int upcase);
extern char *int10_to_str(long val,char *dst,int radix);
extern char *str2int(const char *src,int radix,long lower,long upper,
    long *val);
longlong my_strtoll10(const char *nptr, char **endptr, int *error);
extern char *longlong2str(longlong val,char *dst,int radix);
extern char *longlong10_to_str(longlong val,char *dst,int radix);
extern size_t my_vsnprintf(char *str, size_t n,
                           const char *format, va_list ap);
extern size_t my_snprintf(char *to, size_t n, const char *fmt, ...)
  __attribute__((format(printf, 3, 4)));
struct st_mysql_lex_string
{
  char *str;
  size_t length;
};
typedef struct st_mysql_lex_string LEX_STRING;
#include <hash.h>
typedef uchar *(*hash_get_key)(const uchar *,size_t*,my_bool);
typedef void (*hash_free_key)(void *);
typedef struct st_hash {
  size_t key_offset,key_length;
  size_t blength;
  ulong records;
  uint flags;
  DYNAMIC_ARRAY array;
  hash_get_key get_key;
  void (*free)(void *);
  CHARSET_INFO *charset;
} HASH;
typedef uint HASH_SEARCH_STATE;
my_bool _hash_init(HASH *hash, uint growth_size,CHARSET_INFO *charset,
     ulong default_array_elements, size_t key_offset,
     size_t key_length, hash_get_key get_key,
     void (*free_element)(void*), uint flags );
void hash_free(HASH *tree);
void my_hash_reset(HASH *hash);
uchar *hash_element(HASH *hash,ulong idx);
uchar *hash_search(const HASH *info, const uchar *key, size_t length);
uchar *hash_first(const HASH *info, const uchar *key, size_t length,
                HASH_SEARCH_STATE *state);
uchar *hash_next(const HASH *info, const uchar *key, size_t length,
                 HASH_SEARCH_STATE *state);
my_bool my_hash_insert(HASH *info,const uchar *data);
my_bool hash_delete(HASH *hash,uchar *record);
my_bool hash_update(HASH *hash,uchar *record,uchar *old_key,size_t old_key_length);
void hash_replace(HASH *hash, HASH_SEARCH_STATE *state, uchar *new_row);
my_bool hash_check(HASH *hash);
#include <signal.h>
#include <thr_lock.h>
#include <my_pthread.h>
#include <my_list.h>
typedef struct st_list {
  struct st_list *prev,*next;
  void *data;
} LIST;
typedef int (*list_walk_action)(void *,void *);
extern LIST *list_add(LIST *root,LIST *element);
extern LIST *list_delete(LIST *root,LIST *element);
extern LIST *list_cons(void *data,LIST *root);
extern LIST *list_reverse(LIST *root);
extern void list_free(LIST *root,unsigned int free_data);
extern unsigned int list_length(LIST *);
extern int list_walk(LIST *,list_walk_action action,unsigned char * argument);
struct st_thr_lock;
extern ulong locks_immediate,locks_waited ;
enum thr_lock_type { TL_IGNORE=-1,
       TL_UNLOCK,
       TL_READ,
       TL_READ_WITH_SHARED_LOCKS,
       TL_READ_HIGH_PRIORITY,
       TL_READ_NO_INSERT,
       TL_WRITE_ALLOW_WRITE,
       TL_WRITE_ALLOW_READ,
       TL_WRITE_CONCURRENT_INSERT,
       TL_WRITE_DELAYED,
                     TL_WRITE_DEFAULT,
       TL_WRITE_LOW_PRIORITY,
       TL_WRITE,
       TL_WRITE_ONLY};
enum enum_thr_lock_result { THR_LOCK_SUCCESS= 0, THR_LOCK_ABORTED= 1,
                            THR_LOCK_WAIT_TIMEOUT= 2, THR_LOCK_DEADLOCK= 3 };
extern ulong max_write_lock_count;
extern ulong table_lock_wait_timeout;
extern my_bool thr_lock_inited;
extern enum thr_lock_type thr_upgraded_concurrent_insert_lock;
typedef struct st_thr_lock_info
{
  pthread_t thread;
  my_thread_id thread_id;
  ulong n_cursors;
} THR_LOCK_INFO;
typedef struct st_thr_lock_owner
{
  THR_LOCK_INFO *info;
} THR_LOCK_OWNER;
typedef struct st_thr_lock_data {
  THR_LOCK_OWNER *owner;
  struct st_thr_lock_data *next,**prev;
  struct st_thr_lock *lock;
  pthread_cond_t *cond;
  enum thr_lock_type type;
  void *status_param;
  void *debug_print_param;
} THR_LOCK_DATA;
struct st_lock_list {
  THR_LOCK_DATA *data,**last;
};
typedef struct st_thr_lock {
  LIST list;
  pthread_mutex_t mutex;
  struct st_lock_list read_wait;
  struct st_lock_list read;
  struct st_lock_list write_wait;
  struct st_lock_list write;
  ulong write_lock_count;
  uint read_no_write_count;
  void (*get_status)(void*, int);
  void (*copy_status)(void*,void*);
  void (*update_status)(void*);
  void (*restore_status)(void*);
  my_bool (*check_status)(void *);
} THR_LOCK;
extern LIST *thr_lock_thread_list;
extern pthread_mutex_t THR_LOCK_lock;
my_bool init_thr_lock(void);
void thr_lock_info_init(THR_LOCK_INFO *info);
void thr_lock_init(THR_LOCK *lock);
void thr_lock_delete(THR_LOCK *lock);
void thr_lock_data_init(THR_LOCK *lock,THR_LOCK_DATA *data,
   void *status_param);
enum enum_thr_lock_result thr_lock(THR_LOCK_DATA *data,
                                   THR_LOCK_OWNER *owner,
                                   enum thr_lock_type lock_type);
void thr_unlock(THR_LOCK_DATA *data);
enum enum_thr_lock_result thr_multi_lock(THR_LOCK_DATA **data,
                                         uint count, THR_LOCK_OWNER *owner);
void thr_multi_unlock(THR_LOCK_DATA **data,uint count);
void thr_abort_locks(THR_LOCK *lock, my_bool upgrade_lock);
my_bool thr_abort_locks_for_thread(THR_LOCK *lock, my_thread_id thread);
void thr_print_locks(void);
my_bool thr_upgrade_write_delay_lock(THR_LOCK_DATA *data);
void thr_downgrade_write_lock(THR_LOCK_DATA *data,
                                 enum thr_lock_type new_lock_type);
my_bool thr_reschedule_write_lock(THR_LOCK_DATA *data);
#include <my_base.h>
#include <my_global.h>
#include <my_dir.h>
#include <sys/stat.h>
typedef struct fileinfo
{
  char *name;
  struct stat *mystat;
} FILEINFO;
typedef struct st_my_dir
{
  struct fileinfo *dir_entry;
  uint number_off_files;
} MY_DIR;
extern MY_DIR *my_dir(const char *path,myf MyFlags);
extern void my_dirend(MY_DIR *buffer);
extern struct stat *my_stat(const char *path, struct stat *stat_area, myf my_flags);
extern int my_fstat(int filenr, struct stat *stat_area, myf MyFlags);
#include <my_sys.h>
#include <m_string.h>
#include <errno.h>
#include <my_list.h>
enum ha_rkey_function {
  HA_READ_KEY_EXACT,
  HA_READ_KEY_OR_NEXT,
  HA_READ_KEY_OR_PREV,
  HA_READ_AFTER_KEY,
  HA_READ_BEFORE_KEY,
  HA_READ_PREFIX,
  HA_READ_PREFIX_LAST,
  HA_READ_PREFIX_LAST_OR_PREV,
  HA_READ_MBR_CONTAIN,
  HA_READ_MBR_INTERSECT,
  HA_READ_MBR_WITHIN,
  HA_READ_MBR_DISJOINT,
  HA_READ_MBR_EQUAL
};
enum ha_key_alg {
  HA_KEY_ALG_UNDEF= 0,
  HA_KEY_ALG_BTREE= 1,
  HA_KEY_ALG_RTREE= 2,
  HA_KEY_ALG_HASH= 3,
  HA_KEY_ALG_FULLTEXT= 4
};
enum ha_storage_media {
  HA_SM_DEFAULT= 0,
  HA_SM_DISK= 1,
  HA_SM_MEMORY= 2
};
enum ha_extra_function {
  HA_EXTRA_NORMAL=0,
  HA_EXTRA_QUICK=1,
  HA_EXTRA_NOT_USED=2,
  HA_EXTRA_CACHE=3,
  HA_EXTRA_NO_CACHE=4,
  HA_EXTRA_NO_READCHECK=5,
  HA_EXTRA_READCHECK=6,
  HA_EXTRA_KEYREAD=7,
  HA_EXTRA_NO_KEYREAD=8,
  HA_EXTRA_NO_USER_CHANGE=9,
  HA_EXTRA_KEY_CACHE=10,
  HA_EXTRA_NO_KEY_CACHE=11,
  HA_EXTRA_WAIT_LOCK=12,
  HA_EXTRA_NO_WAIT_LOCK=13,
  HA_EXTRA_WRITE_CACHE=14,
  HA_EXTRA_FLUSH_CACHE=15,
  HA_EXTRA_NO_KEYS=16,
  HA_EXTRA_KEYREAD_CHANGE_POS=17,
  HA_EXTRA_REMEMBER_POS=18,
  HA_EXTRA_RESTORE_POS=19,
  HA_EXTRA_REINIT_CACHE=20,
  HA_EXTRA_FORCE_REOPEN=21,
  HA_EXTRA_FLUSH,
  HA_EXTRA_NO_ROWS,
  HA_EXTRA_RESET_STATE,
  HA_EXTRA_IGNORE_DUP_KEY,
  HA_EXTRA_NO_IGNORE_DUP_KEY,
  HA_EXTRA_PREPARE_FOR_DROP,
  HA_EXTRA_PREPARE_FOR_UPDATE,
  HA_EXTRA_PRELOAD_BUFFER_SIZE,
  HA_EXTRA_CHANGE_KEY_TO_UNIQUE,
  HA_EXTRA_CHANGE_KEY_TO_DUP,
  HA_EXTRA_KEYREAD_PRESERVE_FIELDS,
  HA_EXTRA_MMAP,
  HA_EXTRA_IGNORE_NO_KEY,
  HA_EXTRA_NO_IGNORE_NO_KEY,
  HA_EXTRA_MARK_AS_LOG_TABLE,
  HA_EXTRA_WRITE_CAN_REPLACE,
  HA_EXTRA_WRITE_CANNOT_REPLACE,
  HA_EXTRA_DELETE_CANNOT_BATCH,
  HA_EXTRA_UPDATE_CANNOT_BATCH,
  HA_EXTRA_INSERT_WITH_UPDATE,
  HA_EXTRA_PREPARE_FOR_RENAME,
  HA_EXTRA_ATTACH_CHILDREN,
  HA_EXTRA_DETACH_CHILDREN
};
enum ha_panic_function {
  HA_PANIC_CLOSE,
  HA_PANIC_WRITE,
  HA_PANIC_READ
};
enum ha_base_keytype {
  HA_KEYTYPE_END=0,
  HA_KEYTYPE_TEXT=1,
  HA_KEYTYPE_BINARY=2,
  HA_KEYTYPE_SHORT_INT=3,
  HA_KEYTYPE_LONG_INT=4,
  HA_KEYTYPE_FLOAT=5,
  HA_KEYTYPE_DOUBLE=6,
  HA_KEYTYPE_NUM=7,
  HA_KEYTYPE_USHORT_INT=8,
  HA_KEYTYPE_ULONG_INT=9,
  HA_KEYTYPE_LONGLONG=10,
  HA_KEYTYPE_ULONGLONG=11,
  HA_KEYTYPE_INT24=12,
  HA_KEYTYPE_UINT24=13,
  HA_KEYTYPE_INT8=14,
  HA_KEYTYPE_VARTEXT1=15,
  HA_KEYTYPE_VARBINARY1=16,
  HA_KEYTYPE_VARTEXT2=17,
  HA_KEYTYPE_VARBINARY2=18,
  HA_KEYTYPE_BIT=19
};
typedef ulong key_part_map;
enum en_fieldtype {
  FIELD_LAST=-1,FIELD_NORMAL,FIELD_SKIP_ENDSPACE,FIELD_SKIP_PRESPACE,
  FIELD_SKIP_ZERO,FIELD_BLOB,FIELD_CONSTANT,FIELD_INTERVALL,FIELD_ZERO,
  FIELD_VARCHAR,FIELD_CHECK,
  FIELD_enum_val_count
};
enum data_file_type {
  STATIC_RECORD, DYNAMIC_RECORD, COMPRESSED_RECORD, BLOCK_RECORD
};
typedef struct st_key_range
{
  const uchar *key;
  uint length;
  key_part_map keypart_map;
  enum ha_rkey_function flag;
} key_range;
typedef struct st_key_multi_range
{
  key_range start_key;
  key_range end_key;
  char *ptr;
  uint range_flag;
} KEY_MULTI_RANGE;
typedef my_off_t ha_rows;
typedef void (* invalidator_by_filename)(const char * filename);
#include <queues.h>
typedef struct st_queue {
  uchar **root;
  void *first_cmp_arg;
  uint elements;
  uint max_elements;
  uint offset_to_key;
  int max_at_top;
  int (*compare)(void *, uchar *,uchar *);
  uint auto_extent;
} QUEUE;
typedef int (*queue_compare)(void *,uchar *, uchar *);
int init_queue(QUEUE *queue,uint max_elements,uint offset_to_key,
        pbool max_at_top, queue_compare compare,
        void *first_cmp_arg);
int init_queue_ex(QUEUE *queue,uint max_elements,uint offset_to_key,
        pbool max_at_top, queue_compare compare,
        void *first_cmp_arg, uint auto_extent);
int reinit_queue(QUEUE *queue,uint max_elements,uint offset_to_key,
                 pbool max_at_top, queue_compare compare,
                 void *first_cmp_arg);
int resize_queue(QUEUE *queue, uint max_elements);
void delete_queue(QUEUE *queue);
void queue_insert(QUEUE *queue,uchar *element);
int queue_insert_safe(QUEUE *queue, uchar *element);
uchar *queue_remove(QUEUE *queue,uint idx);
void _downheap(QUEUE *queue,uint idx);
void queue_fix(QUEUE *queue);
#include "sql_bitmap.h"
#include <my_bitmap.h>
#include <m_string.h>
typedef uint32 my_bitmap_map;
typedef struct st_bitmap
{
  my_bitmap_map *bitmap;
  uint n_bits;
  my_bitmap_map last_word_mask;
  my_bitmap_map *last_word_ptr;
  pthread_mutex_t *mutex;
} MY_BITMAP;
extern void create_last_word_mask(MY_BITMAP *map);
extern my_bool bitmap_init(MY_BITMAP *map, my_bitmap_map *buf, uint n_bits,
                           my_bool thread_safe);
extern my_bool bitmap_is_clear_all(const MY_BITMAP *map);
extern my_bool bitmap_is_prefix(const MY_BITMAP *map, uint prefix_size);
extern my_bool bitmap_is_set_all(const MY_BITMAP *map);
extern my_bool bitmap_is_subset(const MY_BITMAP *map1, const MY_BITMAP *map2);
extern my_bool bitmap_is_overlapping(const MY_BITMAP *map1,
                                     const MY_BITMAP *map2);
extern my_bool bitmap_test_and_set(MY_BITMAP *map, uint bitmap_bit);
extern my_bool bitmap_test_and_clear(MY_BITMAP *map, uint bitmap_bit);
extern my_bool bitmap_fast_test_and_set(MY_BITMAP *map, uint bitmap_bit);
extern uint bitmap_set_next(MY_BITMAP *map);
extern uint bitmap_get_first(const MY_BITMAP *map);
extern uint bitmap_get_first_set(const MY_BITMAP *map);
extern uint bitmap_bits_set(const MY_BITMAP *map);
extern void bitmap_free(MY_BITMAP *map);
extern void bitmap_set_above(MY_BITMAP *map, uint from_byte, uint use_bit);
extern void bitmap_set_prefix(MY_BITMAP *map, uint prefix_size);
extern void bitmap_intersect(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_subtract(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_union(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_xor(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_invert(MY_BITMAP *map);
extern void bitmap_copy(MY_BITMAP *map, const MY_BITMAP *map2);
extern uint bitmap_lock_set_next(MY_BITMAP *map);
extern void bitmap_lock_clear_bit(MY_BITMAP *map, uint bitmap_bit);
static inline void
bitmap_set_bit(MY_BITMAP *map,uint bit)
{
  assert(bit < (map)->n_bits);
  (((uchar*)(map)->bitmap)[(bit) / 8] |= (1 << ((bit) & 7)));
}
static inline void
bitmap_flip_bit(MY_BITMAP *map,uint bit)
{
  assert(bit < (map)->n_bits);
  (((uchar*)(map)->bitmap)[(bit) / 8] ^= (1 << ((bit) & 7)));
}
static inline void
bitmap_clear_bit(MY_BITMAP *map,uint bit)
{
  assert(bit < (map)->n_bits);
  (((uchar*)(map)->bitmap)[(bit) / 8] &= ~ (1 << ((bit) & 7)));
}
static inline uint
bitmap_is_set(const MY_BITMAP *map,uint bit)
{
  assert(bit < (map)->n_bits);
  return (uint) (((uchar*)(map)->bitmap)[(bit) / 8] & (1 << ((bit) & 7)));
}
static inline my_bool bitmap_cmp(const MY_BITMAP *map1, const MY_BITMAP *map2)
{
  *(map1)->last_word_ptr|= (map1)->last_word_mask;
  *(map2)->last_word_ptr|= (map2)->last_word_mask;
  return memcmp((map1)->bitmap, (map2)->bitmap, 4*((((map1))->n_bits + 31)/32))==0;
}
template <uint default_width> class Bitmap
{
  MY_BITMAP map;
  uint32 buffer[(default_width+31)/32];
public:
  Bitmap() { init(); }
  Bitmap(const Bitmap& from) { *this=from; }
  explicit Bitmap(uint prefix_to_set) { init(prefix_to_set); }
  void init() { bitmap_init(&map, buffer, default_width, 0); }
  void init(uint prefix_to_set) { init(); set_prefix(prefix_to_set); }
  uint length() const { return default_width; }
  Bitmap& operator=(const Bitmap& map2)
  {
    init();
    memcpy(buffer, map2.buffer, sizeof(buffer));
    return *this;
  }
  void set_bit(uint n) { bitmap_set_bit(&map, n); }
  void clear_bit(uint n) { bitmap_clear_bit(&map, n); }
  void set_prefix(uint n) { bitmap_set_prefix(&map, n); }
  void set_all() { (memset((&map)->bitmap, 0xFF, 4*((((&map))->n_bits + 31)/32))); }
  void clear_all() { { memset((&map)->bitmap, 0, 4*((((&map))->n_bits + 31)/32)); }; }
  void intersect(Bitmap& map2) { bitmap_intersect(&map, &map2.map); }
  void intersect(ulonglong map2buff)
  {
    MY_BITMAP map2;
    bitmap_init(&map2, (uint32 *)&map2buff, sizeof(ulonglong)*8, 0);
    bitmap_intersect(&map, &map2);
  }
  void intersect_extended(ulonglong map2buff)
  {
    intersect(map2buff);
    if (map.n_bits > sizeof(ulonglong) * 8)
      bitmap_set_above(&map, sizeof(ulonglong),
                       ((map2buff & (1LL << (sizeof(ulonglong) * 8 - 1))) ? 1 : 0));
  }
  void subtract(Bitmap& map2) { bitmap_subtract(&map, &map2.map); }
  void merge(Bitmap& map2) { bitmap_union(&map, &map2.map); }
  my_bool is_set(uint n) const { return bitmap_is_set(&map, n); }
  my_bool is_prefix(uint n) const { return bitmap_is_prefix(&map, n); }
  my_bool is_clear_all() const { return bitmap_is_clear_all(&map); }
  my_bool is_set_all() const { return bitmap_is_set_all(&map); }
  my_bool is_subset(const Bitmap& map2) const { return bitmap_is_subset(&map, &map2.map); }
  my_bool is_overlapping(const Bitmap& map2) const { return bitmap_is_overlapping(&map, &map2.map); }
  my_bool operator==(const Bitmap& map2) const { return bitmap_cmp(&map, &map2.map); }
  char *print(char *buf) const
  {
    char *s=buf;
    const uchar *e=(uchar *)buffer, *b=e+sizeof(buffer)-1;
    while (!*b && b>e)
      b--;
    if ((*s=_dig_vec_upper[*b >> 4]) != '0')
        s++;
    *s++=_dig_vec_upper[*b & 15];
    while (--b>=e)
    {
      *s++=_dig_vec_upper[*b >> 4];
      *s++=_dig_vec_upper[*b & 15];
    }
    *s=0;
    return buf;
  }
  ulonglong to_ulonglong() const
  {
    if (sizeof(buffer) >= 8)
      return (*((ulonglong *) (buffer)));
    assert(sizeof(buffer) >= 4);
    return (ulonglong) (*((uint32 *) (buffer)));
  }
};
template <> class Bitmap<64>
{
  ulonglong map;
public:
  Bitmap<64>() { }
  explicit Bitmap<64>(uint prefix_to_set) { set_prefix(prefix_to_set); }
  void init() { }
  void init(uint prefix_to_set) { set_prefix(prefix_to_set); }
  uint length() const { return 64; }
  void set_bit(uint n) { map|= ((ulonglong)1) << n; }
  void clear_bit(uint n) { map&= ~(((ulonglong)1) << n); }
  void set_prefix(uint n)
  {
    if (n >= length())
      set_all();
    else
      map= (((ulonglong)1) << n)-1;
  }
  void set_all() { map=~(ulonglong)0; }
  void clear_all() { map=(ulonglong)0; }
  void intersect(Bitmap<64>& map2) { map&= map2.map; }
  void intersect(ulonglong map2) { map&= map2; }
  void intersect_extended(ulonglong map2) { map&= map2; }
  void subtract(Bitmap<64>& map2) { map&= ~map2.map; }
  void merge(Bitmap<64>& map2) { map|= map2.map; }
  my_bool is_set(uint n) const { return ((map & (((ulonglong)1) << n)) ? 1 : 0); }
  my_bool is_prefix(uint n) const { return map == (((ulonglong)1) << n)-1; }
  my_bool is_clear_all() const { return map == (ulonglong)0; }
  my_bool is_set_all() const { return map == ~(ulonglong)0; }
  my_bool is_subset(const Bitmap<64>& map2) const { return !(map & ~map2.map); }
  my_bool is_overlapping(const Bitmap<64>& map2) const { return (map & map2.map)!= 0; }
  my_bool operator==(const Bitmap<64>& map2) const { return map == map2.map; }
  char *print(char *buf) const { longlong2str(map,buf,16); return buf; }
  ulonglong to_ulonglong() const { return map; }
};
#include "sql_array.h"
#include <my_sys.h>
template <class Elem> class Dynamic_array
{
  DYNAMIC_ARRAY array;
public:
  Dynamic_array(uint prealloc=16, uint increment=16)
  {
    init_dynamic_array2(&array,sizeof(Elem),NULL,prealloc,increment );
  }
  Elem& at(int idx)
  {
    return *(((Elem*)array.buffer) + idx);
  }
  Elem *front()
  {
    return (Elem*)array.buffer;
  }
  Elem *back()
  {
    return ((Elem*)array.buffer) + array.elements;
  }
  In_C_you_should_use_my_bool_instead() append(Elem &el)
  {
    return (insert_dynamic(&array, (uchar*)&el));
  }
  int elements()
  {
    return array.elements;
  }
  ~Dynamic_array()
  {
    delete_dynamic(&array);
  }
  typedef int (*CMP_FUNC)(const Elem *el1, const Elem *el2);
  void sort(CMP_FUNC cmp_func)
  {
    my_qsort(array.buffer, array.elements, sizeof(Elem), (qsort_cmp)cmp_func);
  }
};
#include "sql_plugin.h"
class sys_var;
#include <mysql/plugin.h>
typedef struct st_mysql_lex_string MYSQL_LEX_STRING;
struct st_mysql_xid {
  long formatID;
  long gtrid_length;
  long bqual_length;
  char data[128];
};
typedef struct st_mysql_xid MYSQL_XID;
enum enum_mysql_show_type
{
  SHOW_UNDEF, SHOW_BOOL, SHOW_INT, SHOW_LONG,
  SHOW_LONGLONG, SHOW_CHAR, SHOW_CHAR_PTR,
  SHOW_ARRAY, SHOW_FUNC, SHOW_KEY_CACHE_LONG, SHOW_KEY_CACHE_LONGLONG, SHOW_LONG_STATUS, SHOW_DOUBLE_STATUS, SHOW_HAVE, SHOW_MY_BOOL, SHOW_HA_ROWS, SHOW_SYS, SHOW_LONG_NOFLUSH, SHOW_LONGLONG_STATUS, SHOW_DOUBLE
};
struct st_mysql_show_var {
  const char *name;
  char *value;
  enum enum_mysql_show_type type;
};
typedef int (*mysql_show_var_func)(void*, struct st_mysql_show_var*, char *);
struct st_mysql_sys_var;
struct st_mysql_value;
typedef int (*mysql_var_check_func)(void* thd,
                                    struct st_mysql_sys_var *var,
                                    void *save, struct st_mysql_value *value);
typedef void (*mysql_var_update_func)(void* thd,
                                      struct st_mysql_sys_var *var,
                                      void *var_ptr, const void *save);
struct st_mysql_plugin
{
  int type;
  void *info;
  const char *name;
  const char *author;
  const char *descr;
  int license;
  int (*init)(void *);
  int (*deinit)(void *);
  unsigned int version;
  struct st_mysql_show_var *status_vars;
  struct st_mysql_sys_var **system_vars;
  void * __reserved1;
};
enum enum_ftparser_mode
{
  MYSQL_FTPARSER_SIMPLE_MODE= 0,
  MYSQL_FTPARSER_WITH_STOPWORDS= 1,
  MYSQL_FTPARSER_FULL_BOOLEAN_INFO= 2
};
enum enum_ft_token_type
{
  FT_TOKEN_EOF= 0,
  FT_TOKEN_WORD= 1,
  FT_TOKEN_LEFT_PAREN= 2,
  FT_TOKEN_RIGHT_PAREN= 3,
  FT_TOKEN_STOPWORD= 4
};
typedef struct st_mysql_ftparser_boolean_info
{
  enum enum_ft_token_type type;
  int yesno;
  int weight_adjust;
  char wasign;
  char trunc;
  char prev;
  char *quot;
} MYSQL_FTPARSER_BOOLEAN_INFO;
typedef struct st_mysql_ftparser_param
{
  int (*mysql_parse)(struct st_mysql_ftparser_param *,
                     char *doc, int doc_len);
  int (*mysql_add_word)(struct st_mysql_ftparser_param *,
                        char *word, int word_len,
                        MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info);
  void *ftparser_state;
  void *mysql_ftparam;
  struct charset_info_st *cs;
  char *doc;
  int length;
  int flags;
  enum enum_ftparser_mode mode;
} MYSQL_FTPARSER_PARAM;
struct st_mysql_ftparser
{
  int interface_version;
  int (*parse)(MYSQL_FTPARSER_PARAM *param);
  int (*init)(MYSQL_FTPARSER_PARAM *param);
  int (*deinit)(MYSQL_FTPARSER_PARAM *param);
};
struct st_mysql_storage_engine
{
  int interface_version;
};
struct handlerton;
struct st_mysql_daemon
{
  int interface_version;
};
struct st_mysql_information_schema
{
  int interface_version;
};
struct st_mysql_value
{
  int (*value_type)(struct st_mysql_value *);
  const char *(*val_str)(struct st_mysql_value *, char *buffer, int *length);
  int (*val_real)(struct st_mysql_value *, double *realbuf);
  int (*val_int)(struct st_mysql_value *, long long *intbuf);
};
int thd_in_lock_tables(const void* thd);
int thd_tablespace_op(const void* thd);
long long thd_test_options(const void* thd, long long test_options);
int thd_sql_command(const void* thd);
const char *thd_proc_info(void* thd, const char *info);
void **thd_ha_data(const void* thd, const struct handlerton *hton);
int thd_tx_isolation(const void* thd);
char *thd_security_context(void* thd, char *buffer, unsigned int length,
                           unsigned int max_query_len);
void thd_inc_row_count(void* thd);
int mysql_tmpfile(const char *prefix);
int thd_killed(const void* thd);
unsigned long thd_get_thread_id(const void* thd);
void *thd_alloc(void* thd, unsigned int size);
void *thd_calloc(void* thd, unsigned int size);
char *thd_strdup(void* thd, const char *str);
char *thd_strmake(void* thd, const char *str, unsigned int size);
void *thd_memdup(void* thd, const void* str, unsigned int size);
MYSQL_LEX_STRING *thd_make_lex_string(void* thd, MYSQL_LEX_STRING *lex_str,
                                      const char *str, unsigned int size,
                                      int allocate_lex_string);
void thd_get_xid(const void* thd, MYSQL_XID *xid);
void mysql_query_cache_invalidate4(void* thd,
                                   const char *key, unsigned int key_length,
                                   int using_trx);
typedef enum enum_mysql_show_type SHOW_TYPE;
typedef struct st_mysql_show_var SHOW_VAR;
struct st_plugin_dl
{
  LEX_STRING dl;
  void *handle;
  struct st_mysql_plugin *plugins;
  int version;
  uint ref_count;
};
struct st_plugin_int
{
  LEX_STRING name;
  struct st_mysql_plugin *plugin;
  struct st_plugin_dl *plugin_dl;
  uint state;
  uint ref_count;
  void *data;
  MEM_ROOT mem_root;
  sys_var *system_vars;
};
typedef struct st_plugin_int **plugin_ref;
typedef int (*plugin_type_init)(struct st_plugin_int *);
extern char *opt_plugin_load;
extern char *opt_plugin_dir_ptr;
extern char opt_plugin_dir[512];
extern const LEX_STRING plugin_type_names[];
extern int plugin_init(int *argc, char **argv, int init_flags);
extern void plugin_shutdown(void);
extern void my_print_help_inc_plugins(struct my_option *options, uint size);
extern In_C_you_should_use_my_bool_instead() plugin_is_ready(const LEX_STRING *name, int type);
extern plugin_ref plugin_lock(THD *thd, plugin_ref *ptr );
extern plugin_ref plugin_lock_by_name(THD *thd, const LEX_STRING *name,
                                      int type );
extern void plugin_unlock(THD *thd, plugin_ref plugin);
extern void plugin_unlock_list(THD *thd, plugin_ref *list, uint count);
extern In_C_you_should_use_my_bool_instead() mysql_install_plugin(THD *thd, const LEX_STRING *name,
                                 const LEX_STRING *dl);
extern In_C_you_should_use_my_bool_instead() mysql_uninstall_plugin(THD *thd, const LEX_STRING *name);
extern In_C_you_should_use_my_bool_instead() plugin_register_builtin(struct st_mysql_plugin *plugin);
extern void plugin_thdvar_init(THD *thd);
extern void plugin_thdvar_cleanup(THD *thd);
typedef my_bool (plugin_foreach_func)(THD *thd,
                                      plugin_ref plugin,
                                      void *arg);
extern In_C_you_should_use_my_bool_instead() plugin_foreach_with_mask(THD *thd, plugin_foreach_func *func,
                                     int type, uint state_mask, void *arg);
#include "scheduler.h"
class THD;
class scheduler_functions
{
public:
  uint max_threads;
  In_C_you_should_use_my_bool_instead() (*init)(void);
  In_C_you_should_use_my_bool_instead() (*init_new_connection_thread)(void);
  void (*add_connection)(THD *thd);
  void (*post_kill_notification)(THD *thd);
  In_C_you_should_use_my_bool_instead() (*end_thread)(THD *thd, In_C_you_should_use_my_bool_instead() cache_thread);
  void (*end)(void);
  scheduler_functions();
};
enum scheduler_types
{
  SCHEDULER_ONE_THREAD_PER_CONNECTION=0,
  SCHEDULER_NO_THREADS,
  SCHEDULER_POOL_OF_THREADS
};
void one_thread_per_connection_scheduler(scheduler_functions* func);
void one_thread_scheduler(scheduler_functions* func);
enum pool_command_op
{
  NOT_IN_USE_OP= 0, NORMAL_OP= 1, CONNECT_OP, KILL_OP, DIE_OP
};
class thd_scheduler
{};
enum enum_query_type
{
  QT_ORDINARY,
  QT_IS
};
typedef ulonglong table_map;
typedef Bitmap<64> key_map;
typedef ulong nesting_map;
typedef ulonglong nested_join_map;
typedef ulonglong query_id_t;
extern query_id_t global_query_id;
inline query_id_t next_query_id() { return global_query_id++; }
extern const key_map key_map_empty;
extern key_map key_map_full;
extern const char *primary_key_name;
#include "mysql_com.h"
enum enum_server_command
{
  COM_SLEEP, COM_QUIT, COM_INIT_DB, COM_QUERY, COM_FIELD_LIST,
  COM_CREATE_DB, COM_DROP_DB, COM_REFRESH, COM_SHUTDOWN, COM_STATISTICS,
  COM_PROCESS_INFO, COM_CONNECT, COM_PROCESS_KILL, COM_DEBUG, COM_PING,
  COM_TIME, COM_DELAYED_INSERT, COM_CHANGE_USER, COM_BINLOG_DUMP,
  COM_TABLE_DUMP, COM_CONNECT_OUT, COM_REGISTER_SLAVE,
  COM_STMT_PREPARE, COM_STMT_EXECUTE, COM_STMT_SEND_LONG_DATA, COM_STMT_CLOSE,
  COM_STMT_RESET, COM_SET_OPTION, COM_STMT_FETCH, COM_DAEMON,
  COM_END
};
struct st_vio;
typedef struct st_vio Vio;
typedef struct st_net {
  Vio *vio;
  unsigned char *buff,*buff_end,*write_pos,*read_pos;
  my_socket fd;
  unsigned long remain_in_buf,length, buf_length, where_b;
  unsigned long max_packet,max_packet_size;
  unsigned int pkt_nr,compress_pkt_nr;
  unsigned int write_timeout, read_timeout, retry_count;
  int fcntl;
  unsigned int *return_status;
  unsigned char reading_or_writing;
  char save_char;
  my_bool unused0;
  my_bool unused;
  my_bool compress;
  my_bool unused1;
  unsigned char *query_cache_query;
  unsigned int last_errno;
  unsigned char error;
  my_bool unused2;
  my_bool return_errno;
  char last_error[512];
  char sqlstate[5 +1];
  void *extension;
} NET;
enum enum_field_types { MYSQL_TYPE_DECIMAL, MYSQL_TYPE_TINY,
   MYSQL_TYPE_SHORT, MYSQL_TYPE_LONG,
   MYSQL_TYPE_FLOAT, MYSQL_TYPE_DOUBLE,
   MYSQL_TYPE_NULL, MYSQL_TYPE_TIMESTAMP,
   MYSQL_TYPE_LONGLONG,MYSQL_TYPE_INT24,
   MYSQL_TYPE_DATE, MYSQL_TYPE_TIME,
   MYSQL_TYPE_DATETIME, MYSQL_TYPE_YEAR,
   MYSQL_TYPE_NEWDATE, MYSQL_TYPE_VARCHAR,
   MYSQL_TYPE_BIT,
                        MYSQL_TYPE_NEWDECIMAL=246,
   MYSQL_TYPE_ENUM=247,
   MYSQL_TYPE_SET=248,
   MYSQL_TYPE_TINY_BLOB=249,
   MYSQL_TYPE_MEDIUM_BLOB=250,
   MYSQL_TYPE_LONG_BLOB=251,
   MYSQL_TYPE_BLOB=252,
   MYSQL_TYPE_VAR_STRING=253,
   MYSQL_TYPE_STRING=254,
   MYSQL_TYPE_GEOMETRY=255
};
enum mysql_enum_shutdown_level {
  SHUTDOWN_DEFAULT = 0,
  SHUTDOWN_WAIT_CONNECTIONS= (unsigned char)(1 << 0),
  SHUTDOWN_WAIT_TRANSACTIONS= (unsigned char)(1 << 1),
  SHUTDOWN_WAIT_UPDATES= (unsigned char)(1 << 3),
  SHUTDOWN_WAIT_ALL_BUFFERS= ((unsigned char)(1 << 3) << 1),
  SHUTDOWN_WAIT_CRITICAL_BUFFERS= ((unsigned char)(1 << 3) << 1) + 1,
  KILL_QUERY= 254,
  KILL_CONNECTION= 255
};
enum enum_cursor_type
{
  CURSOR_TYPE_NO_CURSOR= 0,
  CURSOR_TYPE_READ_ONLY= 1,
  CURSOR_TYPE_FOR_UPDATE= 2,
  CURSOR_TYPE_SCROLLABLE= 4
};
enum enum_mysql_set_option
{
  MYSQL_OPTION_MULTI_STATEMENTS_ON,
  MYSQL_OPTION_MULTI_STATEMENTS_OFF
};
my_bool my_net_init(NET *net, Vio* vio);
void my_net_local_init(NET *net);
void net_end(NET *net);
  void net_clear(NET *net, my_bool clear_buffer);
my_bool net_realloc(NET *net, size_t length);
my_bool net_flush(NET *net);
my_bool my_net_write(NET *net,const unsigned char *packet, size_t len);
my_bool net_write_command(NET *net,unsigned char command,
     const unsigned char *header, size_t head_len,
     const unsigned char *packet, size_t len);
int net_real_write(NET *net,const unsigned char *packet, size_t len);
unsigned long my_net_read(NET *net);
void my_net_set_write_timeout(NET *net, uint timeout);
void my_net_set_read_timeout(NET *net, uint timeout);
struct sockaddr;
int my_connect(my_socket s, const struct sockaddr *name, unsigned int namelen,
        unsigned int timeout);
struct rand_struct {
  unsigned long seed1,seed2,max_value;
  double max_value_dbl;
};
enum Item_result {STRING_RESULT=0, REAL_RESULT, INT_RESULT, ROW_RESULT,
                  DECIMAL_RESULT};
typedef struct st_udf_args
{
  unsigned int arg_count;
  enum Item_result *arg_type;
  char **args;
  unsigned long *lengths;
  char *maybe_null;
  char **attributes;
  unsigned long *attribute_lengths;
  void *extension;
} UDF_ARGS;
typedef struct st_udf_init
{
  my_bool maybe_null;
  unsigned int decimals;
  unsigned long max_length;
  char *ptr;
  my_bool const_item;
  void *extension;
} UDF_INIT;
void randominit(struct rand_struct *, unsigned long seed1,
                unsigned long seed2);
double my_rnd(struct rand_struct *);
void create_random_string(char *to, unsigned int length, struct rand_struct *rand_st);
void hash_password(unsigned long *to, const char *password, unsigned int password_len);
void make_scrambled_password_323(char *to, const char *password);
void scramble_323(char *to, const char *message, const char *password);
my_bool check_scramble_323(const char *, const char *message,
                           unsigned long *salt);
void get_salt_from_password_323(unsigned long *res, const char *password);
void make_password_from_salt_323(char *to, const unsigned long *salt);
void make_scrambled_password(char *to, const char *password);
void scramble(char *to, const char *message, const char *password);
my_bool check_scramble(const char *reply, const char *message,
                       const unsigned char *hash_stage2);
void get_salt_from_password(unsigned char *res, const char *password);
void make_password_from_salt(char *to, const unsigned char *hash_stage2);
char *octet2hex(char *to, const char *str, unsigned int len);
char *get_tty_password(const char *opt_message);
const char *mysql_errno_to_sqlstate(unsigned int mysql_errno);
my_bool my_thread_init(void);
void my_thread_end(void);
ulong net_field_length(uchar **packet);
my_ulonglong net_field_length_ll(uchar **packet);
uchar *net_store_length(uchar *pkg, ulonglong length);
#include <violite.h>
#include "my_net.h"
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
void my_inet_ntoa(struct in_addr in, char *buf);
struct hostent;
struct hostent *my_gethostbyname_r(const char *name,
       struct hostent *result, char *buffer,
       int buflen, int *h_errnop);
enum enum_vio_type
{
  VIO_CLOSED, VIO_TYPE_TCPIP, VIO_TYPE_SOCKET, VIO_TYPE_NAMEDPIPE,
  VIO_TYPE_SSL, VIO_TYPE_SHARED_MEMORY
};
Vio* vio_new(my_socket sd, enum enum_vio_type type, uint flags);
void vio_delete(Vio* vio);
int vio_close(Vio* vio);
void vio_reset(Vio* vio, enum enum_vio_type type,
                  my_socket sd, void * hPipe, uint flags);
size_t vio_read(Vio *vio, uchar * buf, size_t size);
size_t vio_read_buff(Vio *vio, uchar * buf, size_t size);
size_t vio_write(Vio *vio, const uchar * buf, size_t size);
int vio_blocking(Vio *vio, my_bool onoff, my_bool *old_mode);
my_bool vio_is_blocking(Vio *vio);
int vio_fastsend(Vio *vio);
int vio_keepalive(Vio *vio, my_bool onoff);
my_bool vio_should_retry(Vio *vio);
my_bool vio_was_interrupted(Vio *vio);
const char* vio_description(Vio *vio);
enum enum_vio_type vio_type(Vio* vio);
int vio_errno(Vio*vio);
my_socket vio_fd(Vio*vio);
my_bool vio_peer_addr(Vio* vio, char *buf, uint16 *port);
void vio_in_addr(Vio *vio, struct in_addr *in);
my_bool vio_poll_read(Vio *vio,uint timeout);
void vio_end(void);
enum SSL_type
{
  SSL_TYPE_NOT_SPECIFIED= -1,
  SSL_TYPE_NONE,
  SSL_TYPE_ANY,
  SSL_TYPE_X509,
  SSL_TYPE_SPECIFIED
};
struct st_vio
{
  my_socket sd;
  void * hPipe;
  my_bool localhost;
  int fcntl_mode;
  struct sockaddr_in local;
  struct sockaddr_in remote;
  enum enum_vio_type type;
  char desc[30];
  char *read_buffer;
  char *read_pos;
  char *read_end;
  void (*viodelete)(Vio*);
  int (*vioerrno)(Vio*);
  size_t (*read)(Vio*, uchar *, size_t);
  size_t (*write)(Vio*, const uchar *, size_t);
  int (*vioblocking)(Vio*, my_bool, my_bool *);
  my_bool (*is_blocking)(Vio*);
  int (*viokeepalive)(Vio*, my_bool);
  int (*fastsend)(Vio*);
  my_bool (*peer_addr)(Vio*, char *, uint16*);
  void (*in_addr)(Vio*, struct in_addr*);
  my_bool (*should_retry)(Vio*);
  my_bool (*was_interrupted)(Vio*);
  int (*vioclose)(Vio*);
  void (*timeout)(Vio*, unsigned int which, unsigned int timeout);
};
#include "unireg.h"
#include "mysqld_error.h"
#include "structs.h"
struct st_table;
class Field;
typedef struct st_date_time_format {
  uchar positions[8];
  char time_separator;
  uint flag;
  LEX_STRING format;
} DATE_TIME_FORMAT;
typedef struct st_keyfile_info {
  uchar ref[8];
  uchar dupp_ref[8];
  uint ref_length;
  uint block_size;
  File filenr;
  ha_rows records;
  ha_rows deleted;
  ulonglong data_file_length;
  ulonglong max_data_file_length;
  ulonglong index_file_length;
  ulonglong max_index_file_length;
  ulonglong delete_length;
  ulonglong auto_increment_value;
  int errkey,sortkey;
  time_t create_time;
  time_t check_time;
  time_t update_time;
  ulong mean_rec_length;
} KEYFILE_INFO;
typedef struct st_key_part_info {
  Field *field;
  uint offset;
  uint null_offset;
  uint16 length;
  uint16 store_length;
  uint16 key_type;
  uint16 fieldnr;
  uint16 key_part_flag;
  uint8 type;
  uint8 null_bit;
} KEY_PART_INFO ;
typedef struct st_key {
  uint key_length;
  ulong flags;
  uint key_parts;
  uint extra_length;
  uint usable_key_parts;
  uint block_size;
  enum ha_key_alg algorithm;
  union
  {
    plugin_ref parser;
    LEX_STRING *parser_name;
  };
  KEY_PART_INFO *key_part;
  char *name;
  ulong *rec_per_key;
  union {
    int bdb_return_if_eq;
  } handler;
  struct st_table *table;
} KEY;
struct st_join_table;
typedef struct st_reginfo {
  struct st_join_table *join_tab;
  enum thr_lock_type lock_type;
  In_C_you_should_use_my_bool_instead() not_exists_optimize;
  In_C_you_should_use_my_bool_instead() impossible_range;
} REGINFO;
struct st_read_record;
class SQL_SELECT;
class THD;
class handler;
typedef struct st_read_record {
  struct st_table *table;
  handler *file;
  struct st_table **forms;
  int (*read_record)(struct st_read_record *);
  THD *thd;
  SQL_SELECT *select;
  uint cache_records;
  uint ref_length,struct_length,reclength,rec_cache_size,error_offset;
  uint index;
  uchar *ref_pos;
  uchar *record;
  uchar *rec_buf;
  uchar *cache,*cache_pos,*cache_end,*read_positions;
  IO_CACHE *io_cache;
  In_C_you_should_use_my_bool_instead() print_error, ignore_not_found_rows;
} READ_RECORD;
typedef enum enum_mysql_timestamp_type timestamp_type;
typedef struct {
  ulong year,month,day,hour;
  ulonglong minute,second,second_part;
  In_C_you_should_use_my_bool_instead() neg;
} INTERVAL;
typedef struct st_known_date_time_format {
  const char *format_name;
  const char *date_format;
  const char *datetime_format;
  const char *time_format;
} KNOWN_DATE_TIME_FORMAT;
enum SHOW_COMP_OPTION { SHOW_OPTION_YES, SHOW_OPTION_NO, SHOW_OPTION_DISABLED};
extern const char *show_comp_option_name[];
typedef int *(*update_var)(THD *, struct st_mysql_show_var *);
typedef struct st_lex_user {
  LEX_STRING user, host, password;
} LEX_USER;
typedef struct user_resources {
  uint questions;
  uint updates;
  uint conn_per_hour;
  uint user_conn;
  enum {QUERIES_PER_HOUR= 1, UPDATES_PER_HOUR= 2, CONNECTIONS_PER_HOUR= 4,
        USER_CONNECTIONS= 8};
  uint specified_limits;
} USER_RESOURCES;
typedef struct user_conn {
  char *user;
  char *host;
  ulonglong reset_utime;
  uint len;
  uint connections;
  uint conn_per_hour, updates, questions;
  USER_RESOURCES user_resources;
} USER_CONN;
class Discrete_interval {
private:
  ulonglong interval_min;
  ulonglong interval_values;
  ulonglong interval_max;
public:
  Discrete_interval *next;
  void replace(ulonglong start, ulonglong val, ulonglong incr)
  {
    interval_min= start;
    interval_values= val;
    interval_max= (val == ((unsigned long long)(~0ULL))) ? val : start + val * incr;
  }
  Discrete_interval(ulonglong start, ulonglong val, ulonglong incr) :
    next(NULL) { replace(start, val, incr); };
  Discrete_interval() : next(NULL) { replace(0, 0, 0); };
  ulonglong minimum() const { return interval_min; };
  ulonglong values() const { return interval_values; };
  ulonglong maximum() const { return interval_max; };
  In_C_you_should_use_my_bool_instead() merge_if_contiguous(ulonglong start, ulonglong val, ulonglong incr)
  {
    if (interval_max == start)
    {
      if (val == ((unsigned long long)(~0ULL)))
      {
        interval_values= interval_max= val;
      }
      else
      {
        interval_values+= val;
        interval_max= start + val * incr;
      }
      return 0;
    }
    return 1;
  };
};
class Discrete_intervals_list {
private:
  Discrete_interval *head;
  Discrete_interval *tail;
  Discrete_interval *current;
  uint elements;
  void copy_(const Discrete_intervals_list& from)
  {
    for (Discrete_interval *i= from.head; i; i= i->next)
    {
      Discrete_interval j= *i;
      append(&j);
    }
  }
public:
  Discrete_intervals_list() : head(NULL), current(NULL), elements(0) {};
  Discrete_intervals_list(const Discrete_intervals_list& from)
  {
    copy_(from);
  }
  void operator=(const Discrete_intervals_list& from)
  {
    empty();
    copy_(from);
  }
  void empty_no_free()
  {
    head= current= NULL;
    elements= 0;
  }
  void empty()
  {
    for (Discrete_interval *i= head; i;)
    {
      Discrete_interval *next= i->next;
      delete i;
      i= next;
    }
    empty_no_free();
  }
  const Discrete_interval* get_next()
  {
    Discrete_interval *tmp= current;
    if (current != NULL)
      current= current->next;
    return tmp;
  }
  ~Discrete_intervals_list() { empty(); };
  In_C_you_should_use_my_bool_instead() append(ulonglong start, ulonglong val, ulonglong incr);
  In_C_you_should_use_my_bool_instead() append(Discrete_interval *interval);
  ulonglong minimum() const { return (head ? head->minimum() : 0); };
  ulonglong maximum() const { return (head ? tail->maximum() : 0); };
  uint nb_elements() const { return elements; }
};
void init_sql_alloc(MEM_ROOT *root, uint block_size, uint pre_alloc_size);
void *sql_alloc(size_t);
void *sql_calloc(size_t);
char *sql_strdup(const char *str);
char *sql_strmake(const char *str, size_t len);
void *sql_memdup(const void * ptr, size_t size);
void sql_element_free(void *ptr);
char *sql_strmake_with_convert(const char *str, size_t arg_length,
          CHARSET_INFO *from_cs,
          size_t max_res_length,
          CHARSET_INFO *to_cs, size_t *result_length);
uint kill_one_thread(THD *thd, ulong id, In_C_you_should_use_my_bool_instead() only_kill_query);
void sql_kill(THD *thd, ulong id, In_C_you_should_use_my_bool_instead() only_kill_query);
In_C_you_should_use_my_bool_instead() net_request_file(NET* net, const char* fname);
char* query_table_status(THD *thd,const char *db,const char *table_name);
extern CHARSET_INFO *system_charset_info, *files_charset_info ;
extern CHARSET_INFO *national_charset_info, *table_alias_charset;
enum Derivation
{
  DERIVATION_IGNORABLE= 5,
  DERIVATION_COERCIBLE= 4,
  DERIVATION_SYSCONST= 3,
  DERIVATION_IMPLICIT= 2,
  DERIVATION_NONE= 1,
  DERIVATION_EXPLICIT= 0
};
typedef struct my_locale_st
{
  uint number;
  const char *name;
  const char *description;
  const In_C_you_should_use_my_bool_instead() is_ascii;
  TYPELIB *month_names;
  TYPELIB *ab_month_names;
  TYPELIB *day_names;
  TYPELIB *ab_day_names;
} MY_LOCALE;
extern MY_LOCALE my_locale_en_US;
extern MY_LOCALE *my_locales[];
extern MY_LOCALE *my_default_lc_time_names;
MY_LOCALE *my_locale_by_name(const char *name);
MY_LOCALE *my_locale_by_number(uint number);
class Object_creation_ctx
{
public:
  Object_creation_ctx *set_n_backup(THD *thd);
  void restore_env(THD *thd, Object_creation_ctx *backup_ctx);
protected:
  Object_creation_ctx() {}
  virtual Object_creation_ctx *create_backup_ctx(THD *thd) const = 0;
  virtual void change_env(THD *thd) const = 0;
public:
  virtual ~Object_creation_ctx()
  { }
};
class Default_object_creation_ctx : public Object_creation_ctx
{
public:
  CHARSET_INFO *get_client_cs()
  {
    return m_client_cs;
  }
  CHARSET_INFO *get_connection_cl()
  {
    return m_connection_cl;
  }
protected:
  Default_object_creation_ctx(THD *thd);
  Default_object_creation_ctx(CHARSET_INFO *client_cs,
                              CHARSET_INFO *connection_cl);
protected:
  virtual Object_creation_ctx *create_backup_ctx(THD *thd) const;
  virtual void change_env(THD *thd) const;
protected:
  CHARSET_INFO *m_client_cs;
  CHARSET_INFO *m_connection_cl;
};
struct TABLE_LIST;
class String;
void view_store_options(THD *thd, TABLE_LIST *table, String *buff);
enum enum_parsing_place
{
  NO_MATTER,
  IN_HAVING,
  SELECT_LIST,
  IN_WHERE,
  IN_ON
};
struct st_table;
class THD;
enum enum_check_fields
{
  CHECK_FIELD_IGNORE,
  CHECK_FIELD_WARN,
  CHECK_FIELD_ERROR_FOR_NULL
};
typedef struct st_sql_list {
  uint elements;
  uchar *first;
  uchar **next;
  st_sql_list() {}
  inline void empty()
  {
    elements=0;
    first=0;
    next= &first;
  }
  inline void link_in_list(uchar *element,uchar **next_ptr)
  {
    elements++;
    (*next)=element;
    next= next_ptr;
    *next=0;
  }
  inline void save_and_clear(struct st_sql_list *save)
  {
    *save= *this;
    empty();
  }
  inline void push_front(struct st_sql_list *save)
  {
    *save->next= first;
    first= save->first;
    elements+= save->elements;
  }
  inline void push_back(struct st_sql_list *save)
  {
    if (save->first)
    {
      *next= save->first;
      next= save->next;
      elements+= save->elements;
    }
  }
} SQL_LIST;
extern pthread_key_t THR_THD;
inline THD *_current_thd(void)
{
  return ((THD*) pthread_getspecific((THR_THD)));
}
extern "C"
const char *set_thd_proc_info(THD *thd, const char *info,
                              const char *calling_func,
                              const char *calling_file,
                              const unsigned int calling_line);
enum enum_table_ref_type
{
  TABLE_REF_NULL= 0,
  TABLE_REF_VIEW,
  TABLE_REF_BASE_TABLE,
  TABLE_REF_I_S_TABLE,
  TABLE_REF_TMP_TABLE
};
extern ulong server_id, concurrency;
typedef my_bool (*qc_engine_callback)(THD *thd, char *table_key,
                                      uint key_length,
                                      ulonglong *engine_data);
#include "sql_string.h"
class String;
int sortcmp(const String *a,const String *b, CHARSET_INFO *cs);
String *copy_if_not_alloced(String *a,String *b,uint32 arg_length);
uint32 copy_and_convert(char *to, uint32 to_length, CHARSET_INFO *to_cs,
   const char *from, uint32 from_length,
   CHARSET_INFO *from_cs, uint *errors);
uint32 well_formed_copy_nchars(CHARSET_INFO *to_cs,
                               char *to, uint to_length,
                               CHARSET_INFO *from_cs,
                               const char *from, uint from_length,
                               uint nchars,
                               const char **well_formed_error_pos,
                               const char **cannot_convert_error_pos,
                               const char **from_end_pos);
size_t my_copy_with_hex_escaping(CHARSET_INFO *cs,
                                 char *dst, size_t dstlen,
                                 const char *src, size_t srclen);
class String
{
  char *Ptr;
  uint32 str_length,Alloced_length;
  In_C_you_should_use_my_bool_instead() alloced;
  CHARSET_INFO *str_charset;
public:
  String()
  {
    Ptr=0; str_length=Alloced_length=0; alloced=0;
    str_charset= &my_charset_bin;
  }
  String(uint32 length_arg)
  {
    alloced=0; Alloced_length=0; (void) real_alloc(length_arg);
    str_charset= &my_charset_bin;
  }
  String(const char *str, CHARSET_INFO *cs)
  {
    Ptr=(char*) str; str_length=(uint) strlen(str); Alloced_length=0; alloced=0;
    str_charset=cs;
  }
  String(const char *str,uint32 len, CHARSET_INFO *cs)
  {
    Ptr=(char*) str; str_length=len; Alloced_length=0; alloced=0;
    str_charset=cs;
  }
  String(char *str,uint32 len, CHARSET_INFO *cs)
  {
    Ptr=(char*) str; Alloced_length=str_length=len; alloced=0;
    str_charset=cs;
  }
  String(const String &str)
  {
    Ptr=str.Ptr ; str_length=str.str_length ;
    Alloced_length=str.Alloced_length; alloced=0;
    str_charset=str.str_charset;
  }
  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return (void*) alloc_root(mem_root, (uint) size); }
  static void operator delete(void *ptr_arg,size_t size)
  { ; }
  static void operator delete(void *ptr_arg, MEM_ROOT *mem_root)
  { }
  ~String() { free(); }
  inline void set_charset(CHARSET_INFO *charset_arg)
  { str_charset= charset_arg; }
  inline CHARSET_INFO *charset() const { return str_charset; }
  inline uint32 length() const { return str_length;}
  inline uint32 alloced_length() const { return Alloced_length;}
  inline char& operator [] (uint32 i) const { return Ptr[i]; }
  inline void length(uint32 len) { str_length=len ; }
  inline In_C_you_should_use_my_bool_instead() is_empty() { return (str_length == 0); }
  inline void mark_as_const() { Alloced_length= 0;}
  inline const char *ptr() const { return Ptr; }
  inline char *c_ptr()
  {
    if (!Ptr || Ptr[str_length])
      (void) realloc(str_length);
    return Ptr;
  }
  inline char *c_ptr_quick()
  {
    if (Ptr && str_length < Alloced_length)
      Ptr[str_length]=0;
    return Ptr;
  }
  inline char *c_ptr_safe()
  {
    if (Ptr && str_length < Alloced_length)
      Ptr[str_length]=0;
    else
      (void) realloc(str_length);
    return Ptr;
  }
  void set(String &str,uint32 offset,uint32 arg_length)
  {
    assert(&str != this);
    free();
    Ptr=(char*) str.ptr()+offset; str_length=arg_length; alloced=0;
    if (str.Alloced_length)
      Alloced_length=str.Alloced_length-offset;
    else
      Alloced_length=0;
    str_charset=str.str_charset;
  }
  inline void set(char *str,uint32 arg_length, CHARSET_INFO *cs)
  {
    free();
    Ptr=(char*) str; str_length=Alloced_length=arg_length ; alloced=0;
    str_charset=cs;
  }
  inline void set(const char *str,uint32 arg_length, CHARSET_INFO *cs)
  {
    free();
    Ptr=(char*) str; str_length=arg_length; Alloced_length=0 ; alloced=0;
    str_charset=cs;
  }
  In_C_you_should_use_my_bool_instead() set_ascii(const char *str, uint32 arg_length);
  inline void set_quick(char *str,uint32 arg_length, CHARSET_INFO *cs)
  {
    if (!alloced)
    {
      Ptr=(char*) str; str_length=Alloced_length=arg_length;
    }
    str_charset=cs;
  }
  In_C_you_should_use_my_bool_instead() set_int(longlong num, In_C_you_should_use_my_bool_instead() unsigned_flag, CHARSET_INFO *cs);
  In_C_you_should_use_my_bool_instead() set(longlong num, CHARSET_INFO *cs)
  { return set_int(num, false, cs); }
  In_C_you_should_use_my_bool_instead() set(ulonglong num, CHARSET_INFO *cs)
  { return set_int((longlong)num, true, cs); }
  In_C_you_should_use_my_bool_instead() set_real(double num,uint decimals, CHARSET_INFO *cs);
  inline void chop()
  {
    Ptr[str_length--]= '\0';
  }
  inline void free()
  {
    if (alloced)
    {
      alloced=0;
      Alloced_length=0;
      ((void)(myf) (0),my_no_flags_free(Ptr));
      Ptr=0;
      str_length=0;
    }
  }
  inline In_C_you_should_use_my_bool_instead() alloc(uint32 arg_length)
  {
    if (arg_length < Alloced_length)
      return 0;
    return real_alloc(arg_length);
  }
  In_C_you_should_use_my_bool_instead() real_alloc(uint32 arg_length);
  In_C_you_should_use_my_bool_instead() realloc(uint32 arg_length);
  inline void shrink(uint32 arg_length)
  {
    if (arg_length < Alloced_length)
    {
      char *new_ptr;
      if (!(new_ptr=(char*) my_realloc(Ptr,arg_length,(myf) (0))))
      {
 Alloced_length = 0;
 real_alloc(arg_length);
      }
      else
      {
 Ptr=new_ptr;
 Alloced_length=arg_length;
      }
    }
  }
  In_C_you_should_use_my_bool_instead() is_alloced() { return alloced; }
  inline String& operator = (const String &s)
  {
    if (&s != this)
    {
      assert(!s.uses_buffer_owned_by(this));
      free();
      Ptr=s.Ptr ; str_length=s.str_length ; Alloced_length=s.Alloced_length;
      alloced=0;
    }
    return *this;
  }
  In_C_you_should_use_my_bool_instead() copy();
  In_C_you_should_use_my_bool_instead() copy(const String &s);
  In_C_you_should_use_my_bool_instead() copy(const char *s,uint32 arg_length, CHARSET_INFO *cs);
  static In_C_you_should_use_my_bool_instead() needs_conversion(uint32 arg_length,
            CHARSET_INFO *cs_from, CHARSET_INFO *cs_to,
          uint32 *offset);
  In_C_you_should_use_my_bool_instead() copy_aligned(const char *s, uint32 arg_length, uint32 offset,
      CHARSET_INFO *cs);
  In_C_you_should_use_my_bool_instead() set_or_copy_aligned(const char *s, uint32 arg_length, CHARSET_INFO *cs);
  In_C_you_should_use_my_bool_instead() copy(const char*s,uint32 arg_length, CHARSET_INFO *csfrom,
     CHARSET_INFO *csto, uint *errors);
  In_C_you_should_use_my_bool_instead() append(const String &s);
  In_C_you_should_use_my_bool_instead() append(const char *s);
  In_C_you_should_use_my_bool_instead() append(const char *s,uint32 arg_length);
  In_C_you_should_use_my_bool_instead() append(const char *s,uint32 arg_length, CHARSET_INFO *cs);
  In_C_you_should_use_my_bool_instead() append(IO_CACHE* file, uint32 arg_length);
  In_C_you_should_use_my_bool_instead() append_with_prefill(const char *s, uint32 arg_length,
      uint32 full_length, char fill_char);
  int strstr(const String &search,uint32 offset=0);
  int strrstr(const String &search,uint32 offset=0);
  In_C_you_should_use_my_bool_instead() replace(uint32 offset,uint32 arg_length,const char *to,uint32 length);
  In_C_you_should_use_my_bool_instead() replace(uint32 offset,uint32 arg_length,const String &to);
  inline In_C_you_should_use_my_bool_instead() append(char chr)
  {
    if (str_length < Alloced_length)
    {
      Ptr[str_length++]=chr;
    }
    else
    {
      if (realloc(str_length+1))
 return 1;
      Ptr[str_length++]=chr;
    }
    return 0;
  }
  In_C_you_should_use_my_bool_instead() fill(uint32 max_length,char fill);
  void strip_sp();
  friend int sortcmp(const String *a,const String *b, CHARSET_INFO *cs);
  friend int stringcmp(const String *a,const String *b);
  friend String *copy_if_not_alloced(String *a,String *b,uint32 arg_length);
  uint32 numchars();
  int charpos(int i,uint32 offset=0);
  int reserve(uint32 space_needed)
  {
    return realloc(str_length + space_needed);
  }
  int reserve(uint32 space_needed, uint32 grow_by);
  void q_append(const char c)
  {
    Ptr[str_length++] = c;
  }
  void q_append(const uint32 n)
  {
    *((long *) (Ptr + str_length))= (long) (n);
    str_length += 4;
  }
  void q_append(double d)
  {
    do { *((long *) (Ptr + str_length)) = ((doubleget_union *)&(d))->m[0]; *(((long *) (Ptr + str_length))+1) = ((doubleget_union *)&(d))->m[1]; } while (0);
    str_length += 8;
  }
  void q_append(double *d)
  {
    do { *((long *) (Ptr + str_length)) = ((doubleget_union *)&(*d))->m[0]; *(((long *) (Ptr + str_length))+1) = ((doubleget_union *)&(*d))->m[1]; } while (0);
    str_length += 8;
  }
  void q_append(const char *data, uint32 data_len)
  {
    memcpy(Ptr + str_length, data, data_len);
    str_length += data_len;
  }
  void write_at_position(int position, uint32 value)
  {
    *((long *) (Ptr + position))= (long) (value);
  }
  void qs_append(const char *str, uint32 len);
  void qs_append(double d);
  void qs_append(double *d);
  inline void qs_append(const char c)
  {
     Ptr[str_length]= c;
     str_length++;
  }
  void qs_append(int i);
  void qs_append(uint i);
  inline char *prep_append(uint32 arg_length, uint32 step_alloc)
  {
    uint32 new_length= arg_length + str_length;
    if (new_length > Alloced_length)
    {
      if (realloc(new_length + step_alloc))
        return 0;
    }
    uint32 old_length= str_length;
    str_length+= arg_length;
    return Ptr+ old_length;
  }
  inline In_C_you_should_use_my_bool_instead() append(const char *s, uint32 arg_length, uint32 step_alloc)
  {
    uint32 new_length= arg_length + str_length;
    if (new_length > Alloced_length && realloc(new_length + step_alloc))
      return (1);
    memcpy(Ptr+str_length, s, arg_length);
    str_length+= arg_length;
    return (0);
  }
  void print(String *print);
  void swap(String &s);
  inline In_C_you_should_use_my_bool_instead() uses_buffer_owned_by(const String *s) const
  {
    return (s->alloced && Ptr >= s->Ptr && Ptr < s->Ptr + s->str_length);
  }
};
static inline In_C_you_should_use_my_bool_instead() check_if_only_end_space(CHARSET_INFO *cs, char *str,
                                           char *end)
{
  return str+ cs->cset->scan(cs, str, end, 2) == end;
}
#include "sql_list.h"
class Sql_alloc
{
public:
  static void *operator new(size_t size) throw ()
  {
    return sql_alloc(size);
  }
  static void *operator new[](size_t size)
  {
    return sql_alloc(size);
  }
  static void *operator new[](size_t size, MEM_ROOT *mem_root) throw ()
  { return alloc_root(mem_root, size); }
  static void *operator new(size_t size, MEM_ROOT *mem_root) throw ()
  { return alloc_root(mem_root, size); }
  static void operator delete(void *ptr, size_t size) { ; }
  static void operator delete(void *ptr, MEM_ROOT *mem_root)
  { }
  static void operator delete[](void *ptr, MEM_ROOT *mem_root)
  { }
  static void operator delete[](void *ptr, size_t size) { ; }
  inline Sql_alloc() {}
  inline ~Sql_alloc() {}
};
struct list_node :public Sql_alloc
{
  list_node *next;
  void *info;
  list_node(void *info_par,list_node *next_par)
    :next(next_par),info(info_par)
  {}
  list_node()
  {
    info= 0;
    next= this;
  }
};
extern list_node end_of_list;
class base_list :public Sql_alloc
{
protected:
  list_node *first,**last;
public:
  uint elements;
  inline void empty() { elements=0; first= &end_of_list; last=&first;}
  inline base_list() { empty(); }
  inline base_list(const base_list &tmp) :Sql_alloc()
  {
    elements= tmp.elements;
    first= tmp.first;
    last= elements ? tmp.last : &first;
  }
  base_list(const base_list &rhs, MEM_ROOT *mem_root);
  inline base_list(In_C_you_should_use_my_bool_instead() error) { }
  inline In_C_you_should_use_my_bool_instead() push_back(void *info)
  {
    if (((*last)=new list_node(info, &end_of_list)))
    {
      last= &(*last)->next;
      elements++;
      return 0;
    }
    return 1;
  }
  inline In_C_you_should_use_my_bool_instead() push_back(void *info, MEM_ROOT *mem_root)
  {
    if (((*last)=new (mem_root) list_node(info, &end_of_list)))
    {
      last= &(*last)->next;
      elements++;
      return 0;
    }
    return 1;
  }
  inline In_C_you_should_use_my_bool_instead() push_front(void *info)
  {
    list_node *node=new list_node(info,first);
    if (node)
    {
      if (last == &first)
 last= &node->next;
      first=node;
      elements++;
      return 0;
    }
    return 1;
  }
  void remove(list_node **prev)
  {
    list_node *node=(*prev)->next;
    if (!--elements)
      last= &first;
    else if (last == &(*prev)->next)
      last= prev;
    delete *prev;
    *prev=node;
  }
  inline void concat(base_list *list)
  {
    if (!list->is_empty())
    {
      *last= list->first;
      last= list->last;
      elements+= list->elements;
    }
  }
  inline void *pop(void)
  {
    if (first == &end_of_list) return 0;
    list_node *tmp=first;
    first=first->next;
    if (!--elements)
      last= &first;
    return tmp->info;
  }
  inline void disjoin(base_list *list)
  {
    list_node **prev= &first;
    list_node *node= first;
    list_node *list_first= list->first;
    elements=0;
    while (node && node != list_first)
    {
      prev= &node->next;
      node= node->next;
      elements++;
    }
    *prev= *last;
    last= prev;
  }
  inline void prepand(base_list *list)
  {
    if (!list->is_empty())
    {
      *list->last= first;
      first= list->first;
      elements+= list->elements;
    }
  }
  inline void swap(base_list &rhs)
  {
    { list_node * dummy; dummy= first; first= rhs.first; rhs.first= dummy; };
    { list_node ** dummy; dummy= last; last= rhs.last; rhs.last= dummy; };
    { uint dummy; dummy= elements; elements= rhs.elements; rhs.elements= dummy; };
  }
  inline list_node* last_node() { return *last; }
  inline list_node* first_node() { return first;}
  inline void *head() { return first->info; }
  inline void **head_ref() { return first != &end_of_list ? &first->info : 0; }
  inline In_C_you_should_use_my_bool_instead() is_empty() { return first == &end_of_list ; }
  inline list_node *last_ref() { return &end_of_list; }
  friend class base_list_iterator;
  friend class error_list;
  friend class error_list_iterator;
protected:
  void after(void *info,list_node *node)
  {
    list_node *new_node=new list_node(info,node->next);
    node->next=new_node;
    elements++;
    if (last == &(node->next))
      last= &new_node->next;
  }
};
class base_list_iterator
{
protected:
  base_list *list;
  list_node **el,**prev,*current;
  void sublist(base_list &ls, uint elm)
  {
    ls.first= *el;
    ls.last= list->last;
    ls.elements= elm;
  }
public:
  base_list_iterator()
    :list(0), el(0), prev(0), current(0)
  {}
  base_list_iterator(base_list &list_par)
  { init(list_par); }
  inline void init(base_list &list_par)
  {
    list= &list_par;
    el= &list_par.first;
    prev= 0;
    current= 0;
  }
  inline void *next(void)
  {
    prev=el;
    current= *el;
    el= &current->next;
    return current->info;
  }
  inline void *next_fast(void)
  {
    list_node *tmp;
    tmp= *el;
    el= &tmp->next;
    return tmp->info;
  }
  inline void rewind(void)
  {
    el= &list->first;
  }
  inline void *replace(void *element)
  {
    void *tmp=current->info;
    assert(current->info != 0);
    current->info=element;
    return tmp;
  }
  void *replace(base_list &new_list)
  {
    void *ret_value=current->info;
    if (!new_list.is_empty())
    {
      *new_list.last=current->next;
      current->info=new_list.first->info;
      current->next=new_list.first->next;
      if ((list->last == &current->next) && (new_list.elements > 1))
 list->last= new_list.last;
      list->elements+=new_list.elements-1;
    }
    return ret_value;
  }
  inline void remove(void)
  {
    list->remove(prev);
    el=prev;
    current=0;
  }
  void after(void *element)
  {
    list->after(element,current);
    current=current->next;
    el= &current->next;
  }
  inline void **ref(void)
  {
    return &current->info;
  }
  inline In_C_you_should_use_my_bool_instead() is_last(void)
  {
    return el == &list->last_ref()->next;
  }
  friend class error_list_iterator;
};
template <class T> class List :public base_list
{
public:
  inline List() :base_list() {}
  inline List(const List<T> &tmp) :base_list(tmp) {}
  inline List(const List<T> &tmp, MEM_ROOT *mem_root) :
    base_list(tmp, mem_root) {}
  inline In_C_you_should_use_my_bool_instead() push_back(T *a) { return base_list::push_back(a); }
  inline In_C_you_should_use_my_bool_instead() push_back(T *a, MEM_ROOT *mem_root)
  { return base_list::push_back(a, mem_root); }
  inline In_C_you_should_use_my_bool_instead() push_front(T *a) { return base_list::push_front(a); }
  inline T* head() {return (T*) base_list::head(); }
  inline T** head_ref() {return (T**) base_list::head_ref(); }
  inline T* pop() {return (T*) base_list::pop(); }
  inline void concat(List<T> *list) { base_list::concat(list); }
  inline void disjoin(List<T> *list) { base_list::disjoin(list); }
  inline void prepand(List<T> *list) { base_list::prepand(list); }
  void delete_elements(void)
  {
    list_node *element,*next;
    for (element=first; element != &end_of_list; element=next)
    {
      next=element->next;
      delete (T*) element->info;
    }
    empty();
  }
};
template <class T> class List_iterator :public base_list_iterator
{
public:
  List_iterator(List<T> &a) : base_list_iterator(a) {}
  List_iterator() : base_list_iterator() {}
  inline void init(List<T> &a) { base_list_iterator::init(a); }
  inline T* operator++(int) { return (T*) base_list_iterator::next(); }
  inline T *replace(T *a) { return (T*) base_list_iterator::replace(a); }
  inline T *replace(List<T> &a) { return (T*) base_list_iterator::replace(a); }
  inline void rewind(void) { base_list_iterator::rewind(); }
  inline void remove() { base_list_iterator::remove(); }
  inline void after(T *a) { base_list_iterator::after(a); }
  inline T** ref(void) { return (T**) base_list_iterator::ref(); }
};
template <class T> class List_iterator_fast :public base_list_iterator
{
protected:
  inline T *replace(T *a) { return (T*) 0; }
  inline T *replace(List<T> &a) { return (T*) 0; }
  inline void remove(void) { }
  inline void after(T *a) { }
  inline T** ref(void) { return (T**) 0; }
public:
  inline List_iterator_fast(List<T> &a) : base_list_iterator(a) {}
  inline List_iterator_fast() : base_list_iterator() {}
  inline void init(List<T> &a) { base_list_iterator::init(a); }
  inline T* operator++(int) { return (T*) base_list_iterator::next_fast(); }
  inline void rewind(void) { base_list_iterator::rewind(); }
  void sublist(List<T> &list_arg, uint el_arg)
  {
    base_list_iterator::sublist(list_arg, el_arg);
  }
};
struct ilink
{
  struct ilink **prev,*next;
  static void *operator new(size_t size)
  {
    return (void*)my_malloc((uint)size, (myf) (16 | 8));
  }
  static void operator delete(void* ptr_arg, size_t size)
  {
     ((void)(myf) (16|64),my_no_flags_free((uchar*)ptr_arg));
  }
  inline ilink()
  {
    prev=0; next=0;
  }
  inline void unlink()
  {
    if (prev) *prev= next;
    if (next) next->prev=prev;
    prev=0 ; next=0;
  }
  virtual ~ilink() { unlink(); }
};
class i_string: public ilink
{
public:
  const char* ptr;
  i_string():ptr(0) { }
  i_string(const char* s) : ptr(s) {}
};
class i_string_pair: public ilink
{
public:
  const char* key;
  const char* val;
  i_string_pair():key(0),val(0) { }
  i_string_pair(const char* key_arg, const char* val_arg) :
    key(key_arg),val(val_arg) {}
};
template <class T> class I_List_iterator;
class base_ilist
{
public:
  struct ilink *first,last;
  inline void empty() { first= &last; last.prev= &first; }
  base_ilist() { empty(); }
  inline In_C_you_should_use_my_bool_instead() is_empty() { return first == &last; }
  inline void append(ilink *a)
  {
    first->prev= &a->next;
    a->next=first; a->prev= &first; first=a;
  }
  inline void push_back(ilink *a)
  {
    *last.prev= a;
    a->next= &last;
    a->prev= last.prev;
    last.prev= &a->next;
  }
  inline struct ilink *get()
  {
    struct ilink *first_link=first;
    if (first_link == &last)
      return 0;
    first_link->unlink();
    return first_link;
  }
  inline struct ilink *head()
  {
    return (first != &last) ? first : 0;
  }
  friend class base_list_iterator;
};
class base_ilist_iterator
{
  base_ilist *list;
  struct ilink **el,*current;
public:
  base_ilist_iterator(base_ilist &list_par) :list(&list_par),
    el(&list_par.first),current(0) {}
  void *next(void)
  {
    current= *el;
    if (current == &list->last) return 0;
    el= &current->next;
    return current;
  }
};
template <class T>
class I_List :private base_ilist
{
public:
  I_List() :base_ilist() {}
  inline void empty() { base_ilist::empty(); }
  inline In_C_you_should_use_my_bool_instead() is_empty() { return base_ilist::is_empty(); }
  inline void append(T* a) { base_ilist::append(a); }
  inline void push_back(T* a) { base_ilist::push_back(a); }
  inline T* get() { return (T*) base_ilist::get(); }
  inline T* head() { return (T*) base_ilist::head(); }
  friend class I_List_iterator<T>;
};
template <class T> class I_List_iterator :public base_ilist_iterator
{
public:
  I_List_iterator(I_List<T> &a) : base_ilist_iterator(a) {}
  inline T* operator++(int) { return (T*) base_ilist_iterator::next(); }
};
template <typename T>
inline
void
list_copy_and_replace_each_value(List<T> &list, MEM_ROOT *mem_root)
{
  List_iterator<T> it(list);
  T *el;
  while ((el= it++))
    it.replace(el->clone(mem_root));
}
#include "sql_map.h"
class mapped_files;
mapped_files *map_file(const char * name,uchar *magic,uint magic_length);
void unmap_file(mapped_files *map);
class mapped_files :public ilink {
  uchar *map;
  ha_rows size;
  char *name;
  File file;
  int error;
  uint use_count;
public:
  mapped_files(const char * name,uchar *magic,uint magic_length);
  ~mapped_files();
  friend class mapped_file;
  friend mapped_files *map_file(const char * name,uchar *magic,
    uint magic_length);
  friend void unmap_file(mapped_files *map);
};
class mapped_file
{
  mapped_files *file;
public:
  mapped_file(const char * name,uchar *magic,uint magic_length)
  {
    file=map_file(name,magic,magic_length);
  }
  ~mapped_file()
  {
    unmap_file(file);
  }
  uchar *map()
  {
    return file->map;
  }
};
#include "my_decimal.h"
#include <decimal.h>
typedef enum
{TRUNCATE=0, HALF_EVEN, HALF_UP, CEILING, FLOOR}
  decimal_round_mode;
typedef int32 decimal_digit_t;
typedef struct st_decimal_t {
  int intg, frac, len;
  my_bool sign;
  decimal_digit_t *buf;
} decimal_t;
int internal_str2dec(const char *from, decimal_t *to, char **end,
                     my_bool fixed);
int decimal2string(decimal_t *from, char *to, int *to_len,
                   int fixed_precision, int fixed_decimals,
                   char filler);
int decimal2ulonglong(decimal_t *from, ulonglong *to);
int ulonglong2decimal(ulonglong from, decimal_t *to);
int decimal2longlong(decimal_t *from, longlong *to);
int longlong2decimal(longlong from, decimal_t *to);
int decimal2double(decimal_t *from, double *to);
int double2decimal(double from, decimal_t *to);
int decimal_actual_fraction(decimal_t *from);
int decimal2bin(decimal_t *from, uchar *to, int precision, int scale);
int bin2decimal(const uchar *from, decimal_t *to, int precision, int scale);
int decimal_size(int precision, int scale);
int decimal_bin_size(int precision, int scale);
int decimal_result_size(decimal_t *from1, decimal_t *from2, char op,
                        int param);
int decimal_intg(decimal_t *from);
int decimal_add(decimal_t *from1, decimal_t *from2, decimal_t *to);
int decimal_sub(decimal_t *from1, decimal_t *from2, decimal_t *to);
int decimal_cmp(decimal_t *from1, decimal_t *from2);
int decimal_mul(decimal_t *from1, decimal_t *from2, decimal_t *to);
int decimal_div(decimal_t *from1, decimal_t *from2, decimal_t *to,
                int scale_incr);
int decimal_mod(decimal_t *from1, decimal_t *from2, decimal_t *to);
int decimal_round(decimal_t *from, decimal_t *to, int new_scale,
                  decimal_round_mode mode);
int decimal_is_zero(decimal_t *from);
void max_decimal(int precision, int frac, decimal_t *to);
inline uint my_decimal_size(uint precision, uint scale)
{
  return decimal_size(precision, scale) + 1;
}
inline int my_decimal_int_part(uint precision, uint decimals)
{
  return precision - ((decimals == 31) ? 0 : decimals);
}
class my_decimal :public decimal_t
{
  decimal_digit_t buffer[9];
public:
  void init()
  {
    len= 9;
    buf= buffer;
    for (uint i= 0; i < 9; i++)
      buffer[i]= i;
  }
  my_decimal()
  {
    init();
  }
  void fix_buffer_pointer() { buf= buffer; }
  In_C_you_should_use_my_bool_instead() sign() const { return decimal_t::sign; }
  void sign(In_C_you_should_use_my_bool_instead() s) { decimal_t::sign= s; }
  uint precision() const { return intg + frac; }
  void swap(my_decimal &rhs)
  {
    { my_decimal dummy; dummy= *this; *this= rhs; rhs= dummy; };
    { decimal_digit_t * dummy; dummy= buf; buf= rhs.buf; rhs.buf= dummy; };
  }
};
void print_decimal(const my_decimal *dec);
void print_decimal_buff(const my_decimal *dec, const uchar* ptr, int length);
const char *dbug_decimal_as_string(char *buff, const my_decimal *val);
int decimal_operation_results(int result);
inline
void max_my_decimal(my_decimal *to, int precision, int frac)
{
  assert((precision <= ((9 * 9) - 8*2))&& (frac <= 30));
  max_decimal(precision, frac, (decimal_t*) to);
}
inline void max_internal_decimal(my_decimal *to)
{
  max_my_decimal(to, ((9 * 9) - 8*2), 0);
}
inline int check_result(uint mask, int result)
{
  if (result & mask)
    decimal_operation_results(result);
  return result;
}
inline int check_result_and_overflow(uint mask, int result, my_decimal *val)
{
  if (check_result(mask, result) & 2)
  {
    In_C_you_should_use_my_bool_instead() sign= val->sign();
    val->fix_buffer_pointer();
    max_internal_decimal(val);
    val->sign(sign);
  }
  return result;
}
inline uint my_decimal_length_to_precision(uint length, uint scale,
                                           In_C_you_should_use_my_bool_instead() unsigned_flag)
{
  assert(length || !scale);
  return (uint) (length - (scale>0 ? 1:0) -
                 (unsigned_flag || !length ? 0:1));
}
inline uint32 my_decimal_precision_to_length(uint precision, uint8 scale,
                                             In_C_you_should_use_my_bool_instead() unsigned_flag)
{
  assert(precision || !scale);
  do { if ((precision) > (((9 * 9) - 8*2))) (precision)=(((9 * 9) - 8*2)); } while(0);
  return (uint32)(precision + (scale>0 ? 1:0) +
                  (unsigned_flag || !precision ? 0:1));
}
inline
int my_decimal_string_length(const my_decimal *d)
{
  return (((d)->intg ? (d)->intg : 1) + (d)->frac + ((d)->frac > 0) + 2);
}
inline
int my_decimal_max_length(const my_decimal *d)
{
  return (((d)->intg ? (d)->intg : 1) + (d)->frac + ((d)->frac > 0) + 2) - 1;
}
inline
int my_decimal_get_binary_size(uint precision, uint scale)
{
  return decimal_bin_size((int)precision, (int)scale);
}
inline
void my_decimal2decimal(const my_decimal *from, my_decimal *to)
{
  *to= *from;
  to->fix_buffer_pointer();
}
int my_decimal2binary(uint mask, const my_decimal *d, uchar *bin, int prec,
        int scale);
inline
int binary2my_decimal(uint mask, const uchar *bin, my_decimal *d, int prec,
        int scale)
{
  return check_result(mask, bin2decimal(bin, (decimal_t*) d, prec, scale));
}
inline
int my_decimal_set_zero(my_decimal *d)
{
  do { (((decimal_t*) d))->buf[0]=0; (((decimal_t*) d))->intg=1; (((decimal_t*) d))->frac=0; (((decimal_t*) d))->sign=0; } while(0);
  return 0;
}
inline
In_C_you_should_use_my_bool_instead() my_decimal_is_zero(const my_decimal *decimal_value)
{
  return decimal_is_zero((decimal_t*) decimal_value);
}
inline
int my_decimal_round(uint mask, const my_decimal *from, int scale,
                     In_C_you_should_use_my_bool_instead() truncate, my_decimal *to)
{
  return check_result(mask, decimal_round((decimal_t*) from, to, scale,
       (truncate ? TRUNCATE : HALF_UP)));
}
inline
int my_decimal_floor(uint mask, const my_decimal *from, my_decimal *to)
{
  return check_result(mask, decimal_round((decimal_t*) from, to, 0, FLOOR));
}
inline
int my_decimal_ceiling(uint mask, const my_decimal *from, my_decimal *to)
{
  return check_result(mask, decimal_round((decimal_t*) from, to, 0, CEILING));
}
int my_decimal2string(uint mask, const my_decimal *d, uint fixed_prec,
        uint fixed_dec, char filler, String *str);
inline
int my_decimal2int(uint mask, const my_decimal *d, my_bool unsigned_flag,
     longlong *l)
{
  my_decimal rounded;
  decimal_round((decimal_t*)d, &rounded, 0, HALF_UP);
  return check_result(mask, (unsigned_flag ?
        decimal2ulonglong(&rounded, (ulonglong *)l) :
        decimal2longlong(&rounded, l)));
}
inline
int my_decimal2double(uint mask, const my_decimal *d, double *result)
{
  return decimal2double((decimal_t*) d, result);
}
inline
int str2my_decimal(uint mask, const char *str, my_decimal *d, char **end)
{
  return check_result_and_overflow(mask, internal_str2dec((str), ((decimal_t*)d), (end), 0),
                                   d);
}
int str2my_decimal(uint mask, const char *from, uint length,
                   CHARSET_INFO *charset, my_decimal *decimal_value);
inline
int double2my_decimal(uint mask, double val, my_decimal *d)
{
  return check_result_and_overflow(mask, double2decimal(val, (decimal_t*)d), d);
}
inline
int int2my_decimal(uint mask, longlong i, my_bool unsigned_flag, my_decimal *d)
{
  return check_result(mask, (unsigned_flag ?
        ulonglong2decimal((ulonglong)i, d) :
        longlong2decimal(i, d)));
}
inline
void my_decimal_neg(decimal_t *arg)
{
  if (decimal_is_zero(arg))
  {
    arg->sign= 0;
    return;
  }
  do { (arg)->sign^=1; } while(0);
}
inline
int my_decimal_add(uint mask, my_decimal *res, const my_decimal *a,
     const my_decimal *b)
{
  return check_result_and_overflow(mask,
                                   decimal_add((decimal_t*)a,(decimal_t*)b,res),
                                   res);
}
inline
int my_decimal_sub(uint mask, my_decimal *res, const my_decimal *a,
     const my_decimal *b)
{
  return check_result_and_overflow(mask,
                                   decimal_sub((decimal_t*)a,(decimal_t*)b,res),
                                   res);
}
inline
int my_decimal_mul(uint mask, my_decimal *res, const my_decimal *a,
     const my_decimal *b)
{
  return check_result_and_overflow(mask,
                                   decimal_mul((decimal_t*)a,(decimal_t*)b,res),
                                   res);
}
inline
int my_decimal_div(uint mask, my_decimal *res, const my_decimal *a,
     const my_decimal *b, int div_scale_inc)
{
  return check_result_and_overflow(mask,
                                   decimal_div((decimal_t*)a,(decimal_t*)b,res,
                                               div_scale_inc),
                                   res);
}
inline
int my_decimal_mod(uint mask, my_decimal *res, const my_decimal *a,
     const my_decimal *b)
{
  return check_result_and_overflow(mask,
                                   decimal_mod((decimal_t*)a,(decimal_t*)b,res),
                                   res);
}
inline
int my_decimal_cmp(const my_decimal *a, const my_decimal *b)
{
  return decimal_cmp((decimal_t*) a, (decimal_t*) b);
}
inline
int my_decimal_intg(const my_decimal *a)
{
  return decimal_intg((decimal_t*) a);
}
void my_decimal_trim(ulong *precision, uint *scale);
#include "handler.h"
#include <my_handler.h>
#include "myisampack.h"
typedef struct st_HA_KEYSEG
{
  CHARSET_INFO *charset;
  uint32 start;
  uint32 null_pos;
  uint16 bit_pos;
  uint16 flag;
  uint16 length;
  uint8 type;
  uint8 language;
  uint8 null_bit;
  uint8 bit_start,bit_end;
  uint8 bit_length;
} HA_KEYSEG;
extern int ha_compare_text(CHARSET_INFO *, uchar *, uint, uchar *, uint ,
      my_bool, my_bool);
extern int ha_key_cmp(register HA_KEYSEG *keyseg, register uchar *a,
        register uchar *b, uint key_length, uint nextflag,
        uint *diff_pos);
extern HA_KEYSEG *ha_find_null(HA_KEYSEG *keyseg, uchar *a);
extern void my_handler_error_register(void);
extern void my_handler_error_unregister(void);
#include <ft_global.h>
typedef struct st_ft_info FT_INFO;
struct _ft_vft
{
  int (*read_next)(FT_INFO *, char *);
  float (*find_relevance)(FT_INFO *, uchar *, uint);
  void (*close_search)(FT_INFO *);
  float (*get_relevance)(FT_INFO *);
  void (*reinit_search)(FT_INFO *);
};
struct st_ft_info
{
  struct _ft_vft *please;
};
extern const char *ft_stopword_file;
extern const char *ft_precompiled_stopwords[];
extern ulong ft_min_word_len;
extern ulong ft_max_word_len;
extern ulong ft_query_expansion_limit;
extern char ft_boolean_syntax[15];
extern struct st_mysql_ftparser ft_default_parser;
int ft_init_stopwords(void);
void ft_free_stopwords(void);
FT_INFO *ft_init_search(uint,void *, uint, uchar *, uint,CHARSET_INFO *, uchar *);
my_bool ft_boolean_check_syntax_string(const uchar *);
#include <keycache.h>
struct st_block_link;
typedef struct st_block_link BLOCK_LINK;
struct st_keycache_page;
typedef struct st_keycache_page KEYCACHE_PAGE;
struct st_hash_link;
typedef struct st_hash_link HASH_LINK;
typedef struct st_keycache_wqueue
{
  struct st_my_thread_var *last_thread;
} KEYCACHE_WQUEUE;
typedef struct st_key_cache
{
  my_bool key_cache_inited;
  my_bool in_resize;
  my_bool resize_in_flush;
  my_bool can_be_used;
  size_t key_cache_mem_size;
  uint key_cache_block_size;
  ulong min_warm_blocks;
  ulong age_threshold;
  ulonglong keycache_time;
  uint hash_entries;
  int hash_links;
  int hash_links_used;
  int disk_blocks;
  ulong blocks_used;
  ulong blocks_unused;
  ulong blocks_changed;
  ulong warm_blocks;
  ulong cnt_for_resize_op;
  long blocks_available;
  HASH_LINK **hash_root;
  HASH_LINK *hash_link_root;
  HASH_LINK *free_hash_list;
  BLOCK_LINK *free_block_list;
  BLOCK_LINK *block_root;
  uchar *block_mem;
  BLOCK_LINK *used_last;
  BLOCK_LINK *used_ins;
  pthread_mutex_t cache_lock;
  KEYCACHE_WQUEUE resize_queue;
  KEYCACHE_WQUEUE waiting_for_resize_cnt;
  KEYCACHE_WQUEUE waiting_for_hash_link;
  KEYCACHE_WQUEUE waiting_for_block;
  BLOCK_LINK *changed_blocks[128];
  BLOCK_LINK *file_blocks[128];
  ulonglong param_buff_size;
  ulong param_block_size;
  ulong param_division_limit;
  ulong param_age_threshold;
  ulong global_blocks_changed;
  ulonglong global_cache_w_requests;
  ulonglong global_cache_write;
  ulonglong global_cache_r_requests;
  ulonglong global_cache_read;
  int blocks;
  my_bool in_init;
} KEY_CACHE;
extern KEY_CACHE dflt_key_cache_var, *dflt_key_cache;
extern int init_key_cache(KEY_CACHE *keycache, uint key_cache_block_size,
     size_t use_mem, uint division_limit,
     uint age_threshold);
extern int resize_key_cache(KEY_CACHE *keycache, uint key_cache_block_size,
       size_t use_mem, uint division_limit,
       uint age_threshold);
extern void change_key_cache_param(KEY_CACHE *keycache, uint division_limit,
       uint age_threshold);
extern uchar *key_cache_read(KEY_CACHE *keycache,
                            File file, my_off_t filepos, int level,
                            uchar *buff, uint length,
       uint block_length,int return_buffer);
extern int key_cache_insert(KEY_CACHE *keycache,
                            File file, my_off_t filepos, int level,
                            uchar *buff, uint length);
extern int key_cache_write(KEY_CACHE *keycache,
                           File file, my_off_t filepos, int level,
                           uchar *buff, uint length,
      uint block_length,int force_write);
extern int flush_key_blocks(KEY_CACHE *keycache,
                            int file, enum flush_type type);
extern void end_key_cache(KEY_CACHE *keycache, my_bool cleanup);
extern my_bool multi_keycache_init(void);
extern void multi_keycache_free(void);
extern KEY_CACHE *multi_key_cache_search(uchar *key, uint length);
extern my_bool multi_key_cache_set(const uchar *key, uint length,
       KEY_CACHE *key_cache);
extern void multi_key_cache_change(KEY_CACHE *old_data,
       KEY_CACHE *new_data);
extern int reset_key_cache_counters(const char *name,
                                    KEY_CACHE *key_cache);
enum legacy_db_type
{
  DB_TYPE_UNKNOWN=0,DB_TYPE_DIAB_ISAM=1,
  DB_TYPE_HASH,DB_TYPE_MISAM,DB_TYPE_PISAM,
  DB_TYPE_RMS_ISAM, DB_TYPE_HEAP, DB_TYPE_ISAM,
  DB_TYPE_MRG_ISAM, DB_TYPE_MYISAM, DB_TYPE_MRG_MYISAM,
  DB_TYPE_BERKELEY_DB, DB_TYPE_INNODB,
  DB_TYPE_GEMINI, DB_TYPE_NDBCLUSTER,
  DB_TYPE_EXAMPLE_DB, DB_TYPE_ARCHIVE_DB, DB_TYPE_CSV_DB,
  DB_TYPE_FEDERATED_DB,
  DB_TYPE_BLACKHOLE_DB,
  DB_TYPE_PARTITION_DB,
  DB_TYPE_BINLOG,
  DB_TYPE_SOLID,
  DB_TYPE_PBXT,
  DB_TYPE_TABLE_FUNCTION,
  DB_TYPE_MEMCACHE,
  DB_TYPE_FALCON,
  DB_TYPE_MARIA,
  DB_TYPE_FIRST_DYNAMIC=42,
  DB_TYPE_DEFAULT=127
};
enum row_type { ROW_TYPE_NOT_USED=-1, ROW_TYPE_DEFAULT, ROW_TYPE_FIXED,
  ROW_TYPE_DYNAMIC, ROW_TYPE_COMPRESSED,
  ROW_TYPE_REDUNDANT, ROW_TYPE_COMPACT, ROW_TYPE_PAGE };
enum enum_binlog_func {
  BFN_RESET_LOGS= 1,
  BFN_RESET_SLAVE= 2,
  BFN_BINLOG_WAIT= 3,
  BFN_BINLOG_END= 4,
  BFN_BINLOG_PURGE_FILE= 5
};
enum enum_binlog_command {
  LOGCOM_CREATE_TABLE,
  LOGCOM_ALTER_TABLE,
  LOGCOM_RENAME_TABLE,
  LOGCOM_DROP_TABLE,
  LOGCOM_CREATE_DB,
  LOGCOM_ALTER_DB,
  LOGCOM_DROP_DB
};
typedef ulonglong my_xid;
struct xid_t {
  long formatID;
  long gtrid_length;
  long bqual_length;
  char data[128];
  xid_t() {}
  In_C_you_should_use_my_bool_instead() eq(struct xid_t *xid)
  { return eq(xid->gtrid_length, xid->bqual_length, xid->data); }
  In_C_you_should_use_my_bool_instead() eq(long g, long b, const char *d)
  { return g == gtrid_length && b == bqual_length && !memcmp(d, data, g+b); }
  void set(struct xid_t *xid)
  { memcpy(this, xid, xid->length()); }
  void set(long f, const char *g, long gl, const char *b, long bl)
  {
    formatID= f;
    memcpy(data, g, gtrid_length= gl);
    memcpy(data+gl, b, bqual_length= bl);
  }
  void set(ulonglong xid)
  {
    my_xid tmp;
    formatID= 1;
    set(8, 0, "MySQLXid");
    memcpy(data+8, &server_id, sizeof(server_id));
    tmp= xid;
    memcpy(data+(8 +sizeof(server_id)), &tmp, sizeof(tmp));
    gtrid_length=((8 +sizeof(server_id))+sizeof(my_xid));
  }
  void set(long g, long b, const char *d)
  {
    formatID= 1;
    gtrid_length= g;
    bqual_length= b;
    memcpy(data, d, g+b);
  }
  In_C_you_should_use_my_bool_instead() is_null() { return formatID == -1; }
  void null() { formatID= -1; }
  my_xid quick_get_my_xid()
  {
    my_xid tmp;
    memcpy(&tmp, data+(8 +sizeof(server_id)), sizeof(tmp));
    return tmp;
  }
  my_xid get_my_xid()
  {
    return gtrid_length == ((8 +sizeof(server_id))+sizeof(my_xid)) && bqual_length == 0 &&
           !memcmp(data+8, &server_id, sizeof(server_id)) &&
           !memcmp(data, "MySQLXid", 8) ?
           quick_get_my_xid() : 0;
  }
  uint length()
  {
    return sizeof(formatID)+sizeof(gtrid_length)+sizeof(bqual_length)+
           gtrid_length+bqual_length;
  }
  uchar *key()
  {
    return (uchar *)&gtrid_length;
  }
  uint key_length()
  {
    return sizeof(gtrid_length)+sizeof(bqual_length)+gtrid_length+bqual_length;
  }
};
typedef struct xid_t XID;
enum ts_command_type
{
  TS_CMD_NOT_DEFINED = -1,
  CREATE_TABLESPACE = 0,
  ALTER_TABLESPACE = 1,
  CREATE_LOGFILE_GROUP = 2,
  ALTER_LOGFILE_GROUP = 3,
  DROP_TABLESPACE = 4,
  DROP_LOGFILE_GROUP = 5,
  CHANGE_FILE_TABLESPACE = 6,
  ALTER_ACCESS_MODE_TABLESPACE = 7
};
enum ts_alter_tablespace_type
{
  TS_ALTER_TABLESPACE_TYPE_NOT_DEFINED = -1,
  ALTER_TABLESPACE_ADD_FILE = 1,
  ALTER_TABLESPACE_DROP_FILE = 2
};
enum tablespace_access_mode
{
  TS_NOT_DEFINED= -1,
  TS_READ_ONLY = 0,
  TS_READ_WRITE = 1,
  TS_NOT_ACCESSIBLE = 2
};
struct handlerton;
class st_alter_tablespace : public Sql_alloc
{
  public:
  const char *tablespace_name;
  const char *logfile_group_name;
  enum ts_command_type ts_cmd_type;
  enum ts_alter_tablespace_type ts_alter_tablespace_type;
  const char *data_file_name;
  const char *undo_file_name;
  const char *redo_file_name;
  ulonglong extent_size;
  ulonglong undo_buffer_size;
  ulonglong redo_buffer_size;
  ulonglong initial_size;
  ulonglong autoextend_size;
  ulonglong max_size;
  uint nodegroup_id;
  handlerton *storage_engine;
  In_C_you_should_use_my_bool_instead() wait_until_completed;
  const char *ts_comment;
  enum tablespace_access_mode ts_access_mode;
  st_alter_tablespace()
  {
    tablespace_name= NULL;
    logfile_group_name= "DEFAULT_LG";
    ts_cmd_type= TS_CMD_NOT_DEFINED;
    data_file_name= NULL;
    undo_file_name= NULL;
    redo_file_name= NULL;
    extent_size= 1024*1024;
    undo_buffer_size= 8*1024*1024;
    redo_buffer_size= 8*1024*1024;
    initial_size= 128*1024*1024;
    autoextend_size= 0;
    max_size= 0;
    storage_engine= NULL;
    nodegroup_id= 65535;
    wait_until_completed= (1);
    ts_comment= NULL;
    ts_access_mode= TS_NOT_DEFINED;
  }
};
struct st_table;
typedef struct st_table TABLE;
typedef struct st_table_share TABLE_SHARE;
struct st_foreign_key_info;
typedef struct st_foreign_key_info FOREIGN_KEY_INFO;
typedef In_C_you_should_use_my_bool_instead() (stat_print_fn)(THD *thd, const char *type, uint type_len,
                             const char *file, uint file_len,
                             const char *status, uint status_len);
enum ha_stat_type { HA_ENGINE_STATUS, HA_ENGINE_LOGS, HA_ENGINE_MUTEX };
extern st_plugin_int *hton2plugin[15];
enum log_status
{
  HA_LOG_STATUS_FREE= 0,
  HA_LOG_STATUS_INUSE= 1,
  HA_LOG_STATUS_NOSUCHLOG= 2
};
void signal_log_not_needed(struct handlerton, char *log_file);
struct handler_log_file_data {
  LEX_STRING filename;
  enum log_status status;
};
enum handler_iterator_type
{
  HA_TRANSACTLOG_ITERATOR= 1
};
enum handler_create_iterator_result
{
  HA_ITERATOR_OK,
  HA_ITERATOR_UNSUPPORTED,
  HA_ITERATOR_ERROR
};
struct handler_iterator {
  int (*next)(struct handler_iterator *, void *iterator_object);
  void (*destroy)(struct handler_iterator *);
  void *buffer;
};
struct handlerton
{
  SHOW_COMP_OPTION state;
  enum legacy_db_type db_type;
   uint slot;
   uint savepoint_offset;
   int (*close_connection)(handlerton *hton, THD *thd);
   int (*savepoint_set)(handlerton *hton, THD *thd, void *sv);
   int (*savepoint_rollback)(handlerton *hton, THD *thd, void *sv);
   int (*savepoint_release)(handlerton *hton, THD *thd, void *sv);
   int (*commit)(handlerton *hton, THD *thd, In_C_you_should_use_my_bool_instead() all);
   int (*rollback)(handlerton *hton, THD *thd, In_C_you_should_use_my_bool_instead() all);
   int (*prepare)(handlerton *hton, THD *thd, In_C_you_should_use_my_bool_instead() all);
   int (*recover)(handlerton *hton, XID *xid_list, uint len);
   int (*commit_by_xid)(handlerton *hton, XID *xid);
   int (*rollback_by_xid)(handlerton *hton, XID *xid);
   void *(*create_cursor_read_view)(handlerton *hton, THD *thd);
   void (*set_cursor_read_view)(handlerton *hton, THD *thd, void *read_view);
   void (*close_cursor_read_view)(handlerton *hton, THD *thd, void *read_view);
   handler *(*create)(handlerton *hton, TABLE_SHARE *table, MEM_ROOT *mem_root);
   void (*drop_database)(handlerton *hton, char* path);
   int (*panic)(handlerton *hton, enum ha_panic_function flag);
   int (*start_consistent_snapshot)(handlerton *hton, THD *thd);
   In_C_you_should_use_my_bool_instead() (*flush_logs)(handlerton *hton);
   In_C_you_should_use_my_bool_instead() (*show_status)(handlerton *hton, THD *thd, stat_print_fn *print, enum ha_stat_type stat);
   uint (*partition_flags)();
   uint (*alter_table_flags)(uint flags);
   int (*alter_tablespace)(handlerton *hton, THD *thd, st_alter_tablespace *ts_info);
   int (*fill_files_table)(handlerton *hton, THD *thd,
                           TABLE_LIST *tables,
                           class Item *cond);
   uint32 flags;
   int (*binlog_func)(handlerton *hton, THD *thd, enum_binlog_func fn, void *arg);
   void (*binlog_log_query)(handlerton *hton, THD *thd,
                            enum_binlog_command binlog_command,
                            const char *query, uint query_length,
                            const char *db, const char *table_name);
   int (*release_temporary_latches)(handlerton *hton, THD *thd);
   enum log_status (*get_log_status)(handlerton *hton, char *log);
   enum handler_create_iterator_result
     (*create_iterator)(handlerton *hton, enum handler_iterator_type type,
                        struct handler_iterator *fill_this_in);
   int (*discover)(handlerton *hton, THD* thd, const char *db,
                   const char *name,
                   uchar **frmblob,
                   size_t *frmlen);
   int (*find_files)(handlerton *hton, THD *thd,
                     const char *db,
                     const char *path,
                     const char *wild, In_C_you_should_use_my_bool_instead() dir, List<LEX_STRING> *files);
   int (*table_exists_in_engine)(handlerton *hton, THD* thd, const char *db,
                                 const char *name);
   uint32 license;
   void *data;
};
class Ha_trx_info;
struct THD_TRANS
{
  In_C_you_should_use_my_bool_instead() no_2pc;
  Ha_trx_info *ha_list;
  In_C_you_should_use_my_bool_instead() modified_non_trans_table;
  void reset() { no_2pc= (0); modified_non_trans_table= (0); }
};
class Ha_trx_info
{
public:
  void register_ha(THD_TRANS *trans, handlerton *ht_arg)
  {
    assert(m_flags == 0);
    assert(m_ht == NULL);
    assert(m_next == NULL);
    m_ht= ht_arg;
    m_flags= (int) TRX_READ_ONLY;
    m_next= trans->ha_list;
    trans->ha_list= this;
  }
  void reset()
  {
    m_next= NULL;
    m_ht= NULL;
    m_flags= 0;
  }
  Ha_trx_info() { reset(); }
  void set_trx_read_write()
  {
    assert(is_started());
    m_flags|= (int) TRX_READ_WRITE;
  }
  In_C_you_should_use_my_bool_instead() is_trx_read_write() const
  {
    assert(is_started());
    return m_flags & (int) TRX_READ_WRITE;
  }
  In_C_you_should_use_my_bool_instead() is_started() const { return m_ht != NULL; }
  void coalesce_trx_with(const Ha_trx_info *stmt_trx)
  {
    assert(is_started());
    if (stmt_trx->is_trx_read_write())
      set_trx_read_write();
  }
  Ha_trx_info *next() const
  {
    assert(is_started());
    return m_next;
  }
  handlerton *ht() const
  {
    assert(is_started());
    return m_ht;
  }
private:
  enum { TRX_READ_ONLY= 0, TRX_READ_WRITE= 1 };
  Ha_trx_info *m_next;
  handlerton *m_ht;
  uchar m_flags;
};
enum enum_tx_isolation { ISO_READ_UNCOMMITTED, ISO_READ_COMMITTED,
    ISO_REPEATABLE_READ, ISO_SERIALIZABLE};
enum ndb_distribution { ND_KEYHASH= 0, ND_LINHASH= 1 };
typedef struct {
  ulonglong data_file_length;
  ulonglong max_data_file_length;
  ulonglong index_file_length;
  ulonglong delete_length;
  ha_rows records;
  ulong mean_rec_length;
  time_t create_time;
  time_t check_time;
  time_t update_time;
  ulonglong check_sum;
} PARTITION_INFO;
class Item;
struct st_table_log_memory_entry;
class partition_info;
struct st_partition_iter;
enum ha_choice { HA_CHOICE_UNDEF, HA_CHOICE_NO, HA_CHOICE_YES };
typedef struct st_ha_create_information
{
  CHARSET_INFO *table_charset, *default_table_charset;
  LEX_STRING connect_string;
  const char *password, *tablespace;
  LEX_STRING comment;
  const char *data_file_name, *index_file_name;
  const char *alias;
  ulonglong max_rows,min_rows;
  ulonglong auto_increment_value;
  ulong table_options;
  ulong avg_row_length;
  ulong used_fields;
  ulong key_block_size;
  SQL_LIST merge_list;
  handlerton *db_type;
  enum row_type row_type;
  uint null_bits;
  uint options;
  uint merge_insert_method;
  uint extra_size;
  enum ha_choice transactional;
  In_C_you_should_use_my_bool_instead() table_existed;
  In_C_you_should_use_my_bool_instead() frm_only;
  In_C_you_should_use_my_bool_instead() varchar;
  enum ha_storage_media storage_media;
  enum ha_choice page_checksum;
} HA_CREATE_INFO;
typedef struct st_key_create_information
{
  enum ha_key_alg algorithm;
  ulong block_size;
  LEX_STRING parser_name;
} KEY_CREATE_INFO;
class TABLEOP_HOOKS
{
public:
  TABLEOP_HOOKS() {}
  virtual ~TABLEOP_HOOKS() {}
  inline void prelock(TABLE **tables, uint count)
  {
    do_prelock(tables, count);
  }
  inline int postlock(TABLE **tables, uint count)
  {
    return do_postlock(tables, count);
  }
private:
  virtual void do_prelock(TABLE **tables, uint count)
  {
  }
  virtual int do_postlock(TABLE **tables, uint count)
  {
    return 0;
  }
};
typedef struct st_savepoint SAVEPOINT;
extern ulong savepoint_alloc_size;
extern KEY_CREATE_INFO default_key_create_info;
typedef class Item COND;
typedef struct st_ha_check_opt
{
  st_ha_check_opt() {}
  ulong sort_buffer_size;
  uint flags;
  uint sql_flags;
  KEY_CACHE *key_cache;
  void init();
} HA_CHECK_OPT;
typedef struct st_handler_buffer
{
  const uchar *buffer;
  const uchar *buffer_end;
  uchar *end_of_used_area;
} HANDLER_BUFFER;
typedef struct system_status_var SSV;
class ha_statistics
{
public:
  ulonglong data_file_length;
  ulonglong max_data_file_length;
  ulonglong index_file_length;
  ulonglong max_index_file_length;
  ulonglong delete_length;
  ulonglong auto_increment_value;
  ha_rows records;
  ha_rows deleted;
  ulong mean_rec_length;
  time_t create_time;
  time_t check_time;
  time_t update_time;
  uint block_size;
  ha_statistics():
    data_file_length(0), max_data_file_length(0),
    index_file_length(0), delete_length(0), auto_increment_value(0),
    records(0), deleted(0), mean_rec_length(0), create_time(0),
    check_time(0), update_time(0), block_size(0)
  {}
};
uint calculate_key_len(TABLE *, uint, const uchar *, key_part_map);
class handler :public Sql_alloc
{
public:
  typedef ulonglong Table_flags;
protected:
  struct st_table_share *table_share;
  struct st_table *table;
  Table_flags cached_table_flags;
  ha_rows estimation_rows_to_insert;
public:
  handlerton *ht;
  uchar *ref;
  uchar *dup_ref;
  ha_statistics stats;
  In_C_you_should_use_my_bool_instead() multi_range_sorted;
  KEY_MULTI_RANGE *multi_range_curr;
  KEY_MULTI_RANGE *multi_range_end;
  HANDLER_BUFFER *multi_range_buffer;
  key_range save_end_range, *end_range;
  KEY_PART_INFO *range_key_part;
  int key_compare_result_on_equal;
  In_C_you_should_use_my_bool_instead() eq_range;
  uint errkey;
  uint key_used_on_scan;
  uint active_index;
  uint ref_length;
  FT_INFO *ft_handler;
  enum {NONE=0, INDEX, RND} inited;
  In_C_you_should_use_my_bool_instead() locked;
  In_C_you_should_use_my_bool_instead() implicit_emptied;
  const COND *pushed_cond;
  ulonglong next_insert_id;
  ulonglong insert_id_for_cur_row;
  Discrete_interval auto_inc_interval_for_cur_row;
  handler(handlerton *ht_arg, TABLE_SHARE *share_arg)
    :table_share(share_arg), table(0),
    estimation_rows_to_insert(0), ht(ht_arg),
    ref(0), key_used_on_scan(64), active_index(64),
    ref_length(sizeof(my_off_t)),
    ft_handler(0), inited(NONE),
    locked((0)), implicit_emptied(0),
    pushed_cond(0), next_insert_id(0), insert_id_for_cur_row(0)
    {}
  virtual ~handler(void)
  {
    assert(locked == (0));
  }
  virtual handler *clone(MEM_ROOT *mem_root);
  void init()
  {
    cached_table_flags= table_flags();
  }
  int ha_open(TABLE *table, const char *name, int mode, int test_if_locked);
  int ha_index_init(uint idx, In_C_you_should_use_my_bool_instead() sorted)
  {
    int result;
    const char *_db_func_, *_db_file_; uint _db_level_; char **_db_framep_; _db_enter_ ("ha_index_init","./sql/handler.h",1159,&_db_func_,&_db_file_,&_db_level_, &_db_framep_);
    assert(inited==NONE);
    if (!(result= index_init(idx, sorted)))
      inited=INDEX;
    do {_db_return_ (1163, &_db_func_, &_db_file_, &_db_level_); return(result);} while(0);
  }
  int ha_index_end()
  {
    const char *_db_func_, *_db_file_; uint _db_level_; char **_db_framep_; _db_enter_ ("ha_index_end","./sql/handler.h",1167,&_db_func_,&_db_file_,&_db_level_, &_db_framep_);
    assert(inited==INDEX);
    inited=NONE;
    do {_db_return_ (1170, &_db_func_, &_db_file_, &_db_level_); return(index_end());} while(0);
  }
  int ha_rnd_init(In_C_you_should_use_my_bool_instead() scan)
  {
    int result;
    const char *_db_func_, *_db_file_; uint _db_level_; char **_db_framep_; _db_enter_ ("ha_rnd_init","./sql/handler.h",1175,&_db_func_,&_db_file_,&_db_level_, &_db_framep_);
    assert(inited==NONE || (inited==RND && scan));
    inited= (result= rnd_init(scan)) ? NONE: RND;
    do {_db_return_ (1178, &_db_func_, &_db_file_, &_db_level_); return(result);} while(0);
  }
  int ha_rnd_end()
  {
    const char *_db_func_, *_db_file_; uint _db_level_; char **_db_framep_; _db_enter_ ("ha_rnd_end","./sql/handler.h",1182,&_db_func_,&_db_file_,&_db_level_, &_db_framep_);
    assert(inited==RND);
    inited=NONE;
    do {_db_return_ (1185, &_db_func_, &_db_file_, &_db_level_); return(rnd_end());} while(0);
  }
  int ha_reset();
  int ha_index_or_rnd_end()
  {
    return inited == INDEX ? ha_index_end() : inited == RND ? ha_rnd_end() : 0;
  }
  Table_flags ha_table_flags() const { return cached_table_flags; }
  int ha_external_lock(THD *thd, int lock_type);
  int ha_write_row(uchar * buf);
  int ha_update_row(const uchar * old_data, uchar * new_data);
  int ha_delete_row(const uchar * buf);
  void ha_release_auto_increment();
  int ha_check_for_upgrade(HA_CHECK_OPT *check_opt);
  int ha_check(THD *thd, HA_CHECK_OPT *check_opt);
  int ha_repair(THD* thd, HA_CHECK_OPT* check_opt);
  void ha_start_bulk_insert(ha_rows rows)
  {
    estimation_rows_to_insert= rows;
    start_bulk_insert(rows);
  }
  int ha_end_bulk_insert()
  {
    estimation_rows_to_insert= 0;
    return end_bulk_insert();
  }
  int ha_bulk_update_row(const uchar *old_data, uchar *new_data,
                         uint *dup_key_found);
  int ha_delete_all_rows();
  int ha_reset_auto_increment(ulonglong value);
  int ha_backup(THD* thd, HA_CHECK_OPT* check_opt);
  int ha_restore(THD* thd, HA_CHECK_OPT* check_opt);
  int ha_optimize(THD* thd, HA_CHECK_OPT* check_opt);
  int ha_analyze(THD* thd, HA_CHECK_OPT* check_opt);
  In_C_you_should_use_my_bool_instead() ha_check_and_repair(THD *thd);
  int ha_disable_indexes(uint mode);
  int ha_enable_indexes(uint mode);
  int ha_discard_or_import_tablespace(my_bool discard);
  void ha_prepare_for_alter();
  int ha_rename_table(const char *from, const char *to);
  int ha_delete_table(const char *name);
  void ha_drop_table(const char *name);
  int ha_create(const char *name, TABLE *form, HA_CREATE_INFO *info);
  int ha_create_handler_files(const char *name, const char *old_name,
                              int action_flag, HA_CREATE_INFO *info);
  int ha_change_partitions(HA_CREATE_INFO *create_info,
                           const char *path,
                           ulonglong *copied,
                           ulonglong *deleted,
                           const uchar *pack_frm_data,
                           size_t pack_frm_len);
  int ha_drop_partitions(const char *path);
  int ha_rename_partitions(const char *path);
  int ha_optimize_partitions(THD *thd);
  int ha_analyze_partitions(THD *thd);
  int ha_check_partitions(THD *thd);
  int ha_repair_partitions(THD *thd);
  void adjust_next_insert_id_after_explicit_value(ulonglong nr);
  int update_auto_increment();
  void print_keydup_error(uint key_nr, const char *msg);
  virtual void print_error(int error, myf errflag);
  virtual In_C_you_should_use_my_bool_instead() get_error_message(int error, String *buf);
  uint get_dup_key(int error);
  virtual void change_table_ptr(TABLE *table_arg, TABLE_SHARE *share)
  {
    table= table_arg;
    table_share= share;
  }
  virtual double scan_time()
  { return ((double) (ulonglong) (stats.data_file_length)) / 4096 + 2; }
  virtual double read_time(uint index, uint ranges, ha_rows rows)
  { return ((double) (ulonglong) (ranges+rows)); }
  virtual const key_map *keys_to_use_for_scanning() { return &key_map_empty; }
  In_C_you_should_use_my_bool_instead() has_transactions()
  { return (ha_table_flags() & (1 << 0)) == 0; }
  virtual uint extra_rec_buf_length() const { return 0; }
  virtual In_C_you_should_use_my_bool_instead() is_fatal_error(int error, uint flags)
  {
    if (!error ||
        ((flags & 1) &&
         (error == 121 ||
          error == 141)))
      return (0);
    return (1);
  }
  virtual ha_rows records() { return stats.records; }
  virtual ha_rows estimate_rows_upper_bound()
  { return stats.records+10; }
  virtual enum row_type get_row_type() const { return ROW_TYPE_NOT_USED; }
  virtual const char *index_type(uint key_number) { assert(0); return "";}
  virtual void column_bitmaps_signal();
  uint get_index(void) const { return active_index; }
  virtual int close(void)=0;
  virtual In_C_you_should_use_my_bool_instead() start_bulk_update() { return 1; }
  virtual In_C_you_should_use_my_bool_instead() start_bulk_delete() { return 1; }
  virtual int exec_bulk_update(uint *dup_key_found)
  {
    assert((0));
    return 131;
  }
  virtual void end_bulk_update() { return; }
  virtual int end_bulk_delete()
  {
    assert((0));
    return 131;
  }
  virtual int index_read_map(uchar * buf, const uchar * key,
                             key_part_map keypart_map,
                             enum ha_rkey_function find_flag)
  {
    uint key_len= calculate_key_len(table, active_index, key, keypart_map);
    return index_read(buf, key, key_len, find_flag);
  }
  virtual int index_read_idx_map(uchar * buf, uint index, const uchar * key,
                                 key_part_map keypart_map,
                                 enum ha_rkey_function find_flag);
  virtual int index_next(uchar * buf)
   { return 131; }
  virtual int index_prev(uchar * buf)
   { return 131; }
  virtual int index_first(uchar * buf)
   { return 131; }
  virtual int index_last(uchar * buf)
   { return 131; }
  virtual int index_next_same(uchar *buf, const uchar *key, uint keylen);
  virtual int index_read_last_map(uchar * buf, const uchar * key,
                                  key_part_map keypart_map)
  {
    uint key_len= calculate_key_len(table, active_index, key, keypart_map);
    return index_read_last(buf, key, key_len);
  }
  virtual int read_multi_range_first(KEY_MULTI_RANGE **found_range_p,
                                     KEY_MULTI_RANGE *ranges, uint range_count,
                                     In_C_you_should_use_my_bool_instead() sorted, HANDLER_BUFFER *buffer);
  virtual int read_multi_range_next(KEY_MULTI_RANGE **found_range_p);
  virtual int read_range_first(const key_range *start_key,
                               const key_range *end_key,
                               In_C_you_should_use_my_bool_instead() eq_range, In_C_you_should_use_my_bool_instead() sorted);
  virtual int read_range_next();
  int compare_key(key_range *range);
  virtual int ft_init() { return 131; }
  void ft_end() { ft_handler=NULL; }
  virtual FT_INFO *ft_init_ext(uint flags, uint inx,String *key)
    { return NULL; }
  virtual int ft_read(uchar *buf) { return 131; }
  virtual int rnd_next(uchar *buf)=0;
  virtual int rnd_pos(uchar * buf, uchar *pos)=0;
  virtual int rnd_pos_by_record(uchar *record)
    {
      position(record);
      return rnd_pos(record, ref);
    }
  virtual int read_first_row(uchar *buf, uint primary_key);
  virtual int restart_rnd_next(uchar *buf, uchar *pos)
    { return 131; }
  virtual int rnd_same(uchar *buf, uint inx)
    { return 131; }
  virtual ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key)
    { return (ha_rows) 10; }
  virtual void position(const uchar *record)=0;
  virtual int info(uint)=0;
  virtual void get_dynamic_partition_info(PARTITION_INFO *stat_info,
                                          uint part_id);
  virtual int extra(enum ha_extra_function operation)
  { return 0; }
  virtual int extra_opt(enum ha_extra_function operation, ulong cache_size)
  { return extra(operation); }
  virtual In_C_you_should_use_my_bool_instead() was_semi_consistent_read() { return 0; }
  virtual void try_semi_consistent_read(In_C_you_should_use_my_bool_instead()) {}
  virtual void unlock_row() {}
  virtual int start_stmt(THD *thd, thr_lock_type lock_type) {return 0;}
  virtual void get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values);
  void set_next_insert_id(ulonglong id)
  {
    do {_db_pargs_(1488,"info"); _db_doprnt_ ("auto_increment: next value %lu", (ulong)id);} while(0);
    next_insert_id= id;
  }
  void restore_auto_increment(ulonglong prev_insert_id)
  {
    next_insert_id= (prev_insert_id > 0) ? prev_insert_id :
      insert_id_for_cur_row;
  }
  virtual void update_create_info(HA_CREATE_INFO *create_info) {}
  int check_old_types();
  virtual int assign_to_keycache(THD* thd, HA_CHECK_OPT* check_opt)
  { return -1; }
  virtual int preload_keys(THD* thd, HA_CHECK_OPT* check_opt)
  { return -1; }
  virtual int dump(THD* thd, int fd = -1) { return 131; }
  virtual int indexes_are_disabled(void) {return 0;}
  virtual int net_read_dump(NET* net) { return 131; }
  virtual char *update_table_comment(const char * comment)
  { return (char*) comment;}
  virtual void append_create_info(String *packet) {}
  virtual In_C_you_should_use_my_bool_instead() is_fk_defined_on_table_or_index(uint index)
  { return (0); }
  virtual char* get_foreign_key_create_info()
  { return(NULL);}
  virtual char* get_tablespace_name(THD *thd, char *name, uint name_len)
  { return(NULL);}
  virtual In_C_you_should_use_my_bool_instead() can_switch_engines() { return 1; }
  virtual int get_foreign_key_list(THD *thd, List<FOREIGN_KEY_INFO> *f_key_list)
  { return 0; }
  virtual uint referenced_by_foreign_key() { return 0;}
  virtual void init_table_handle_for_HANDLER()
  { return; }
  virtual void free_foreign_key_create_info(char* str) {}
  virtual const char *table_type() const =0;
  virtual const char **bas_ext() const =0;
  virtual int get_default_no_partitions(HA_CREATE_INFO *info) { return 1;}
  virtual void set_auto_partitions(partition_info *part_info) { return; }
  virtual In_C_you_should_use_my_bool_instead() get_no_parts(const char *name,
                            uint *no_parts)
  {
    *no_parts= 0;
    return 0;
  }
  virtual void set_part_info(partition_info *part_info) {return;}
  virtual ulong index_flags(uint idx, uint part, In_C_you_should_use_my_bool_instead() all_parts) const =0;
  virtual int add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys)
  { return (131); }
  virtual int prepare_drop_index(TABLE *table_arg, uint *key_num,
                                 uint num_of_keys)
  { return (131); }
  virtual int final_drop_index(TABLE *table_arg)
  { return (131); }
  uint max_record_length() const
  { return ((65535) < (max_supported_record_length()) ? (65535) : (max_supported_record_length())); }
  uint max_keys() const
  { return ((64) < (max_supported_keys()) ? (64) : (max_supported_keys())); }
  uint max_key_parts() const
  { return ((16) < (max_supported_key_parts()) ? (16) : (max_supported_key_parts())); }
  uint max_key_length() const
  { return ((3072) < (max_supported_key_length()) ? (3072) : (max_supported_key_length())); }
  uint max_key_part_length() const
  { return ((3072) < (max_supported_key_part_length()) ? (3072) : (max_supported_key_part_length())); }
  virtual uint max_supported_record_length() const { return 65535; }
  virtual uint max_supported_keys() const { return 0; }
  virtual uint max_supported_key_parts() const { return 16; }
  virtual uint max_supported_key_length() const { return 3072; }
  virtual uint max_supported_key_part_length() const { return 255; }
  virtual uint min_record_length(uint options) const { return 1; }
  virtual In_C_you_should_use_my_bool_instead() low_byte_first() const { return 1; }
  virtual uint checksum() const { return 0; }
  virtual In_C_you_should_use_my_bool_instead() is_crashed() const { return 0; }
  virtual In_C_you_should_use_my_bool_instead() auto_repair() const { return 0; }
  virtual uint lock_count(void) const { return 1; }
  virtual THR_LOCK_DATA **store_lock(THD *thd,
         THR_LOCK_DATA **to,
         enum thr_lock_type lock_type)=0;
  virtual uint8 table_cache_type() { return 0; }
  virtual my_bool register_query_cache_table(THD *thd, char *table_key,
                                             uint key_length,
                                             qc_engine_callback
                                             *engine_callback,
                                             ulonglong *engine_data)
  {
    *engine_callback= 0;
    return (1);
  }
 virtual In_C_you_should_use_my_bool_instead() primary_key_is_clustered() { return (0); }
 virtual int cmp_ref(const uchar *ref1, const uchar *ref2)
 {
   return memcmp(ref1, ref2, ref_length);
 }
 virtual const COND *cond_push(const COND *cond) { return cond; };
 virtual void cond_pop() { return; };
 virtual In_C_you_should_use_my_bool_instead() check_if_incompatible_data(HA_CREATE_INFO *create_info,
      uint table_changes)
 { return 1; }
  virtual void use_hidden_primary_key();
protected:
  void ha_statistic_increment(ulong SSV::*offset) const;
  void **ha_data(THD *) const;
  THD *ha_thd(void) const;
  virtual int rename_table(const char *from, const char *to);
  virtual int delete_table(const char *name);
private:
  inline void mark_trx_read_write();
private:
  virtual int open(const char *name, int mode, uint test_if_locked)=0;
  virtual int index_init(uint idx, In_C_you_should_use_my_bool_instead() sorted) { active_index= idx; return 0; }
  virtual int index_end() { active_index= 64; return 0; }
  virtual int rnd_init(In_C_you_should_use_my_bool_instead() scan)= 0;
  virtual int rnd_end() { return 0; }
  virtual int write_row(uchar *buf __attribute__((unused)))
  {
    return 131;
  }
  virtual int update_row(const uchar *old_data __attribute__((unused)),
                         uchar *new_data __attribute__((unused)))
  {
    return 131;
  }
  virtual int delete_row(const uchar *buf __attribute__((unused)))
  {
    return 131;
  }
  virtual int reset() { return 0; }
  virtual Table_flags table_flags(void) const= 0;
  virtual int external_lock(THD *thd __attribute__((unused)),
                            int lock_type __attribute__((unused)))
  {
    return 0;
  }
  virtual void release_auto_increment() { return; };
  virtual int check_for_upgrade(HA_CHECK_OPT *check_opt)
  { return 0; }
  virtual int check(THD* thd, HA_CHECK_OPT* check_opt)
  { return -1; }
  virtual int repair(THD* thd, HA_CHECK_OPT* check_opt)
  { return -1; }
  virtual void start_bulk_insert(ha_rows rows) {}
  virtual int end_bulk_insert() { return 0; }
  virtual int index_read(uchar * buf, const uchar * key, uint key_len,
                         enum ha_rkey_function find_flag)
   { return 131; }
  virtual int index_read_last(uchar * buf, const uchar * key, uint key_len)
   { return ((_my_thread_var())->thr_errno= 131); }
  virtual int bulk_update_row(const uchar *old_data, uchar *new_data,
                              uint *dup_key_found)
  {
    assert((0));
    return 131;
  }
  virtual int delete_all_rows()
  { return ((_my_thread_var())->thr_errno=131); }
  virtual int reset_auto_increment(ulonglong value)
  { return 131; }
  virtual int backup(THD* thd, HA_CHECK_OPT* check_opt)
  { return -1; }
  virtual int restore(THD* thd, HA_CHECK_OPT* check_opt)
  { return -1; }
  virtual int optimize(THD* thd, HA_CHECK_OPT* check_opt)
  { return -1; }
  virtual int analyze(THD* thd, HA_CHECK_OPT* check_opt)
  { return -1; }
  virtual In_C_you_should_use_my_bool_instead() check_and_repair(THD *thd) { return (1); }
  virtual int disable_indexes(uint mode) { return 131; }
  virtual int enable_indexes(uint mode) { return 131; }
  virtual int discard_or_import_tablespace(my_bool discard)
  { return ((_my_thread_var())->thr_errno=131); }
  virtual void prepare_for_alter() { return; }
  virtual void drop_table(const char *name);
  virtual int create(const char *name, TABLE *form, HA_CREATE_INFO *info)=0;
  virtual int create_handler_files(const char *name, const char *old_name,
                                   int action_flag, HA_CREATE_INFO *info)
  { return (0); }
  virtual int change_partitions(HA_CREATE_INFO *create_info,
                                const char *path,
                                ulonglong *copied,
                                ulonglong *deleted,
                                const uchar *pack_frm_data,
                                size_t pack_frm_len)
  { return 131; }
  virtual int drop_partitions(const char *path)
  { return 131; }
  virtual int rename_partitions(const char *path)
  { return 131; }
  virtual int optimize_partitions(THD *thd)
  { return 131; }
  virtual int analyze_partitions(THD *thd)
  { return 131; }
  virtual int check_partitions(THD *thd)
  { return 131; }
  virtual int repair_partitions(THD *thd)
  { return 131; }
};
extern const char *ha_row_type[];
extern const char *tx_isolation_names[];
extern const char *binlog_format_names[];
extern TYPELIB tx_isolation_typelib;
extern TYPELIB myisam_stats_method_typelib;
extern ulong total_ha, total_ha_2pc;
handlerton *ha_default_handlerton(THD *thd);
plugin_ref ha_resolve_by_name(THD *thd, const LEX_STRING *name);
plugin_ref ha_lock_engine(THD *thd, handlerton *hton);
handlerton *ha_resolve_by_legacy_type(THD *thd, enum legacy_db_type db_type);
handler *get_new_handler(TABLE_SHARE *share, MEM_ROOT *alloc,
                         handlerton *db_type);
handlerton *ha_checktype(THD *thd, enum legacy_db_type database_type,
                          In_C_you_should_use_my_bool_instead() no_substitute, In_C_you_should_use_my_bool_instead() report_error);
static inline enum legacy_db_type ha_legacy_type(const handlerton *db_type)
{
  return (db_type == NULL) ? DB_TYPE_UNKNOWN : db_type->db_type;
}
static inline const char *ha_resolve_storage_engine_name(const handlerton *db_type)
{
  return db_type == NULL ? "UNKNOWN" : hton2plugin[db_type->slot]->name.str;
}
static inline In_C_you_should_use_my_bool_instead() ha_check_storage_engine_flag(const handlerton *db_type, uint32 flag)
{
  return db_type == NULL ? (0) : ((db_type->flags & flag) ? 1 : 0);
}
static inline In_C_you_should_use_my_bool_instead() ha_storage_engine_is_enabled(const handlerton *db_type)
{
  return (db_type && db_type->create) ?
         (db_type->state == SHOW_OPTION_YES) : (0);
}
int ha_init_errors(void);
int ha_init(void);
int ha_end(void);
int ha_initialize_handlerton(st_plugin_int *plugin);
int ha_finalize_handlerton(st_plugin_int *plugin);
TYPELIB *ha_known_exts(void);
int ha_panic(enum ha_panic_function flag);
void ha_close_connection(THD* thd);
In_C_you_should_use_my_bool_instead() ha_flush_logs(handlerton *db_type);
void ha_drop_database(char* path);
int ha_create_table(THD *thd, const char *path,
                    const char *db, const char *table_name,
                    HA_CREATE_INFO *create_info,
      In_C_you_should_use_my_bool_instead() update_create_info);
int ha_delete_table(THD *thd, handlerton *db_type, const char *path,
                    const char *db, const char *alias, In_C_you_should_use_my_bool_instead() generate_warning);
In_C_you_should_use_my_bool_instead() ha_show_status(THD *thd, handlerton *db_type, enum ha_stat_type stat);
int ha_create_table_from_engine(THD* thd, const char *db, const char *name);
int ha_discover(THD* thd, const char* dbname, const char* name,
                uchar** frmblob, size_t* frmlen);
int ha_find_files(THD *thd,const char *db,const char *path,
                  const char *wild, In_C_you_should_use_my_bool_instead() dir, List<LEX_STRING>* files);
int ha_table_exists_in_engine(THD* thd, const char* db, const char* name);
extern "C" int ha_init_key_cache(const char *name, KEY_CACHE *key_cache);
int ha_resize_key_cache(KEY_CACHE *key_cache);
int ha_change_key_cache_param(KEY_CACHE *key_cache);
int ha_change_key_cache(KEY_CACHE *old_key_cache, KEY_CACHE *new_key_cache);
int ha_end_key_cache(KEY_CACHE *key_cache);
int ha_release_temporary_latches(THD *thd);
int ha_start_consistent_snapshot(THD *thd);
int ha_commit_or_rollback_by_xid(XID *xid, In_C_you_should_use_my_bool_instead() commit);
int ha_commit_one_phase(THD *thd, In_C_you_should_use_my_bool_instead() all);
int ha_rollback_trans(THD *thd, In_C_you_should_use_my_bool_instead() all);
int ha_prepare(THD *thd);
int ha_recover(HASH *commit_list);
int ha_commit_trans(THD *thd, In_C_you_should_use_my_bool_instead() all);
int ha_autocommit_or_rollback(THD *thd, int error);
int ha_enable_transaction(THD *thd, In_C_you_should_use_my_bool_instead() on);
int ha_rollback_to_savepoint(THD *thd, SAVEPOINT *sv);
int ha_savepoint(THD *thd, SAVEPOINT *sv);
int ha_release_savepoint(THD *thd, SAVEPOINT *sv);
void trans_register_ha(THD *thd, In_C_you_should_use_my_bool_instead() all, handlerton *ht);
#include "parse_file.h"
enum file_opt_type {
  FILE_OPTIONS_STRING,
  FILE_OPTIONS_ESTRING,
  FILE_OPTIONS_ULONGLONG,
  FILE_OPTIONS_REV,
  FILE_OPTIONS_TIMESTAMP,
  FILE_OPTIONS_STRLIST,
  FILE_OPTIONS_ULLLIST
};
struct File_option
{
  LEX_STRING name;
  int offset;
  file_opt_type type;
};
class Unknown_key_hook
{
public:
  Unknown_key_hook() {}
  virtual ~Unknown_key_hook() {}
  virtual In_C_you_should_use_my_bool_instead() process_unknown_string(char *&unknown_key, uchar* base,
                                      MEM_ROOT *mem_root, char *end)= 0;
};
class File_parser_dummy_hook: public Unknown_key_hook
{
public:
  File_parser_dummy_hook() {}
  virtual In_C_you_should_use_my_bool_instead() process_unknown_string(char *&unknown_key, uchar* base,
                                      MEM_ROOT *mem_root, char *end);
};
extern File_parser_dummy_hook file_parser_dummy_hook;
In_C_you_should_use_my_bool_instead() get_file_options_ulllist(char *&ptr, char *end, char *line,
                              uchar* base, File_option *parameter,
                              MEM_ROOT *mem_root);
char *
parse_escaped_string(char *ptr, char *end, MEM_ROOT *mem_root, LEX_STRING *str);
class File_parser;
File_parser *sql_parse_prepare(const LEX_STRING *file_name,
          MEM_ROOT *mem_root, In_C_you_should_use_my_bool_instead() bad_format_errors);
my_bool
sql_create_definition_file(const LEX_STRING *dir, const LEX_STRING *file_name,
      const LEX_STRING *type,
      uchar* base, File_option *parameters, uint versions);
my_bool rename_in_schema_file(const char *schema, const char *old_name,
                              const char *new_name, ulonglong revision,
                              uint num_view_backups);
class File_parser: public Sql_alloc
{
  char *buff, *start, *end;
  LEX_STRING file_type;
  my_bool content_ok;
public:
  File_parser() :buff(0), start(0), end(0), content_ok(0)
    { file_type.str= 0; file_type.length= 0; }
  my_bool ok() { return content_ok; }
  LEX_STRING *type() { return &file_type; }
  my_bool parse(uchar* base, MEM_ROOT *mem_root,
  struct File_option *parameters, uint required,
                Unknown_key_hook *hook);
  friend File_parser *sql_parse_prepare(const LEX_STRING *file_name,
     MEM_ROOT *mem_root,
     In_C_you_should_use_my_bool_instead() bad_format_errors);
};
#include "table.h"
class Item;
class Item_subselect;
class GRANT_TABLE;
class st_select_lex_unit;
class st_select_lex;
class partition_info;
class COND_EQUAL;
class Security_context;
class View_creation_ctx : public Default_object_creation_ctx,
                          public Sql_alloc
{
public:
  static View_creation_ctx *create(THD *thd);
  static View_creation_ctx *create(THD *thd,
                                   TABLE_LIST *view);
private:
  View_creation_ctx(THD *thd)
    : Default_object_creation_ctx(thd)
  { }
};
typedef struct st_order {
  struct st_order *next;
  Item **item;
  Item *item_ptr;
  Item **item_copy;
  int counter;
  In_C_you_should_use_my_bool_instead() asc;
  In_C_you_should_use_my_bool_instead() free_me;
  In_C_you_should_use_my_bool_instead() in_field_list;
  In_C_you_should_use_my_bool_instead() counter_used;
  Field *field;
  char *buff;
  table_map used, depend_map;
} ORDER;
typedef struct st_grant_info
{
  GRANT_TABLE *grant_table;
  uint version;
  ulong privilege;
  ulong want_privilege;
  ulong orig_want_privilege;
} GRANT_INFO;
enum tmp_table_type
{
  NO_TMP_TABLE, NON_TRANSACTIONAL_TMP_TABLE, TRANSACTIONAL_TMP_TABLE,
  INTERNAL_TMP_TABLE, SYSTEM_TMP_TABLE
};
enum trg_event_type
{
  TRG_EVENT_INSERT= 0,
  TRG_EVENT_UPDATE= 1,
  TRG_EVENT_DELETE= 2,
  TRG_EVENT_MAX
};
enum frm_type_enum
{
  FRMTYPE_ERROR= 0,
  FRMTYPE_TABLE,
  FRMTYPE_VIEW
};
enum release_type { RELEASE_NORMAL, RELEASE_WAIT_FOR_DROP };
typedef struct st_filesort_info
{
  IO_CACHE *io_cache;
  uchar **sort_keys;
  uchar *buffpek;
  uint buffpek_len;
  uchar *addon_buf;
  size_t addon_length;
  struct st_sort_addon_field *addon_field;
  void (*unpack)(struct st_sort_addon_field *, uchar *);
  uchar *record_pointers;
  ha_rows found_records;
} FILESORT_INFO;
enum timestamp_auto_set_type
{
  TIMESTAMP_NO_AUTO_SET= 0, TIMESTAMP_AUTO_SET_ON_INSERT= 1,
  TIMESTAMP_AUTO_SET_ON_UPDATE= 2, TIMESTAMP_AUTO_SET_ON_BOTH= 3
};
class Field_timestamp;
class Field_blob;
class Table_triggers_list;
enum enum_table_category
{
  TABLE_UNKNOWN_CATEGORY=0,
  TABLE_CATEGORY_TEMPORARY=1,
  TABLE_CATEGORY_USER=2,
  TABLE_CATEGORY_SYSTEM=3,
  TABLE_CATEGORY_INFORMATION=4,
  TABLE_CATEGORY_PERFORMANCE=5
};
typedef enum enum_table_category TABLE_CATEGORY;
TABLE_CATEGORY get_table_category(const LEX_STRING *db,
                                  const LEX_STRING *name);
typedef struct st_table_share
{
  st_table_share() {}
  TABLE_CATEGORY table_category;
  HASH name_hash;
  MEM_ROOT mem_root;
  TYPELIB keynames;
  TYPELIB fieldnames;
  TYPELIB *intervals;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  struct st_table_share *next,
    **prev;
  Field **field;
  Field **found_next_number_field;
  Field *timestamp_field;
  KEY *key_info;
  uint *blob_field;
  uchar *default_values;
  LEX_STRING comment;
  CHARSET_INFO *table_charset;
  MY_BITMAP all_set;
  LEX_STRING table_cache_key;
  LEX_STRING db;
  LEX_STRING table_name;
  LEX_STRING path;
  LEX_STRING normalized_path;
  LEX_STRING connect_string;
  key_map keys_in_use;
  key_map keys_for_keyread;
  ha_rows min_rows, max_rows;
  ulong avg_row_length;
  ulong raid_chunksize;
  ulong version, mysql_version;
  ulong timestamp_offset;
  ulong reclength;
  plugin_ref db_plugin;
  inline handlerton *db_type() const
  {
    return db_plugin ? ((handlerton*)((db_plugin)[0]->data)) : NULL;
  }
  enum row_type row_type;
  enum tmp_table_type tmp_table;
  enum ha_choice transactional;
  enum ha_choice page_checksum;
  uint ref_count;
  uint open_count;
  uint blob_ptr_size;
  uint key_block_size;
  uint null_bytes, last_null_bit_pos;
  uint fields;
  uint rec_buff_length;
  uint keys, key_parts;
  uint max_key_length, max_unique_length, total_key_length;
  uint uniques;
  uint null_fields;
  uint blob_fields;
  uint timestamp_field_offset;
  uint varchar_fields;
  uint db_create_options;
  uint db_options_in_use;
  uint db_record_offset;
  uint raid_type, raid_chunks;
  uint rowid_field_offset;
  uint primary_key;
  uint next_number_index;
  uint next_number_key_offset;
  uint next_number_keypart;
  uint error, open_errno, errarg;
  uint column_bitmap_size;
  uchar frm_version;
  In_C_you_should_use_my_bool_instead() null_field_first;
  In_C_you_should_use_my_bool_instead() system;
  In_C_you_should_use_my_bool_instead() crypted;
  In_C_you_should_use_my_bool_instead() db_low_byte_first;
  In_C_you_should_use_my_bool_instead() crashed;
  In_C_you_should_use_my_bool_instead() is_view;
  In_C_you_should_use_my_bool_instead() name_lock, replace_with_name_lock;
  In_C_you_should_use_my_bool_instead() waiting_on_cond;
  ulong table_map_id;
  ulonglong table_map_version;
  int cached_row_logging_check;
  void set_table_cache_key(char *key_buff, uint key_length)
  {
    table_cache_key.str= key_buff;
    table_cache_key.length= key_length;
    db.str= table_cache_key.str;
    db.length= strlen(db.str);
    table_name.str= db.str + db.length + 1;
    table_name.length= strlen(table_name.str);
  }
  void set_table_cache_key(char *key_buff, const char *key, uint key_length)
  {
    memcpy(key_buff, key, key_length);
    set_table_cache_key(key_buff, key_length);
  }
  inline In_C_you_should_use_my_bool_instead() honor_global_locks()
  {
    return ((table_category == TABLE_CATEGORY_USER)
            || (table_category == TABLE_CATEGORY_SYSTEM));
  }
  inline In_C_you_should_use_my_bool_instead() require_write_privileges()
  {
    return (table_category == TABLE_CATEGORY_PERFORMANCE);
  }
  inline ulong get_table_def_version()
  {
    return table_map_id;
  }
  enum enum_table_ref_type get_table_ref_type() const
  {
    if (is_view)
      return TABLE_REF_VIEW;
    switch (tmp_table) {
    case NO_TMP_TABLE:
      return TABLE_REF_BASE_TABLE;
    case SYSTEM_TMP_TABLE:
      return TABLE_REF_I_S_TABLE;
    default:
      return TABLE_REF_TMP_TABLE;
    }
  }
  ulong get_table_ref_version() const
  {
    return (tmp_table == SYSTEM_TMP_TABLE || is_view) ? 0 : table_map_id;
  }
} TABLE_SHARE;
extern ulong refresh_version;
enum index_hint_type
{
  INDEX_HINT_IGNORE,
  INDEX_HINT_USE,
  INDEX_HINT_FORCE
};
struct st_table {
  st_table() {}
  TABLE_SHARE *s;
  handler *file;
  struct st_table *next, *prev;
  struct st_table *parent;
  TABLE_LIST *child_l;
  TABLE_LIST **child_last_l;
  THD *in_use;
  Field **field;
  uchar *record[2];
  uchar *write_row_record;
  uchar *insert_values;
  key_map covering_keys;
  key_map quick_keys, merge_keys;
  key_map keys_in_use_for_query;
  key_map keys_in_use_for_group_by;
  key_map keys_in_use_for_order_by;
  KEY *key_info;
  Field *next_number_field;
  Field *found_next_number_field;
  Field_timestamp *timestamp_field;
  Table_triggers_list *triggers;
  TABLE_LIST *pos_in_table_list;
  ORDER *group;
  const char *alias;
  uchar *null_flags;
  my_bitmap_map *bitmap_init_value;
  MY_BITMAP def_read_set, def_write_set, tmp_set;
  MY_BITMAP *read_set, *write_set;
  query_id_t query_id;
  ha_rows quick_rows[64];
  key_part_map const_key_parts[64];
  uint quick_key_parts[64];
  uint quick_n_ranges[64];
  ha_rows quick_condition_rows;
  timestamp_auto_set_type timestamp_field_type;
  table_map map;
  uint lock_position;
  uint lock_data_start;
  uint lock_count;
  uint tablenr,used_fields;
  uint temp_pool_slot;
  uint status;
  uint db_stat;
  uint derived_select_number;
  int current_lock;
  my_bool copy_blobs;
  uint maybe_null;
  my_bool null_row;
  my_bool force_index;
  my_bool distinct,const_table,no_rows;
  my_bool key_read, no_keyread;
  my_bool open_placeholder;
  my_bool locked_by_logger;
  my_bool no_replicate;
  my_bool locked_by_name;
  my_bool fulltext_searched;
  my_bool no_cache;
  my_bool open_by_handler;
  my_bool auto_increment_field_not_null;
  my_bool insert_or_update;
  my_bool alias_name_used;
  my_bool get_fields_in_item_tree;
  my_bool children_attached;
  REGINFO reginfo;
  MEM_ROOT mem_root;
  GRANT_INFO grant;
  FILESORT_INFO sort;
  In_C_you_should_use_my_bool_instead() fill_item_list(List<Item> *item_list) const;
  void reset_item_list(List<Item> *item_list) const;
  void clear_column_bitmaps(void);
  void prepare_for_position(void);
  void mark_columns_used_by_index_no_reset(uint index, MY_BITMAP *map);
  void mark_columns_used_by_index(uint index);
  void restore_column_maps_after_mark_index();
  void mark_auto_increment_column(void);
  void mark_columns_needed_for_update(void);
  void mark_columns_needed_for_delete(void);
  void mark_columns_needed_for_insert(void);
  inline void column_bitmaps_set(MY_BITMAP *read_set_arg,
                                 MY_BITMAP *write_set_arg)
  {
    read_set= read_set_arg;
    write_set= write_set_arg;
    if (file)
      file->column_bitmaps_signal();
  }
  inline void column_bitmaps_set_no_signal(MY_BITMAP *read_set_arg,
                                           MY_BITMAP *write_set_arg)
  {
    read_set= read_set_arg;
    write_set= write_set_arg;
  }
  inline void use_all_columns()
  {
    column_bitmaps_set(&s->all_set, &s->all_set);
  }
  inline void default_column_bitmaps()
  {
    read_set= &def_read_set;
    write_set= &def_write_set;
  }
  inline In_C_you_should_use_my_bool_instead() is_name_opened() { return db_stat || open_placeholder; }
  inline In_C_you_should_use_my_bool_instead() needs_reopen_or_name_lock()
  { return s->version != refresh_version; }
  In_C_you_should_use_my_bool_instead() is_children_attached(void);
};
enum enum_schema_table_state
{
  NOT_PROCESSED= 0,
  PROCESSED_BY_CREATE_SORT_INDEX,
  PROCESSED_BY_JOIN_EXEC
};
typedef struct st_foreign_key_info
{
  LEX_STRING *forein_id;
  LEX_STRING *referenced_db;
  LEX_STRING *referenced_table;
  LEX_STRING *update_method;
  LEX_STRING *delete_method;
  LEX_STRING *referenced_key_name;
  List<LEX_STRING> foreign_fields;
  List<LEX_STRING> referenced_fields;
} FOREIGN_KEY_INFO;
enum enum_schema_tables
{
  SCH_CHARSETS= 0,
  SCH_COLLATIONS,
  SCH_COLLATION_CHARACTER_SET_APPLICABILITY,
  SCH_COLUMNS,
  SCH_COLUMN_PRIVILEGES,
  SCH_ENGINES,
  SCH_EVENTS,
  SCH_FILES,
  SCH_GLOBAL_STATUS,
  SCH_GLOBAL_VARIABLES,
  SCH_KEY_COLUMN_USAGE,
  SCH_OPEN_TABLES,
  SCH_PARTITIONS,
  SCH_PLUGINS,
  SCH_PROCESSLIST,
  SCH_PROFILES,
  SCH_REFERENTIAL_CONSTRAINTS,
  SCH_PROCEDURES,
  SCH_SCHEMATA,
  SCH_SCHEMA_PRIVILEGES,
  SCH_SESSION_STATUS,
  SCH_SESSION_VARIABLES,
  SCH_STATISTICS,
  SCH_STATUS,
  SCH_TABLES,
  SCH_TABLE_CONSTRAINTS,
  SCH_TABLE_NAMES,
  SCH_TABLE_PRIVILEGES,
  SCH_TRIGGERS,
  SCH_USER_PRIVILEGES,
  SCH_VARIABLES,
  SCH_VIEWS
};
typedef struct st_field_info
{
  const char* field_name;
  uint field_length;
  enum enum_field_types field_type;
  int value;
  uint field_flags;
  const char* old_name;
  uint open_method;
} ST_FIELD_INFO;
struct TABLE_LIST;
typedef class Item COND;
typedef struct st_schema_table
{
  const char* table_name;
  ST_FIELD_INFO *fields_info;
  TABLE *(*create_table) (THD *thd, TABLE_LIST *table_list);
  int (*fill_table) (THD *thd, TABLE_LIST *tables, COND *cond);
  int (*old_format) (THD *thd, struct st_schema_table *schema_table);
  int (*process_table) (THD *thd, TABLE_LIST *tables, TABLE *table,
                        In_C_you_should_use_my_bool_instead() res, LEX_STRING *db_name, LEX_STRING *table_name);
  int idx_field1, idx_field2;
  In_C_you_should_use_my_bool_instead() hidden;
  uint i_s_requested_object;
} ST_SCHEMA_TABLE;
struct st_lex;
class select_union;
class TMP_TABLE_PARAM;
Item *create_view_field(THD *thd, TABLE_LIST *view, Item **field_ref,
                        const char *name);
struct Field_translator
{
  Item *item;
  const char *name;
};
class Natural_join_column: public Sql_alloc
{
public:
  Field_translator *view_field;
  Field *table_field;
  TABLE_LIST *table_ref;
  In_C_you_should_use_my_bool_instead() is_common;
public:
  Natural_join_column(Field_translator *field_param, TABLE_LIST *tab);
  Natural_join_column(Field *field_param, TABLE_LIST *tab);
  const char *name();
  Item *create_item(THD *thd);
  Field *field();
  const char *table_name();
  const char *db_name();
  GRANT_INFO *grant();
};
class Index_hint;
struct TABLE_LIST
{
  TABLE_LIST() {}
  inline void init_one_table(const char *db_name_arg,
                             const char *table_name_arg,
                             enum thr_lock_type lock_type_arg)
  {
    bzero((char*) this, sizeof(*this));
    db= (char*) db_name_arg;
    table_name= alias= (char*) table_name_arg;
    lock_type= lock_type_arg;
  }
  TABLE_LIST *next_local;
  TABLE_LIST *next_global, **prev_global;
  char *db, *alias, *table_name, *schema_table_name;
  char *option;
  Item *on_expr;
  Item *prep_on_expr;
  COND_EQUAL *cond_equal;
  TABLE_LIST *natural_join;
  In_C_you_should_use_my_bool_instead() is_natural_join;
  List<String> *join_using_fields;
  List<Natural_join_column> *join_columns;
  In_C_you_should_use_my_bool_instead() is_join_columns_complete;
  TABLE_LIST *next_name_resolution_table;
  List<Index_hint> *index_hints;
  TABLE *table;
  uint table_id;
  select_union *derived_result;
  TABLE_LIST *correspondent_table;
  st_select_lex_unit *derived;
  ST_SCHEMA_TABLE *schema_table;
  st_select_lex *schema_select_lex;
  In_C_you_should_use_my_bool_instead() schema_table_reformed;
  TMP_TABLE_PARAM *schema_table_param;
  st_select_lex *select_lex;
  st_lex *view;
  Field_translator *field_translation;
  Field_translator *field_translation_end;
  TABLE_LIST *merge_underlying_list;
  List<TABLE_LIST> *view_tables;
  TABLE_LIST *belong_to_view;
  TABLE_LIST *referencing_view;
  TABLE_LIST *parent_l;
  Security_context *security_ctx;
  Security_context *view_sctx;
  In_C_you_should_use_my_bool_instead() allowed_show;
  TABLE_LIST *next_leaf;
  Item *where;
  Item *check_option;
  LEX_STRING select_stmt;
  LEX_STRING md5;
  LEX_STRING source;
  LEX_STRING view_db;
  LEX_STRING view_name;
  LEX_STRING timestamp;
  st_lex_user definer;
  ulonglong file_version;
  ulonglong updatable_view;
  ulonglong revision;
  ulonglong algorithm;
  ulonglong view_suid;
  ulonglong with_check;
  uint8 effective_with_check;
  uint8 effective_algorithm;
  GRANT_INFO grant;
  ulonglong engine_data;
  qc_engine_callback callback_func;
  thr_lock_type lock_type;
  uint outer_join;
  uint shared;
  size_t db_length;
  size_t table_name_length;
  In_C_you_should_use_my_bool_instead() updatable;
  In_C_you_should_use_my_bool_instead() straight;
  In_C_you_should_use_my_bool_instead() updating;
  In_C_you_should_use_my_bool_instead() force_index;
  In_C_you_should_use_my_bool_instead() ignore_leaves;
  table_map dep_tables;
  table_map on_expr_dep_tables;
  struct st_nested_join *nested_join;
  TABLE_LIST *embedding;
  List<TABLE_LIST> *join_list;
  In_C_you_should_use_my_bool_instead() cacheable_table;
  In_C_you_should_use_my_bool_instead() table_in_first_from_clause;
  In_C_you_should_use_my_bool_instead() skip_temporary;
  In_C_you_should_use_my_bool_instead() contain_auto_increment;
  In_C_you_should_use_my_bool_instead() multitable_view;
  In_C_you_should_use_my_bool_instead() compact_view_format;
  In_C_you_should_use_my_bool_instead() where_processed;
  In_C_you_should_use_my_bool_instead() check_option_processed;
  enum frm_type_enum required_type;
  handlerton *db_type;
  char timestamp_buffer[20];
  In_C_you_should_use_my_bool_instead() prelocking_placeholder;
  In_C_you_should_use_my_bool_instead() create;
  In_C_you_should_use_my_bool_instead() internal_tmp_table;
  View_creation_ctx *view_creation_ctx;
  LEX_STRING view_client_cs_name;
  LEX_STRING view_connection_cl_name;
  LEX_STRING view_body_utf8;
  uint8 trg_event_map;
  uint i_s_requested_object;
  In_C_you_should_use_my_bool_instead() has_db_lookup_value;
  In_C_you_should_use_my_bool_instead() has_table_lookup_value;
  uint table_open_method;
  enum enum_schema_table_state schema_table_state;
  void calc_md5(char *buffer);
  void set_underlying_merge();
  int view_check_option(THD *thd, In_C_you_should_use_my_bool_instead() ignore_failure);
  In_C_you_should_use_my_bool_instead() setup_underlying(THD *thd);
  void cleanup_items();
  In_C_you_should_use_my_bool_instead() placeholder()
  {
    return derived || view || schema_table || create && !table->db_stat ||
           !table;
  }
  void print(THD *thd, String *str, enum_query_type query_type);
  In_C_you_should_use_my_bool_instead() check_single_table(TABLE_LIST **table, table_map map,
                          TABLE_LIST *view);
  In_C_you_should_use_my_bool_instead() set_insert_values(MEM_ROOT *mem_root);
  void hide_view_error(THD *thd);
  TABLE_LIST *find_underlying_table(TABLE *table);
  TABLE_LIST *first_leaf_for_name_resolution();
  TABLE_LIST *last_leaf_for_name_resolution();
  In_C_you_should_use_my_bool_instead() is_leaf_for_name_resolution();
  inline TABLE_LIST *top_table()
    { return belong_to_view ? belong_to_view : this; }
  inline In_C_you_should_use_my_bool_instead() prepare_check_option(THD *thd)
  {
    In_C_you_should_use_my_bool_instead() res= (0);
    if (effective_with_check)
      res= prep_check_option(thd, effective_with_check);
    return res;
  }
  inline In_C_you_should_use_my_bool_instead() prepare_where(THD *thd, Item **conds,
                            In_C_you_should_use_my_bool_instead() no_where_clause)
  {
    if (effective_algorithm == 2)
      return prep_where(thd, conds, no_where_clause);
    return (0);
  }
  void register_want_access(ulong want_access);
  In_C_you_should_use_my_bool_instead() prepare_security(THD *thd);
  Security_context *find_view_security_context(THD *thd);
  In_C_you_should_use_my_bool_instead() prepare_view_securety_context(THD *thd);
  void reinit_before_use(THD *thd);
  Item_subselect *containing_subselect();
  In_C_you_should_use_my_bool_instead() process_index_hints(TABLE *table);
  inline ulong get_child_def_version()
  {
    return child_def_version;
  }
  inline void set_child_def_version(ulong version)
  {
    child_def_version= version;
  }
  inline void init_child_def_version()
  {
    child_def_version= ~0UL;
  }
  inline
  In_C_you_should_use_my_bool_instead() is_table_ref_id_equal(TABLE_SHARE *s) const
  {
    return (m_table_ref_type == s->get_table_ref_type() &&
            m_table_ref_version == s->get_table_ref_version());
  }
  inline
  void set_table_ref_id(TABLE_SHARE *s)
  {
    m_table_ref_type= s->get_table_ref_type();
    m_table_ref_version= s->get_table_ref_version();
  }
private:
  In_C_you_should_use_my_bool_instead() prep_check_option(THD *thd, uint8 check_opt_type);
  In_C_you_should_use_my_bool_instead() prep_where(THD *thd, Item **conds, In_C_you_should_use_my_bool_instead() no_where_clause);
  ulong child_def_version;
  enum enum_table_ref_type m_table_ref_type;
  ulong m_table_ref_version;
};
class Item;
class Field_iterator: public Sql_alloc
{
public:
  Field_iterator() {}
  virtual ~Field_iterator() {}
  virtual void set(TABLE_LIST *)= 0;
  virtual void next()= 0;
  virtual In_C_you_should_use_my_bool_instead() end_of_fields()= 0;
  virtual const char *name()= 0;
  virtual Item *create_item(THD *)= 0;
  virtual Field *field()= 0;
};
class Field_iterator_table: public Field_iterator
{
  Field **ptr;
public:
  Field_iterator_table() :ptr(0) {}
  void set(TABLE_LIST *table) { ptr= table->table->field; }
  void set_table(TABLE *table) { ptr= table->field; }
  void next() { ptr++; }
  In_C_you_should_use_my_bool_instead() end_of_fields() { return *ptr == 0; }
  const char *name();
  Item *create_item(THD *thd);
  Field *field() { return *ptr; }
};
class Field_iterator_view: public Field_iterator
{
  Field_translator *ptr, *array_end;
  TABLE_LIST *view;
public:
  Field_iterator_view() :ptr(0), array_end(0) {}
  void set(TABLE_LIST *table);
  void next() { ptr++; }
  In_C_you_should_use_my_bool_instead() end_of_fields() { return ptr == array_end; }
  const char *name();
  Item *create_item(THD *thd);
  Item **item_ptr() {return &ptr->item; }
  Field *field() { return 0; }
  inline Item *item() { return ptr->item; }
  Field_translator *field_translator() { return ptr; }
};
class Field_iterator_natural_join: public Field_iterator
{
  List_iterator_fast<Natural_join_column> column_ref_it;
  Natural_join_column *cur_column_ref;
public:
  Field_iterator_natural_join() :cur_column_ref(NULL) {}
  ~Field_iterator_natural_join() {}
  void set(TABLE_LIST *table);
  void next();
  In_C_you_should_use_my_bool_instead() end_of_fields() { return !cur_column_ref; }
  const char *name() { return cur_column_ref->name(); }
  Item *create_item(THD *thd) { return cur_column_ref->create_item(thd); }
  Field *field() { return cur_column_ref->field(); }
  Natural_join_column *column_ref() { return cur_column_ref; }
};
class Field_iterator_table_ref: public Field_iterator
{
  TABLE_LIST *table_ref, *first_leaf, *last_leaf;
  Field_iterator_table table_field_it;
  Field_iterator_view view_field_it;
  Field_iterator_natural_join natural_join_it;
  Field_iterator *field_it;
  void set_field_iterator();
public:
  Field_iterator_table_ref() :field_it(NULL) {}
  void set(TABLE_LIST *table);
  void next();
  In_C_you_should_use_my_bool_instead() end_of_fields()
  { return (table_ref == last_leaf && field_it->end_of_fields()); }
  const char *name() { return field_it->name(); }
  const char *table_name();
  const char *db_name();
  GRANT_INFO *grant();
  Item *create_item(THD *thd) { return field_it->create_item(thd); }
  Field *field() { return field_it->field(); }
  Natural_join_column *get_or_create_column_ref(TABLE_LIST *parent_table_ref);
  Natural_join_column *get_natural_column_ref();
};
typedef struct st_nested_join
{
  List<TABLE_LIST> join_list;
  table_map used_tables;
  table_map not_null_tables;
  struct st_join_table *first_nested;
  uint counter;
  nested_join_map nj_map;
} NESTED_JOIN;
typedef struct st_changed_table_list
{
  struct st_changed_table_list *next;
  char *key;
  uint32 key_length;
} CHANGED_TABLE_LIST;
typedef struct st_open_table_list{
  struct st_open_table_list *next;
  char *db,*table;
  uint32 in_use,locked;
} OPEN_TABLE_LIST;
typedef struct st_table_field_w_type
{
  LEX_STRING name;
  LEX_STRING type;
  LEX_STRING cset;
} TABLE_FIELD_W_TYPE;
my_bool
table_check_intact(TABLE *table, const uint table_f_count,
                   const TABLE_FIELD_W_TYPE *table_def);
static inline my_bitmap_map *tmp_use_all_columns(TABLE *table,
                                                 MY_BITMAP *bitmap)
{
  my_bitmap_map *old= bitmap->bitmap;
  bitmap->bitmap= table->s->all_set.bitmap;
  return old;
}
static inline void tmp_restore_column_map(MY_BITMAP *bitmap,
                                          my_bitmap_map *old)
{
  bitmap->bitmap= old;
}
static inline my_bitmap_map *dbug_tmp_use_all_columns(TABLE *table,
                                                      MY_BITMAP *bitmap)
{
  return tmp_use_all_columns(table, bitmap);
}
static inline void dbug_tmp_restore_column_map(MY_BITMAP *bitmap,
                                               my_bitmap_map *old)
{
  tmp_restore_column_map(bitmap, old);
}
size_t max_row_length(TABLE *table, const uchar *data);
#include "sql_error.h"
class MYSQL_ERROR: public Sql_alloc
{
public:
  enum enum_warning_level
  { WARN_LEVEL_NOTE, WARN_LEVEL_WARN, WARN_LEVEL_ERROR, WARN_LEVEL_END};
  uint code;
  enum_warning_level level;
  char *msg;
  MYSQL_ERROR(THD *thd, uint code_arg, enum_warning_level level_arg,
       const char *msg_arg)
    :code(code_arg), level(level_arg)
  {
    if (msg_arg)
      set_msg(thd, msg_arg);
  }
  void set_msg(THD *thd, const char *msg_arg);
};
MYSQL_ERROR *push_warning(THD *thd, MYSQL_ERROR::enum_warning_level level,
                          uint code, const char *msg);
void push_warning_printf(THD *thd, MYSQL_ERROR::enum_warning_level level,
    uint code, const char *format, ...);
void mysql_reset_errors(THD *thd, In_C_you_should_use_my_bool_instead() force);
In_C_you_should_use_my_bool_instead() mysqld_show_warnings(THD *thd, ulong levels_to_show);
extern const LEX_STRING warning_level_names[];
#include "field.h"
const uint32 max_field_size= (uint32) 4294967295U;
class Send_field;
class Protocol;
class Create_field;
struct st_cache_field;
int field_conv(Field *to,Field *from);
inline uint get_enum_pack_length(int elements)
{
  return elements < 256 ? 1 : 2;
}
inline uint get_set_pack_length(int elements)
{
  uint len= (elements + 7) / 8;
  return len > 4 ? 8 : len;
}
class Field
{
  Field(const Item &);
  void operator=(Field &);
public:
  static void *operator new(size_t size) {return sql_alloc(size); }
  static void operator delete(void *ptr_arg, size_t size) { ; }
  uchar *ptr;
  uchar *null_ptr;
  struct st_table *table;
  struct st_table *orig_table;
  const char **table_name, *field_name;
  LEX_STRING comment;
  key_map key_start, part_of_key, part_of_key_not_clustered;
  key_map part_of_sortkey;
  enum utype { NONE,DATE,SHIELD,NOEMPTY,CASEUP,PNR,BGNR,PGNR,YES,NO,REL,
  CHECK,EMPTY,UNKNOWN_FIELD,CASEDN,NEXT_NUMBER,INTERVAL_FIELD,
                BIT_FIELD, TIMESTAMP_OLD_FIELD, CAPITALIZE, BLOB_FIELD,
                TIMESTAMP_DN_FIELD, TIMESTAMP_UN_FIELD, TIMESTAMP_DNUN_FIELD};
  enum geometry_type
  {
    GEOM_GEOMETRY = 0, GEOM_POINT = 1, GEOM_LINESTRING = 2, GEOM_POLYGON = 3,
    GEOM_MULTIPOINT = 4, GEOM_MULTILINESTRING = 5, GEOM_MULTIPOLYGON = 6,
    GEOM_GEOMETRYCOLLECTION = 7
  };
  enum imagetype { itRAW, itMBR};
  utype unireg_check;
  uint32 field_length;
  uint32 flags;
  uint16 field_index;
  uchar null_bit;
  In_C_you_should_use_my_bool_instead() is_created_from_null_item;
  Field(uchar *ptr_arg,uint32 length_arg,uchar *null_ptr_arg,
        uchar null_bit_arg, utype unireg_check_arg,
        const char *field_name_arg);
  virtual ~Field() {}
  virtual int store(const char *to, uint length,CHARSET_INFO *cs)=0;
  virtual int store(double nr)=0;
  virtual int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val)=0;
  virtual int store_decimal(const my_decimal *d)=0;
  virtual int store_time(MYSQL_TIME *ltime, timestamp_type t_type);
  int store(const char *to, uint length, CHARSET_INFO *cs,
            enum_check_fields check_level);
  virtual double val_real(void)=0;
  virtual longlong val_int(void)=0;
  virtual my_decimal *val_decimal(my_decimal *);
  inline String *val_str(String *str) { return val_str(str, str); }
  virtual String *val_str(String*,String *)=0;
  String *val_int_as_str(String *val_buffer, my_bool unsigned_flag);
  virtual In_C_you_should_use_my_bool_instead() str_needs_quotes() { return (0); }
  virtual Item_result result_type () const=0;
  virtual Item_result cmp_type () const { return result_type(); }
  virtual Item_result cast_to_int_type () const { return result_type(); }
  static In_C_you_should_use_my_bool_instead() type_can_have_key_part(enum_field_types);
  static enum_field_types field_type_merge(enum_field_types, enum_field_types);
  static Item_result result_merge_type(enum_field_types);
  virtual In_C_you_should_use_my_bool_instead() eq(Field *field)
  {
    return (ptr == field->ptr && null_ptr == field->null_ptr &&
            null_bit == field->null_bit);
  }
  virtual In_C_you_should_use_my_bool_instead() eq_def(Field *field);
  virtual uint32 pack_length() const { return (uint32) field_length; }
  virtual uint32 pack_length_in_rec() const { return pack_length(); }
  virtual int compatible_field_size(uint field_metadata);
  virtual uint pack_length_from_metadata(uint field_metadata)
  { return field_metadata; }
  virtual uint row_pack_length() { return 0; }
  virtual int save_field_metadata(uchar *first_byte)
  { return do_save_field_metadata(first_byte); }
  virtual uint32 data_length() { return pack_length(); }
  virtual uint32 sort_length() const { return pack_length(); }
  virtual uint32 max_data_length() const {
    return pack_length();
  };
  virtual int reset(void) { bzero(ptr,pack_length()); return 0; }
  virtual void reset_fields() {}
  virtual void set_default()
  {
    my_ptrdiff_t l_offset= (my_ptrdiff_t) (table->s->default_values -
       table->record[0]);
    memcpy(ptr, ptr + l_offset, pack_length());
    if (null_ptr)
      *null_ptr= ((*null_ptr & (uchar) ~null_bit) |
    null_ptr[l_offset] & null_bit);
  }
  virtual In_C_you_should_use_my_bool_instead() binary() const { return 1; }
  virtual In_C_you_should_use_my_bool_instead() zero_pack() const { return 1; }
  virtual enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  virtual uint32 key_length() const { return pack_length(); }
  virtual enum_field_types type() const =0;
  virtual enum_field_types real_type() const { return type(); }
  inline int cmp(const uchar *str) { return cmp(ptr,str); }
  virtual int cmp_max(const uchar *a, const uchar *b, uint max_len)
    { return cmp(a, b); }
  virtual int cmp(const uchar *,const uchar *)=0;
  virtual int cmp_binary(const uchar *a,const uchar *b, uint32 max_length=~0L)
  { return memcmp(a,b,pack_length()); }
  virtual int cmp_offset(uint row_offset)
  { return cmp(ptr,ptr+row_offset); }
  virtual int cmp_binary_offset(uint row_offset)
  { return cmp_binary(ptr, ptr+row_offset); };
  virtual int key_cmp(const uchar *a,const uchar *b)
  { return cmp(a, b); }
  virtual int key_cmp(const uchar *str, uint length)
  { return cmp(ptr,str); }
  virtual uint decimals() const { return 0; }
  virtual void sql_type(String &str) const =0;
  virtual uint size_of() const =0;
  inline In_C_you_should_use_my_bool_instead() is_null(my_ptrdiff_t row_offset= 0)
  { return null_ptr ? (null_ptr[row_offset] & null_bit ? 1 : 0) : table->null_row; }
  inline In_C_you_should_use_my_bool_instead() is_real_null(my_ptrdiff_t row_offset= 0)
    { return null_ptr ? (null_ptr[row_offset] & null_bit ? 1 : 0) : 0; }
  inline In_C_you_should_use_my_bool_instead() is_null_in_record(const uchar *record)
  {
    if (!null_ptr)
      return 0;
    return ((record[(uint) (null_ptr -table->record[0])] & null_bit) ? 1 : 0);
  }
  inline In_C_you_should_use_my_bool_instead() is_null_in_record_with_offset(my_ptrdiff_t offset)
  {
    if (!null_ptr)
      return 0;
    return ((null_ptr[offset] & null_bit) ? 1 : 0);
  }
  inline void set_null(my_ptrdiff_t row_offset= 0)
    { if (null_ptr) null_ptr[row_offset]|= null_bit; }
  inline void set_notnull(my_ptrdiff_t row_offset= 0)
    { if (null_ptr) null_ptr[row_offset]&= (uchar) ~null_bit; }
  inline In_C_you_should_use_my_bool_instead() maybe_null(void) { return null_ptr != 0 || table->maybe_null; }
  inline In_C_you_should_use_my_bool_instead() real_maybe_null(void) { return null_ptr != 0; }
  enum {
    LAST_NULL_BYTE_UNDEF= 0
  };
  size_t last_null_byte() const {
    size_t bytes= do_last_null_byte();
    do {_db_pargs_(284,"debug"); _db_doprnt_ ("last_null_byte() ==> %ld", (long) bytes);} while(0);
    assert(bytes <= table->s->null_bytes);
    return bytes;
  }
  virtual void make_field(Send_field *);
  virtual void sort_string(uchar *buff,uint length)=0;
  virtual In_C_you_should_use_my_bool_instead() optimize_range(uint idx, uint part);
  virtual In_C_you_should_use_my_bool_instead() can_be_compared_as_longlong() const { return (0); }
  virtual void free() {}
  virtual Field *new_field(MEM_ROOT *root, struct st_table *new_table,
                           In_C_you_should_use_my_bool_instead() keep_type);
  virtual Field *new_key_field(MEM_ROOT *root, struct st_table *new_table,
                               uchar *new_ptr, uchar *new_null_ptr,
                               uint new_null_bit);
  Field *clone(MEM_ROOT *mem_root, struct st_table *new_table);
  inline void move_field(uchar *ptr_arg,uchar *null_ptr_arg,uchar null_bit_arg)
  {
    ptr=ptr_arg; null_ptr=null_ptr_arg; null_bit=null_bit_arg;
  }
  inline void move_field(uchar *ptr_arg) { ptr=ptr_arg; }
  virtual void move_field_offset(my_ptrdiff_t ptr_diff)
  {
    ptr=(uchar*) ((uchar*) (ptr)+ptr_diff);
    if (null_ptr)
      null_ptr=(uchar*) ((uchar*) (null_ptr)+ptr_diff);
  }
  virtual void get_image(uchar *buff, uint length, CHARSET_INFO *cs)
    { memcpy(buff,ptr,length); }
  virtual void set_image(const uchar *buff,uint length, CHARSET_INFO *cs)
    { memcpy(ptr,buff,length); }
  virtual uint get_key_image(uchar *buff, uint length, imagetype type)
  {
    get_image(buff, length, &my_charset_bin);
    return length;
  }
  virtual void set_key_image(const uchar *buff,uint length)
    { set_image(buff,length, &my_charset_bin); }
  inline longlong val_int_offset(uint row_offset)
    {
      ptr+=row_offset;
      longlong tmp=val_int();
      ptr-=row_offset;
      return tmp;
    }
  inline longlong val_int(const uchar *new_ptr)
  {
    uchar *old_ptr= ptr;
    longlong return_value;
    ptr= (uchar*) new_ptr;
    return_value= val_int();
    ptr= old_ptr;
    return return_value;
  }
  inline String *val_str(String *str, const uchar *new_ptr)
  {
    uchar *old_ptr= ptr;
    ptr= (uchar*) new_ptr;
    val_str(str);
    ptr= old_ptr;
    return str;
  }
  virtual In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first);
  uchar *pack(uchar *to, const uchar *from)
  {
    const char *_db_func_, *_db_file_; uint _db_level_; char **_db_framep_; _db_enter_ ("Field::pack","./sql/field.h",390,&_db_func_,&_db_file_,&_db_level_, &_db_framep_);
    uchar *result= this->pack(to, from, UINT_MAX, table->s->db_low_byte_first);
    do {_db_return_ (392, &_db_func_, &_db_file_, &_db_level_); return(result);} while(0);
  }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, In_C_you_should_use_my_bool_instead() low_byte_first);
  const uchar *unpack(uchar* to, const uchar *from)
  {
    const char *_db_func_, *_db_file_; uint _db_level_; char **_db_framep_; _db_enter_ ("Field::unpack","./sql/field.h",402,&_db_func_,&_db_file_,&_db_level_, &_db_framep_);
    const uchar *result= unpack(to, from, 0U, table->s->db_low_byte_first);
    do {_db_return_ (404, &_db_func_, &_db_file_, &_db_level_); return(result);} while(0);
  }
  virtual uchar *pack_key(uchar* to, const uchar *from,
                          uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    return pack(to, from, max_length, low_byte_first);
  }
  virtual uchar *pack_key_from_key_image(uchar* to, const uchar *from,
     uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    return pack(to, from, max_length, low_byte_first);
  }
  virtual const uchar *unpack_key(uchar* to, const uchar *from,
                                  uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    return unpack(to, from, max_length, low_byte_first);
  }
  virtual uint packed_col_length(const uchar *to, uint length)
  { return length;}
  virtual uint max_packed_col_length(uint max_length)
  { return max_length;}
  virtual int pack_cmp(const uchar *a,const uchar *b, uint key_length_arg,
                       my_bool insert_or_update)
  { return cmp(a,b); }
  virtual int pack_cmp(const uchar *b, uint key_length_arg,
                       my_bool insert_or_update)
  { return cmp(ptr,b); }
  uint offset(uchar *record)
  {
    return (uint) (ptr - record);
  }
  void copy_from_tmp(int offset);
  uint fill_cache_field(struct st_cache_field *copy);
  virtual In_C_you_should_use_my_bool_instead() get_date(MYSQL_TIME *ltime,uint fuzzydate);
  virtual In_C_you_should_use_my_bool_instead() get_time(MYSQL_TIME *ltime);
  virtual CHARSET_INFO *charset(void) const { return &my_charset_bin; }
  virtual CHARSET_INFO *sort_charset(void) const { return charset(); }
  virtual In_C_you_should_use_my_bool_instead() has_charset(void) const { return (0); }
  virtual void set_charset(CHARSET_INFO *charset_arg) { }
  virtual enum Derivation derivation(void) const
  { return DERIVATION_IMPLICIT; }
  virtual void set_derivation(enum Derivation derivation_arg) { }
  In_C_you_should_use_my_bool_instead() set_warning(MYSQL_ERROR::enum_warning_level, unsigned int code,
                   int cuted_increment);
  void set_datetime_warning(MYSQL_ERROR::enum_warning_level, uint code,
                            const char *str, uint str_len,
                            timestamp_type ts_type, int cuted_increment);
  void set_datetime_warning(MYSQL_ERROR::enum_warning_level, uint code,
                            longlong nr, timestamp_type ts_type,
                            int cuted_increment);
  void set_datetime_warning(MYSQL_ERROR::enum_warning_level, const uint code,
                            double nr, timestamp_type ts_type);
  inline In_C_you_should_use_my_bool_instead() check_overflow(int op_result)
  {
    return (op_result == 2);
  }
  int warn_if_overflow(int op_result);
  void init(TABLE *table_arg)
  {
    orig_table= table= table_arg;
    table_name= &table_arg->alias;
  }
  virtual uint32 max_display_length()= 0;
  virtual uint is_equal(Create_field *new_field);
  longlong convert_decimal2longlong(const my_decimal *val, In_C_you_should_use_my_bool_instead() unsigned_flag,
                                    int *err);
  inline uint32 char_length() const
  {
    return field_length / charset()->mbmaxlen;
  }
  virtual geometry_type get_geometry_type()
  {
    assert(0);
    return GEOM_GEOMETRY;
  }
  virtual void hash(ulong *nr, ulong *nr2);
  friend In_C_you_should_use_my_bool_instead() reopen_table(THD *,struct st_table *,In_C_you_should_use_my_bool_instead());
  friend int cre_myisam(char * name, register TABLE *form, uint options,
   ulonglong auto_increment_value);
  friend class Copy_field;
  friend class Item_avg_field;
  friend class Item_std_field;
  friend class Item_sum_num;
  friend class Item_sum_sum;
  friend class Item_sum_str;
  friend class Item_sum_count;
  friend class Item_sum_avg;
  friend class Item_sum_std;
  friend class Item_sum_min;
  friend class Item_sum_max;
  friend class Item_func_group_concat;
private:
  virtual size_t do_last_null_byte() const;
  virtual int do_save_field_metadata(uchar *metadata_ptr)
  { return 0; }
};
class Field_num :public Field {
public:
  const uint8 dec;
  In_C_you_should_use_my_bool_instead() zerofill,unsigned_flag;
  Field_num(uchar *ptr_arg,uint32 len_arg, uchar *null_ptr_arg,
     uchar null_bit_arg, utype unireg_check_arg,
     const char *field_name_arg,
            uint8 dec_arg, In_C_you_should_use_my_bool_instead() zero_arg, In_C_you_should_use_my_bool_instead() unsigned_arg);
  Item_result result_type () const { return REAL_RESULT; }
  void prepend_zeros(String *value);
  void add_zerofill_and_unsigned(String &res) const;
  friend class Create_field;
  void make_field(Send_field *);
  uint decimals() const { return (uint) dec; }
  uint size_of() const { return sizeof(*this); }
  In_C_you_should_use_my_bool_instead() eq_def(Field *field);
  int store_decimal(const my_decimal *);
  my_decimal *val_decimal(my_decimal *);
  uint is_equal(Create_field *new_field);
  int check_int(CHARSET_INFO *cs, const char *str, int length,
                const char *int_end, int error);
  In_C_you_should_use_my_bool_instead() get_int(CHARSET_INFO *cs, const char *from, uint len,
               longlong *rnd, ulonglong unsigned_max,
               longlong signed_min, longlong signed_max);
};
class Field_str :public Field {
protected:
  CHARSET_INFO *field_charset;
  enum Derivation field_derivation;
public:
  Field_str(uchar *ptr_arg,uint32 len_arg, uchar *null_ptr_arg,
     uchar null_bit_arg, utype unireg_check_arg,
     const char *field_name_arg, CHARSET_INFO *charset);
  Item_result result_type () const { return STRING_RESULT; }
  uint decimals() const { return 31; }
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val)=0;
  int store_decimal(const my_decimal *);
  int store(const char *to,uint length,CHARSET_INFO *cs)=0;
  uint size_of() const { return sizeof(*this); }
  CHARSET_INFO *charset(void) const { return field_charset; }
  void set_charset(CHARSET_INFO *charset_arg) { field_charset= charset_arg; }
  enum Derivation derivation(void) const { return field_derivation; }
  virtual void set_derivation(enum Derivation derivation_arg)
  { field_derivation= derivation_arg; }
  In_C_you_should_use_my_bool_instead() binary() const { return field_charset == &my_charset_bin; }
  uint32 max_display_length() { return field_length; }
  friend class Create_field;
  my_decimal *val_decimal(my_decimal *);
  virtual In_C_you_should_use_my_bool_instead() str_needs_quotes() { return (1); }
  In_C_you_should_use_my_bool_instead() compare_str_field_flags(Create_field *new_field, uint32 flags);
  uint is_equal(Create_field *new_field);
};
class Field_longstr :public Field_str
{
protected:
  int report_if_important_data(const char *ptr, const char *end,
                               In_C_you_should_use_my_bool_instead() count_spaces);
public:
  Field_longstr(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
                uchar null_bit_arg, utype unireg_check_arg,
                const char *field_name_arg, CHARSET_INFO *charset_arg)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg, unireg_check_arg,
               field_name_arg, charset_arg)
    {}
  int store_decimal(const my_decimal *d);
  uint32 max_data_length() const;
};
class Field_real :public Field_num {
public:
  my_bool not_fixed;
  Field_real(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
             uchar null_bit_arg, utype unireg_check_arg,
             const char *field_name_arg,
             uint8 dec_arg, In_C_you_should_use_my_bool_instead() zero_arg, In_C_you_should_use_my_bool_instead() unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg, unireg_check_arg,
               field_name_arg, dec_arg, zero_arg, unsigned_arg),
    not_fixed(dec_arg >= 31)
    {}
  int store_decimal(const my_decimal *);
  my_decimal *val_decimal(my_decimal *);
  int truncate(double *nr, double max_length);
  uint32 max_display_length() { return field_length; }
  uint size_of() const { return sizeof(*this); }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, In_C_you_should_use_my_bool_instead() low_byte_first);
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first);
};
class Field_decimal :public Field_real {
public:
  Field_decimal(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
  uchar null_bit_arg,
  enum utype unireg_check_arg, const char *field_name_arg,
  uint8 dec_arg,In_C_you_should_use_my_bool_instead() zero_arg,In_C_you_should_use_my_bool_instead() unsigned_arg)
    :Field_real(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                unireg_check_arg, field_name_arg,
                dec_arg, zero_arg, unsigned_arg)
    {}
  enum_field_types type() const { return MYSQL_TYPE_DECIMAL;}
  enum ha_base_keytype key_type() const
  { return zerofill ? HA_KEYTYPE_BINARY : HA_KEYTYPE_NUM; }
  int reset(void);
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  void overflow(In_C_you_should_use_my_bool_instead() negative);
  In_C_you_should_use_my_bool_instead() zero_pack() const { return 0; }
  void sql_type(String &str) const;
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    return Field::unpack(to, from, param_data, low_byte_first);
  }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    return Field::pack(to, from, max_length, low_byte_first);
  }
};
class Field_new_decimal :public Field_num {
private:
  int do_save_field_metadata(uchar *first_byte);
public:
  uint precision;
  uint bin_size;
  Field_new_decimal(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
                    uchar null_bit_arg,
                    enum utype unireg_check_arg, const char *field_name_arg,
                    uint8 dec_arg, In_C_you_should_use_my_bool_instead() zero_arg, In_C_you_should_use_my_bool_instead() unsigned_arg);
  Field_new_decimal(uint32 len_arg, In_C_you_should_use_my_bool_instead() maybe_null_arg,
                    const char *field_name_arg, uint8 dec_arg,
                    In_C_you_should_use_my_bool_instead() unsigned_arg);
  enum_field_types type() const { return MYSQL_TYPE_NEWDECIMAL;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  Item_result result_type () const { return DECIMAL_RESULT; }
  int reset(void);
  In_C_you_should_use_my_bool_instead() store_value(const my_decimal *decimal_value);
  void set_value_on_overflow(my_decimal *decimal_value, In_C_you_should_use_my_bool_instead() sign);
  int store(const char *to, uint length, CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int store_time(MYSQL_TIME *ltime, timestamp_type t_type);
  int store_decimal(const my_decimal *);
  double val_real(void);
  longlong val_int(void);
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String*, String *);
  int cmp(const uchar *, const uchar *);
  void sort_string(uchar *buff, uint length);
  In_C_you_should_use_my_bool_instead() zero_pack() const { return 0; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return field_length; }
  uint size_of() const { return sizeof(*this); }
  uint32 pack_length() const { return (uint32) bin_size; }
  uint pack_length_from_metadata(uint field_metadata);
  uint row_pack_length() { return pack_length(); }
  int compatible_field_size(uint field_metadata);
  uint is_equal(Create_field *new_field);
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, In_C_you_should_use_my_bool_instead() low_byte_first);
};
class Field_tiny :public Field_num {
public:
  Field_tiny(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
      uchar null_bit_arg,
      enum utype unireg_check_arg, const char *field_name_arg,
      In_C_you_should_use_my_bool_instead() zero_arg, In_C_you_should_use_my_bool_instead() unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
        unireg_check_arg, field_name_arg,
        0, zero_arg,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_TINY;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_BINARY : HA_KEYTYPE_INT8; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int reset(void) { ptr[0]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 1; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return 4; }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    *to= *from;
    return to + 1;
  }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    *to= *from;
    return from + 1;
  }
};
class Field_short :public Field_num {
public:
  Field_short(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
       uchar null_bit_arg,
       enum utype unireg_check_arg, const char *field_name_arg,
       In_C_you_should_use_my_bool_instead() zero_arg, In_C_you_should_use_my_bool_instead() unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
        unireg_check_arg, field_name_arg,
        0, zero_arg,unsigned_arg)
    {}
  Field_short(uint32 len_arg,In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
       In_C_you_should_use_my_bool_instead() unsigned_arg)
    :Field_num((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0,0,
        NONE, field_name_arg, 0, 0, unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_SHORT;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_USHORT_INT : HA_KEYTYPE_SHORT_INT;}
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int reset(void) { ptr[0]=ptr[1]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 2; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return 6; }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    int16 val;
      do { val = (*((int16 *) (from))); } while(0);
      *((uint16*) (to))= (uint16) (val);
    return to + sizeof(val);
  }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    int16 val;
      do { val = (*((int16 *) (from))); } while(0);
      *((uint16*) (to))= (uint16) (val);
    return from + sizeof(val);
  }
};
class Field_medium :public Field_num {
public:
  Field_medium(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
       uchar null_bit_arg,
       enum utype unireg_check_arg, const char *field_name_arg,
       In_C_you_should_use_my_bool_instead() zero_arg, In_C_you_should_use_my_bool_instead() unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
        unireg_check_arg, field_name_arg,
        0, zero_arg,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_INT24;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_UINT24 : HA_KEYTYPE_INT24; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int reset(void) { ptr[0]=ptr[1]=ptr[2]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return 8; }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    return Field::pack(to, from, max_length, low_byte_first);
  }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    return Field::unpack(to, from, param_data, low_byte_first);
  }
};
class Field_long :public Field_num {
public:
  Field_long(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
      uchar null_bit_arg,
      enum utype unireg_check_arg, const char *field_name_arg,
      In_C_you_should_use_my_bool_instead() zero_arg, In_C_you_should_use_my_bool_instead() unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
        unireg_check_arg, field_name_arg,
        0, zero_arg,unsigned_arg)
    {}
  Field_long(uint32 len_arg,In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
      In_C_you_should_use_my_bool_instead() unsigned_arg)
    :Field_num((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0,0,
        NONE, field_name_arg,0,0,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_LONG;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_ULONG_INT : HA_KEYTYPE_LONG_INT; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int reset(void) { ptr[0]=ptr[1]=ptr[2]=ptr[3]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  String *val_str(String*,String *);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return 11; }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    int32 val;
      do { val = (*((long *) (from))); } while(0);
      *((long *) (to))= (long) (val);
    return to + sizeof(val);
  }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    int32 val;
      do { val = (*((long *) (from))); } while(0);
      *((long *) (to))= (long) (val);
    return from + sizeof(val);
  }
};
class Field_longlong :public Field_num {
public:
  Field_longlong(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
       uchar null_bit_arg,
       enum utype unireg_check_arg, const char *field_name_arg,
       In_C_you_should_use_my_bool_instead() zero_arg, In_C_you_should_use_my_bool_instead() unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
        unireg_check_arg, field_name_arg,
        0, zero_arg,unsigned_arg)
    {}
  Field_longlong(uint32 len_arg,In_C_you_should_use_my_bool_instead() maybe_null_arg,
   const char *field_name_arg,
    In_C_you_should_use_my_bool_instead() unsigned_arg)
    :Field_num((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0,0,
        NONE, field_name_arg,0,0,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_LONGLONG;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_ULONGLONG : HA_KEYTYPE_LONGLONG; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int reset(void)
  {
    ptr[0]=ptr[1]=ptr[2]=ptr[3]=ptr[4]=ptr[5]=ptr[6]=ptr[7]=0;
    return 0;
  }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 8; }
  void sql_type(String &str) const;
  In_C_you_should_use_my_bool_instead() can_be_compared_as_longlong() const { return (1); }
  uint32 max_display_length() { return 20; }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    int64 val;
      memcpy(((uchar*) &val),((uchar*) (from)),(sizeof(ulonglong)));
      memcpy(((uchar*) (to)),((uchar*) &val),(sizeof(ulonglong)));
    return to + sizeof(val);
  }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, In_C_you_should_use_my_bool_instead() low_byte_first)
  {
    int64 val;
      memcpy(((uchar*) &val),((uchar*) (from)),(sizeof(ulonglong)));
      memcpy(((uchar*) (to)),((uchar*) &val),(sizeof(ulonglong)));
    return from + sizeof(val);
  }
};
class Field_float :public Field_real {
public:
  Field_float(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
       uchar null_bit_arg,
       enum utype unireg_check_arg, const char *field_name_arg,
              uint8 dec_arg,In_C_you_should_use_my_bool_instead() zero_arg,In_C_you_should_use_my_bool_instead() unsigned_arg)
    :Field_real(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                unireg_check_arg, field_name_arg,
                dec_arg, zero_arg, unsigned_arg)
    {}
  Field_float(uint32 len_arg, In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
       uint8 dec_arg)
    :Field_real((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, (uint) 0,
                NONE, field_name_arg, dec_arg, 0, 0)
    {}
  enum_field_types type() const { return MYSQL_TYPE_FLOAT;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_FLOAT; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int reset(void) { bzero(ptr,sizeof(float)); return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return sizeof(float); }
  uint row_pack_length() { return pack_length(); }
  void sql_type(String &str) const;
private:
  int do_save_field_metadata(uchar *first_byte);
};
class Field_double :public Field_real {
public:
  Field_double(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
        uchar null_bit_arg,
        enum utype unireg_check_arg, const char *field_name_arg,
        uint8 dec_arg,In_C_you_should_use_my_bool_instead() zero_arg,In_C_you_should_use_my_bool_instead() unsigned_arg)
    :Field_real(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                unireg_check_arg, field_name_arg,
                dec_arg, zero_arg, unsigned_arg)
    {}
  Field_double(uint32 len_arg, In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
        uint8 dec_arg)
    :Field_real((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "" : 0, (uint) 0,
                NONE, field_name_arg, dec_arg, 0, 0)
    {}
  Field_double(uint32 len_arg, In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
        uint8 dec_arg, my_bool not_fixed_arg)
    :Field_real((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "" : 0, (uint) 0,
                NONE, field_name_arg, dec_arg, 0, 0)
    {not_fixed= not_fixed_arg; }
  enum_field_types type() const { return MYSQL_TYPE_DOUBLE;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_DOUBLE; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int reset(void) { bzero(ptr,sizeof(double)); return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return sizeof(double); }
  uint row_pack_length() { return pack_length(); }
  void sql_type(String &str) const;
private:
  int do_save_field_metadata(uchar *first_byte);
};
class Field_null :public Field_str {
  static uchar null[1];
public:
  Field_null(uchar *ptr_arg, uint32 len_arg,
      enum utype unireg_check_arg, const char *field_name_arg,
      CHARSET_INFO *cs)
    :Field_str(ptr_arg, len_arg, null, 1,
        unireg_check_arg, field_name_arg, cs)
    {}
  enum_field_types type() const { return MYSQL_TYPE_NULL;}
  int store(const char *to, uint length, CHARSET_INFO *cs)
  { null[0]=1; return 0; }
  int store(double nr) { null[0]=1; return 0; }
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val) { null[0]=1; return 0; }
  int store_decimal(const my_decimal *d) { null[0]=1; return 0; }
  int reset(void) { return 0; }
  double val_real(void) { return 0.0;}
  longlong val_int(void) { return 0;}
  my_decimal *val_decimal(my_decimal *) { return 0; }
  String *val_str(String *value,String *value2)
  { value2->length(0); return value2;}
  int cmp(const uchar *a, const uchar *b) { return 0;}
  void sort_string(uchar *buff, uint length) {}
  uint32 pack_length() const { return 0; }
  void sql_type(String &str) const;
  uint size_of() const { return sizeof(*this); }
  uint32 max_display_length() { return 4; }
};
class Field_timestamp :public Field_str {
public:
  Field_timestamp(uchar *ptr_arg, uint32 len_arg,
                  uchar *null_ptr_arg, uchar null_bit_arg,
    enum utype unireg_check_arg, const char *field_name_arg,
    TABLE_SHARE *share, CHARSET_INFO *cs);
  Field_timestamp(In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
    CHARSET_INFO *cs);
  enum_field_types type() const { return MYSQL_TYPE_TIMESTAMP;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONG_INT; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int reset(void) { ptr[0]=ptr[1]=ptr[2]=ptr[3]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
  In_C_you_should_use_my_bool_instead() can_be_compared_as_longlong() const { return (1); }
  In_C_you_should_use_my_bool_instead() zero_pack() const { return 0; }
  void set_time();
  virtual void set_default()
  {
    if (table->timestamp_field == this &&
        unireg_check != TIMESTAMP_UN_FIELD)
      set_time();
    else
      Field::set_default();
  }
  inline long get_timestamp(my_bool *null_value)
  {
    if ((*null_value= is_null()))
      return 0;
    long tmp;
    do { tmp = (*((long *) (ptr))); } while(0);
    return tmp;
  }
  inline void store_timestamp(my_time_t timestamp)
  {
      *((long *) (ptr))= (long) ((uint32) timestamp);
  }
  In_C_you_should_use_my_bool_instead() get_date(MYSQL_TIME *ltime,uint fuzzydate);
  In_C_you_should_use_my_bool_instead() get_time(MYSQL_TIME *ltime);
  timestamp_auto_set_type get_auto_set_type() const;
};
class Field_year :public Field_tiny {
public:
  Field_year(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
      uchar null_bit_arg,
      enum utype unireg_check_arg, const char *field_name_arg)
    :Field_tiny(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
  unireg_check_arg, field_name_arg, 1, 1)
    {}
  enum_field_types type() const { return MYSQL_TYPE_YEAR;}
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  void sql_type(String &str) const;
  In_C_you_should_use_my_bool_instead() can_be_compared_as_longlong() const { return (1); }
};
class Field_date :public Field_str {
public:
  Field_date(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
      enum utype unireg_check_arg, const char *field_name_arg,
      CHARSET_INFO *cs)
    :Field_str(ptr_arg, 10, null_ptr_arg, null_bit_arg,
        unireg_check_arg, field_name_arg, cs)
    {}
  Field_date(In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
             CHARSET_INFO *cs)
    :Field_str((uchar*) 0,10, maybe_null_arg ? (uchar*) "": 0,0,
        NONE, field_name_arg, cs) {}
  enum_field_types type() const { return MYSQL_TYPE_DATE;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONG_INT; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int reset(void) { ptr[0]=ptr[1]=ptr[2]=ptr[3]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  In_C_you_should_use_my_bool_instead() get_time(MYSQL_TIME *ltime);
  In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
  In_C_you_should_use_my_bool_instead() can_be_compared_as_longlong() const { return (1); }
  In_C_you_should_use_my_bool_instead() zero_pack() const { return 1; }
};
class Field_newdate :public Field_str {
public:
  Field_newdate(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
  enum utype unireg_check_arg, const char *field_name_arg,
  CHARSET_INFO *cs)
    :Field_str(ptr_arg, 10, null_ptr_arg, null_bit_arg,
        unireg_check_arg, field_name_arg, cs)
    {}
  Field_newdate(In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
                CHARSET_INFO *cs)
    :Field_str((uchar*) 0,10, maybe_null_arg ? (uchar*) "": 0,0,
               NONE, field_name_arg, cs) {}
  enum_field_types type() const { return MYSQL_TYPE_DATE;}
  enum_field_types real_type() const { return MYSQL_TYPE_NEWDATE; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_UINT24; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int store_time(MYSQL_TIME *ltime, timestamp_type type);
  int reset(void) { ptr[0]=ptr[1]=ptr[2]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  In_C_you_should_use_my_bool_instead() can_be_compared_as_longlong() const { return (1); }
  In_C_you_should_use_my_bool_instead() zero_pack() const { return 1; }
  In_C_you_should_use_my_bool_instead() get_date(MYSQL_TIME *ltime,uint fuzzydate);
  In_C_you_should_use_my_bool_instead() get_time(MYSQL_TIME *ltime);
};
class Field_time :public Field_str {
public:
  Field_time(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
      enum utype unireg_check_arg, const char *field_name_arg,
      CHARSET_INFO *cs)
    :Field_str(ptr_arg, 8, null_ptr_arg, null_bit_arg,
        unireg_check_arg, field_name_arg, cs)
    {}
  Field_time(In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
             CHARSET_INFO *cs)
    :Field_str((uchar*) 0,8, maybe_null_arg ? (uchar*) "": 0,0,
        NONE, field_name_arg, cs) {}
  enum_field_types type() const { return MYSQL_TYPE_TIME;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_INT24; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int store_time(MYSQL_TIME *ltime, timestamp_type type);
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int reset(void) { ptr[0]=ptr[1]=ptr[2]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  In_C_you_should_use_my_bool_instead() get_date(MYSQL_TIME *ltime, uint fuzzydate);
  In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  In_C_you_should_use_my_bool_instead() get_time(MYSQL_TIME *ltime);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  In_C_you_should_use_my_bool_instead() can_be_compared_as_longlong() const { return (1); }
  In_C_you_should_use_my_bool_instead() zero_pack() const { return 1; }
};
class Field_datetime :public Field_str {
public:
  Field_datetime(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
   enum utype unireg_check_arg, const char *field_name_arg,
   CHARSET_INFO *cs)
    :Field_str(ptr_arg, 19, null_ptr_arg, null_bit_arg,
        unireg_check_arg, field_name_arg, cs)
    {}
  Field_datetime(In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
   CHARSET_INFO *cs)
    :Field_str((uchar*) 0,19, maybe_null_arg ? (uchar*) "": 0,0,
        NONE, field_name_arg, cs) {}
  enum_field_types type() const { return MYSQL_TYPE_DATETIME;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONGLONG; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  uint decimals() const { return 6; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int store_time(MYSQL_TIME *ltime, timestamp_type type);
  int reset(void)
  {
    ptr[0]=ptr[1]=ptr[2]=ptr[3]=ptr[4]=ptr[5]=ptr[6]=ptr[7]=0;
    return 0;
  }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  In_C_you_should_use_my_bool_instead() send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 8; }
  void sql_type(String &str) const;
  In_C_you_should_use_my_bool_instead() can_be_compared_as_longlong() const { return (1); }
  In_C_you_should_use_my_bool_instead() zero_pack() const { return 1; }
  In_C_you_should_use_my_bool_instead() get_date(MYSQL_TIME *ltime,uint fuzzydate);
  In_C_you_should_use_my_bool_instead() get_time(MYSQL_TIME *ltime);
};
class Field_string :public Field_longstr {
public:
  In_C_you_should_use_my_bool_instead() can_alter_field_type;
  Field_string(uchar *ptr_arg, uint32 len_arg,uchar *null_ptr_arg,
        uchar null_bit_arg,
        enum utype unireg_check_arg, const char *field_name_arg,
        CHARSET_INFO *cs)
    :Field_longstr(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                   unireg_check_arg, field_name_arg, cs),
     can_alter_field_type(1) {};
  Field_string(uint32 len_arg,In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
               CHARSET_INFO *cs)
    :Field_longstr((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs),
     can_alter_field_type(1) {};
  enum_field_types type() const
  {
    return ((can_alter_field_type && orig_table &&
             orig_table->s->db_create_options & 1 &&
      field_length >= 4) &&
            orig_table->s->frm_version < (6 +4) ?
     MYSQL_TYPE_VAR_STRING : MYSQL_TYPE_STRING);
  }
  enum ha_base_keytype key_type() const
    { return binary() ? HA_KEYTYPE_BINARY : HA_KEYTYPE_TEXT; }
  In_C_you_should_use_my_bool_instead() zero_pack() const { return 0; }
  int reset(void)
  {
    charset()->cset->fill(charset(),(char*) ptr, field_length,
                          (has_charset() ? ' ' : 0));
    return 0;
  }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int store(double nr) { return Field_str::store(nr); }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  void sql_type(String &str) const;
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first);
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, In_C_you_should_use_my_bool_instead() low_byte_first);
  uint pack_length_from_metadata(uint field_metadata)
  { return (field_metadata & 0x00ff); }
  uint row_pack_length() { return (field_length + 1); }
  int pack_cmp(const uchar *a,const uchar *b,uint key_length,
               my_bool insert_or_update);
  int pack_cmp(const uchar *b,uint key_length,my_bool insert_or_update);
  uint packed_col_length(const uchar *to, uint length);
  uint max_packed_col_length(uint max_length);
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return MYSQL_TYPE_STRING; }
  In_C_you_should_use_my_bool_instead() has_charset(void) const
  { return charset() == &my_charset_bin ? (0) : (1); }
  Field *new_field(MEM_ROOT *root, struct st_table *new_table, In_C_you_should_use_my_bool_instead() keep_type);
  virtual uint get_key_image(uchar *buff,uint length, imagetype type);
private:
  int do_save_field_metadata(uchar *first_byte);
};
class Field_varstring :public Field_longstr {
public:
  static const uint MAX_SIZE;
  uint32 length_bytes;
  Field_varstring(uchar *ptr_arg,
                  uint32 len_arg, uint length_bytes_arg,
                  uchar *null_ptr_arg, uchar null_bit_arg,
    enum utype unireg_check_arg, const char *field_name_arg,
    TABLE_SHARE *share, CHARSET_INFO *cs)
    :Field_longstr(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                   unireg_check_arg, field_name_arg, cs),
     length_bytes(length_bytes_arg)
  {
    share->varchar_fields++;
  }
  Field_varstring(uint32 len_arg,In_C_you_should_use_my_bool_instead() maybe_null_arg,
                  const char *field_name_arg,
                  TABLE_SHARE *share, CHARSET_INFO *cs)
    :Field_longstr((uchar*) 0,len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs),
     length_bytes(len_arg < 256 ? 1 :2)
  {
    share->varchar_fields++;
  }
  enum_field_types type() const { return MYSQL_TYPE_VARCHAR; }
  enum ha_base_keytype key_type() const;
  uint row_pack_length() { return field_length; }
  In_C_you_should_use_my_bool_instead() zero_pack() const { return 0; }
  int reset(void) { bzero(ptr,field_length+length_bytes); return 0; }
  uint32 pack_length() const { return (uint32) field_length+length_bytes; }
  uint32 key_length() const { return (uint32) field_length; }
  uint32 sort_length() const
  {
    return (uint32) field_length + (field_charset == &my_charset_bin ?
                                    length_bytes : 0);
  }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int store(double nr) { return Field_str::store(nr); }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
  int cmp_max(const uchar *, const uchar *, uint max_length);
  int cmp(const uchar *a,const uchar *b)
  {
    return cmp_max(a, b, ~0L);
  }
  void sort_string(uchar *buff,uint length);
  uint get_key_image(uchar *buff,uint length, imagetype type);
  void set_key_image(const uchar *buff,uint length);
  void sql_type(String &str) const;
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first);
  uchar *pack_key(uchar *to, const uchar *from, uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first);
  uchar *pack_key_from_key_image(uchar* to, const uchar *from,
                                 uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first);
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, In_C_you_should_use_my_bool_instead() low_byte_first);
  const uchar *unpack_key(uchar* to, const uchar *from,
                          uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first);
  int pack_cmp(const uchar *a, const uchar *b, uint key_length,
               my_bool insert_or_update);
  int pack_cmp(const uchar *b, uint key_length,my_bool insert_or_update);
  int cmp_binary(const uchar *a,const uchar *b, uint32 max_length=~0L);
  int key_cmp(const uchar *,const uchar*);
  int key_cmp(const uchar *str, uint length);
  uint packed_col_length(const uchar *to, uint length);
  uint max_packed_col_length(uint max_length);
  uint32 data_length();
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return MYSQL_TYPE_VARCHAR; }
  In_C_you_should_use_my_bool_instead() has_charset(void) const
  { return charset() == &my_charset_bin ? (0) : (1); }
  Field *new_field(MEM_ROOT *root, struct st_table *new_table, In_C_you_should_use_my_bool_instead() keep_type);
  Field *new_key_field(MEM_ROOT *root, struct st_table *new_table,
                       uchar *new_ptr, uchar *new_null_ptr,
                       uint new_null_bit);
  uint is_equal(Create_field *new_field);
  void hash(ulong *nr, ulong *nr2);
private:
  int do_save_field_metadata(uchar *first_byte);
};
class Field_blob :public Field_longstr {
protected:
  uint packlength;
  String value;
public:
  Field_blob(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
      enum utype unireg_check_arg, const char *field_name_arg,
      TABLE_SHARE *share, uint blob_pack_length, CHARSET_INFO *cs);
  Field_blob(uint32 len_arg,In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
             CHARSET_INFO *cs)
    :Field_longstr((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs),
    packlength(4)
  {
    flags|= 16;
  }
  Field_blob(uint32 len_arg,In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
      CHARSET_INFO *cs, In_C_you_should_use_my_bool_instead() set_packlength)
    :Field_longstr((uchar*) 0,len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs)
  {
    flags|= 16;
    packlength= 4;
    if (set_packlength)
    {
      uint32 l_char_length= len_arg/cs->mbmaxlen;
      packlength= l_char_length <= 255 ? 1 :
                  l_char_length <= 65535 ? 2 :
                  l_char_length <= 16777215 ? 3 : 4;
    }
  }
  Field_blob(uint32 packlength_arg)
    :Field_longstr((uchar*) 0, 0, (uchar*) "", 0, NONE, "temp", system_charset_info),
    packlength(packlength_arg) {}
  enum_field_types type() const { return MYSQL_TYPE_BLOB;}
  enum ha_base_keytype key_type() const
    { return binary() ? HA_KEYTYPE_VARBINARY2 : HA_KEYTYPE_VARTEXT2; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
  int cmp_max(const uchar *, const uchar *, uint max_length);
  int cmp(const uchar *a,const uchar *b)
    { return cmp_max(a, b, ~0L); }
  int cmp(const uchar *a, uint32 a_length, const uchar *b, uint32 b_length);
  int cmp_binary(const uchar *a,const uchar *b, uint32 max_length=~0L);
  int key_cmp(const uchar *,const uchar*);
  int key_cmp(const uchar *str, uint length);
  uint32 key_length() const { return 0; }
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const
  { return (uint32) (packlength+table->s->blob_ptr_size); }
  uint32 pack_length_no_ptr() const
  { return (uint32) (packlength); }
  uint row_pack_length() { return pack_length_no_ptr(); }
  uint32 sort_length() const;
  virtual uint32 max_data_length() const
  {
    return (uint32) (((ulonglong) 1 << (packlength*8)) -1);
  }
  int reset(void) { bzero(ptr, packlength+sizeof(uchar*)); return 0; }
  void reset_fields() { bzero((uchar*) &value,sizeof(value)); }
  static
  void store_length(uchar *i_ptr, uint i_packlength, uint32 i_number, In_C_you_should_use_my_bool_instead() low_byte_first);
  void store_length(uchar *i_ptr, uint i_packlength, uint32 i_number)
  {
    store_length(i_ptr, i_packlength, i_number, table->s->db_low_byte_first);
  }
  inline void store_length(uint32 number)
  {
    store_length(ptr, packlength, number);
  }
  uint32 get_packed_size(const uchar *ptr_arg, In_C_you_should_use_my_bool_instead() low_byte_first)
    {return packlength + get_length(ptr_arg, packlength, low_byte_first);}
  inline uint32 get_length(uint row_offset= 0)
  { return get_length(ptr+row_offset, this->packlength, table->s->db_low_byte_first); }
  uint32 get_length(const uchar *ptr, uint packlength, In_C_you_should_use_my_bool_instead() low_byte_first);
  uint32 get_length(const uchar *ptr_arg)
  { return get_length(ptr_arg, this->packlength, table->s->db_low_byte_first); }
  void put_length(uchar *pos, uint32 length);
  inline void get_ptr(uchar **str)
    {
      memcpy(((uchar*) str),(ptr+packlength),(sizeof(uchar*)));
    }
  inline void get_ptr(uchar **str, uint row_offset)
    {
      memcpy(((uchar*) str),(ptr+packlength+row_offset),(sizeof(char*)));
    }
  inline void set_ptr(uchar *length, uchar *data)
    {
      memcpy(ptr,length,packlength);
      memcpy((ptr+packlength),(&data),(sizeof(char*)));
    }
  void set_ptr_offset(my_ptrdiff_t ptr_diff, uint32 length, uchar *data)
    {
      uchar *ptr_ofs= (uchar*) ((uchar*) (ptr)+ptr_diff);
      store_length(ptr_ofs, packlength, length);
      memcpy((ptr_ofs+packlength),(&data),(sizeof(char*)));
    }
  inline void set_ptr(uint32 length, uchar *data)
    {
      set_ptr_offset(0, length, data);
    }
  uint get_key_image(uchar *buff,uint length, imagetype type);
  void set_key_image(const uchar *buff,uint length);
  void sql_type(String &str) const;
  inline In_C_you_should_use_my_bool_instead() copy()
  {
    uchar *tmp;
    get_ptr(&tmp);
    if (value.copy((char*) tmp, get_length(), charset()))
    {
      Field_blob::reset();
      return 1;
    }
    tmp=(uchar*) value.ptr();
    memcpy((ptr+packlength),(&tmp),(sizeof(char*)));
    return 0;
  }
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first);
  uchar *pack_key(uchar *to, const uchar *from,
                  uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first);
  uchar *pack_key_from_key_image(uchar* to, const uchar *from,
                                 uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first);
  virtual const uchar *unpack(uchar *to, const uchar *from,
                              uint param_data, In_C_you_should_use_my_bool_instead() low_byte_first);
  const uchar *unpack_key(uchar* to, const uchar *from,
                          uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first);
  int pack_cmp(const uchar *a, const uchar *b, uint key_length,
               my_bool insert_or_update);
  int pack_cmp(const uchar *b, uint key_length,my_bool insert_or_update);
  uint packed_col_length(const uchar *col_ptr, uint length);
  uint max_packed_col_length(uint max_length);
  void free() { value.free(); }
  inline void clear_temporary() { bzero((uchar*) &value,sizeof(value)); }
  friend int field_conv(Field *to,Field *from);
  uint size_of() const { return sizeof(*this); }
  In_C_you_should_use_my_bool_instead() has_charset(void) const
  { return charset() == &my_charset_bin ? (0) : (1); }
  uint32 max_display_length();
  uint is_equal(Create_field *new_field);
  inline In_C_you_should_use_my_bool_instead() in_read_set() { return bitmap_is_set(table->read_set, field_index); }
  inline In_C_you_should_use_my_bool_instead() in_write_set() { return bitmap_is_set(table->write_set, field_index); }
private:
  int do_save_field_metadata(uchar *first_byte);
};
class Field_geom :public Field_blob {
public:
  enum geometry_type geom_type;
  Field_geom(uchar *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
      enum utype unireg_check_arg, const char *field_name_arg,
      TABLE_SHARE *share, uint blob_pack_length,
      enum geometry_type geom_type_arg)
     :Field_blob(ptr_arg, null_ptr_arg, null_bit_arg, unireg_check_arg,
                 field_name_arg, share, blob_pack_length, &my_charset_bin)
  { geom_type= geom_type_arg; }
  Field_geom(uint32 len_arg,In_C_you_should_use_my_bool_instead() maybe_null_arg, const char *field_name_arg,
      TABLE_SHARE *share, enum geometry_type geom_type_arg)
    :Field_blob(len_arg, maybe_null_arg, field_name_arg, &my_charset_bin)
  { geom_type= geom_type_arg; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_VARBINARY2; }
  enum_field_types type() const { return MYSQL_TYPE_GEOMETRY; }
  void sql_type(String &str) const;
  int store(const char *to, uint length, CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int store_decimal(const my_decimal *);
  uint size_of() const { return sizeof(*this); }
  int reset(void) { return !maybe_null() || Field_blob::reset(); }
  geometry_type get_geometry_type() { return geom_type; };
};
class Field_enum :public Field_str {
protected:
  uint packlength;
public:
  TYPELIB *typelib;
  Field_enum(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
             uchar null_bit_arg,
             enum utype unireg_check_arg, const char *field_name_arg,
             uint packlength_arg,
             TYPELIB *typelib_arg,
             CHARSET_INFO *charset_arg)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
        unireg_check_arg, field_name_arg, charset_arg),
    packlength(packlength_arg),typelib(typelib_arg)
  {
      flags|=256;
  }
  Field *new_field(MEM_ROOT *root, struct st_table *new_table, In_C_you_should_use_my_bool_instead() keep_type);
  enum_field_types type() const { return MYSQL_TYPE_STRING; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  enum Item_result cast_to_int_type () const { return INT_RESULT; }
  enum ha_base_keytype key_type() const;
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return (uint32) packlength; }
  void store_type(ulonglong value);
  void sql_type(String &str) const;
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return MYSQL_TYPE_ENUM; }
  uint pack_length_from_metadata(uint field_metadata)
  { return (field_metadata & 0x00ff); }
  uint row_pack_length() { return pack_length(); }
  virtual In_C_you_should_use_my_bool_instead() zero_pack() const { return 0; }
  In_C_you_should_use_my_bool_instead() optimize_range(uint idx, uint part) { return 0; }
  In_C_you_should_use_my_bool_instead() eq_def(Field *field);
  In_C_you_should_use_my_bool_instead() has_charset(void) const { return (1); }
  CHARSET_INFO *sort_charset(void) const { return &my_charset_bin; }
private:
  int do_save_field_metadata(uchar *first_byte);
};
class Field_set :public Field_enum {
public:
  Field_set(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
     uchar null_bit_arg,
     enum utype unireg_check_arg, const char *field_name_arg,
     uint32 packlength_arg,
     TYPELIB *typelib_arg, CHARSET_INFO *charset_arg)
    :Field_enum(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
      unireg_check_arg, field_name_arg,
                packlength_arg,
                typelib_arg,charset_arg)
    {
      flags=(flags & ~256) | 2048;
    }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr) { return Field_set::store((longlong) nr, (0)); }
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  virtual In_C_you_should_use_my_bool_instead() zero_pack() const { return 1; }
  String *val_str(String*,String *);
  void sql_type(String &str) const;
  enum_field_types real_type() const { return MYSQL_TYPE_SET; }
  In_C_you_should_use_my_bool_instead() has_charset(void) const { return (1); }
};
class Field_bit :public Field {
public:
  uchar *bit_ptr;
  uchar bit_ofs;
  uint bit_len;
  uint bytes_in_rec;
  Field_bit(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
            uchar null_bit_arg, uchar *bit_ptr_arg, uchar bit_ofs_arg,
            enum utype unireg_check_arg, const char *field_name_arg);
  enum_field_types type() const { return MYSQL_TYPE_BIT; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_BIT; }
  uint32 key_length() const { return (uint32) (field_length + 7) / 8; }
  uint32 max_data_length() const { return (field_length + 7) / 8; }
  uint32 max_display_length() { return field_length; }
  uint size_of() const { return sizeof(*this); }
  Item_result result_type () const { return INT_RESULT; }
  int reset(void) { bzero(ptr, bytes_in_rec); return 0; }
  int store(const char *to, uint length, CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val);
  int store_decimal(const my_decimal *);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*, String *);
  virtual In_C_you_should_use_my_bool_instead() str_needs_quotes() { return (1); }
  my_decimal *val_decimal(my_decimal *);
  int cmp(const uchar *a, const uchar *b)
  {
    assert(ptr == a);
    return Field_bit::key_cmp(b, bytes_in_rec+((bit_len) ? 1 : 0));
  }
  int cmp_binary_offset(uint row_offset)
  { return cmp_offset(row_offset); }
  int cmp_max(const uchar *a, const uchar *b, uint max_length);
  int key_cmp(const uchar *a, const uchar *b)
  { return cmp_binary((uchar *) a, (uchar *) b); }
  int key_cmp(const uchar *str, uint length);
  int cmp_offset(uint row_offset);
  void get_image(uchar *buff, uint length, CHARSET_INFO *cs)
  { get_key_image(buff, length, itRAW); }
  void set_image(const uchar *buff,uint length, CHARSET_INFO *cs)
  { Field_bit::store((char *) buff, length, cs); }
  uint get_key_image(uchar *buff, uint length, imagetype type);
  void set_key_image(const uchar *buff, uint length)
  { Field_bit::store((char*) buff, length, &my_charset_bin); }
  void sort_string(uchar *buff, uint length)
  { get_key_image(buff, length, itRAW); }
  uint32 pack_length() const { return (uint32) (field_length + 7) / 8; }
  uint32 pack_length_in_rec() const { return bytes_in_rec; }
  uint pack_length_from_metadata(uint field_metadata);
  uint row_pack_length()
  { return (bytes_in_rec + ((bit_len > 0) ? 1 : 0)); }
  int compatible_field_size(uint field_metadata);
  void sql_type(String &str) const;
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, In_C_you_should_use_my_bool_instead() low_byte_first);
  virtual const uchar *unpack(uchar *to, const uchar *from,
                              uint param_data, In_C_you_should_use_my_bool_instead() low_byte_first);
  virtual void set_default();
  Field *new_key_field(MEM_ROOT *root, struct st_table *new_table,
                       uchar *new_ptr, uchar *new_null_ptr,
                       uint new_null_bit);
  void set_bit_ptr(uchar *bit_ptr_arg, uchar bit_ofs_arg)
  {
    bit_ptr= bit_ptr_arg;
    bit_ofs= bit_ofs_arg;
  }
  In_C_you_should_use_my_bool_instead() eq(Field *field)
  {
    return (Field::eq(field) &&
            field->type() == type() &&
            bit_ptr == ((Field_bit *)field)->bit_ptr &&
            bit_ofs == ((Field_bit *)field)->bit_ofs);
  }
  uint is_equal(Create_field *new_field);
  void move_field_offset(my_ptrdiff_t ptr_diff)
  {
    Field::move_field_offset(ptr_diff);
    bit_ptr= (uchar*) ((uchar*) (bit_ptr)+ptr_diff);
  }
  void hash(ulong *nr, ulong *nr2);
private:
  virtual size_t do_last_null_byte() const;
  int do_save_field_metadata(uchar *first_byte);
};
class Field_bit_as_char: public Field_bit {
public:
  Field_bit_as_char(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
                    uchar null_bit_arg,
                    enum utype unireg_check_arg, const char *field_name_arg);
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  uint size_of() const { return sizeof(*this); }
  int store(const char *to, uint length, CHARSET_INFO *charset);
  int store(double nr) { return Field_bit::store(nr); }
  int store(longlong nr, In_C_you_should_use_my_bool_instead() unsigned_val)
  { return Field_bit::store(nr, unsigned_val); }
  void sql_type(String &str) const;
};
class Create_field :public Sql_alloc
{
public:
  const char *field_name;
  const char *change;
  const char *after;
  LEX_STRING comment;
  Item *def;
  enum enum_field_types sql_type;
  ulong length;
  uint32 char_length;
  uint decimals, flags, pack_length, key_length;
  Field::utype unireg_check;
  TYPELIB *interval;
  TYPELIB *save_interval;
  List<String> interval_list;
  CHARSET_INFO *charset;
  Field::geometry_type geom_type;
  Field *field;
  uint8 row,col,sc_length,interval_id;
  uint offset,pack_flag;
  Create_field() :after(0) {}
  Create_field(Field *field, Field *orig_field);
  Create_field *clone(MEM_ROOT *mem_root) const
    { return new (mem_root) Create_field(*this); }
  void create_length_to_internal_length(void);
  void init_for_tmp_table(enum_field_types sql_type_arg,
                          uint32 max_length, uint32 decimals,
                          In_C_you_should_use_my_bool_instead() maybe_null, In_C_you_should_use_my_bool_instead() is_unsigned);
  In_C_you_should_use_my_bool_instead() init(THD *thd, char *field_name, enum_field_types type, char *length,
            char *decimals, uint type_modifier, Item *default_value,
            Item *on_update_value, LEX_STRING *comment, char *change,
            List<String> *interval_list, CHARSET_INFO *cs,
            uint uint_geom_type);
};
class Send_field {
 public:
  const char *db_name;
  const char *table_name,*org_table_name;
  const char *col_name,*org_col_name;
  ulong length;
  uint charsetnr, flags, decimals;
  enum_field_types type;
  Send_field() {}
};
class Copy_field :public Sql_alloc {
  typedef void Copy_func(Copy_field*);
  Copy_func *get_copy_func(Field *to, Field *from);
public:
  uchar *from_ptr,*to_ptr;
  uchar *from_null_ptr,*to_null_ptr;
  my_bool *null_row;
  uint from_bit,to_bit;
  uint from_length,to_length;
  Field *from_field,*to_field;
  String tmp;
  Copy_field() {}
  ~Copy_field() {}
  void set(Field *to,Field *from,In_C_you_should_use_my_bool_instead() save);
  void set(uchar *to,Field *from);
  void (*do_copy)(Copy_field *);
  void (*do_copy2)(Copy_field *);
};
Field *make_field(TABLE_SHARE *share, uchar *ptr, uint32 field_length,
    uchar *null_pos, uchar null_bit,
    uint pack_flag, enum_field_types field_type,
    CHARSET_INFO *cs,
    Field::geometry_type geom_type,
    Field::utype unireg_check,
    TYPELIB *interval, const char *field_name);
uint pack_length_to_packflag(uint type);
enum_field_types get_blob_type_from_length(ulong length);
uint32 calc_pack_length(enum_field_types type,uint32 length);
int set_field_to_null(Field *field);
int set_field_to_null_with_conversions(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
#include "protocol.h"
class i_string;
class THD;
typedef struct st_mysql_field MYSQL_FIELD;
typedef struct st_mysql_rows MYSQL_ROWS;
class Protocol
{
protected:
  THD *thd;
  String *packet;
  String *convert;
  uint field_pos;
  enum enum_field_types *field_types;
  uint field_count;
  In_C_you_should_use_my_bool_instead() net_store_data(const uchar *from, size_t length);
  In_C_you_should_use_my_bool_instead() store_string_aux(const char *from, size_t length,
                        CHARSET_INFO *fromcs, CHARSET_INFO *tocs);
public:
  Protocol() {}
  Protocol(THD *thd_arg) { init(thd_arg); }
  virtual ~Protocol() {}
  void init(THD* thd_arg);
  enum { SEND_NUM_ROWS= 1, SEND_DEFAULTS= 2, SEND_EOF= 4 };
  virtual In_C_you_should_use_my_bool_instead() send_fields(List<Item> *list, uint flags);
  In_C_you_should_use_my_bool_instead() store(I_List<i_string> *str_list);
  In_C_you_should_use_my_bool_instead() store(const char *from, CHARSET_INFO *cs);
  String *storage_packet() { return packet; }
  inline void free() { packet->free(); }
  virtual In_C_you_should_use_my_bool_instead() write();
  inline In_C_you_should_use_my_bool_instead() store(int from)
  { return store_long((longlong) from); }
  inline In_C_you_should_use_my_bool_instead() store(uint32 from)
  { return store_long((longlong) from); }
  inline In_C_you_should_use_my_bool_instead() store(longlong from)
  { return store_longlong((longlong) from, 0); }
  inline In_C_you_should_use_my_bool_instead() store(ulonglong from)
  { return store_longlong((longlong) from, 1); }
  inline In_C_you_should_use_my_bool_instead() store(String *str)
  { return store((char*) str->ptr(), str->length(), str->charset()); }
  virtual In_C_you_should_use_my_bool_instead() prepare_for_send(List<Item> *item_list)
  {
    field_count=item_list->elements;
    return 0;
  }
  virtual In_C_you_should_use_my_bool_instead() flush();
  virtual void end_partial_result_set(THD *thd);
  virtual void prepare_for_resend()=0;
  virtual In_C_you_should_use_my_bool_instead() store_null()=0;
  virtual In_C_you_should_use_my_bool_instead() store_tiny(longlong from)=0;
  virtual In_C_you_should_use_my_bool_instead() store_short(longlong from)=0;
  virtual In_C_you_should_use_my_bool_instead() store_long(longlong from)=0;
  virtual In_C_you_should_use_my_bool_instead() store_longlong(longlong from, In_C_you_should_use_my_bool_instead() unsigned_flag)=0;
  virtual In_C_you_should_use_my_bool_instead() store_decimal(const my_decimal *)=0;
  virtual In_C_you_should_use_my_bool_instead() store(const char *from, size_t length, CHARSET_INFO *cs)=0;
  virtual In_C_you_should_use_my_bool_instead() store(const char *from, size_t length,
         CHARSET_INFO *fromcs, CHARSET_INFO *tocs)=0;
  virtual In_C_you_should_use_my_bool_instead() store(float from, uint32 decimals, String *buffer)=0;
  virtual In_C_you_should_use_my_bool_instead() store(double from, uint32 decimals, String *buffer)=0;
  virtual In_C_you_should_use_my_bool_instead() store(MYSQL_TIME *time)=0;
  virtual In_C_you_should_use_my_bool_instead() store_date(MYSQL_TIME *time)=0;
  virtual In_C_you_should_use_my_bool_instead() store_time(MYSQL_TIME *time)=0;
  virtual In_C_you_should_use_my_bool_instead() store(Field *field)=0;
  void remove_last_row() {}
  enum enum_protocol_type
  {
    PROTOCOL_TEXT= 0, PROTOCOL_BINARY= 1
  };
  virtual enum enum_protocol_type type()= 0;
};
class Protocol_text :public Protocol
{
public:
  Protocol_text() {}
  Protocol_text(THD *thd_arg) :Protocol(thd_arg) {}
  virtual void prepare_for_resend();
  virtual In_C_you_should_use_my_bool_instead() store_null();
  virtual In_C_you_should_use_my_bool_instead() store_tiny(longlong from);
  virtual In_C_you_should_use_my_bool_instead() store_short(longlong from);
  virtual In_C_you_should_use_my_bool_instead() store_long(longlong from);
  virtual In_C_you_should_use_my_bool_instead() store_longlong(longlong from, In_C_you_should_use_my_bool_instead() unsigned_flag);
  virtual In_C_you_should_use_my_bool_instead() store_decimal(const my_decimal *);
  virtual In_C_you_should_use_my_bool_instead() store(const char *from, size_t length, CHARSET_INFO *cs);
  virtual In_C_you_should_use_my_bool_instead() store(const char *from, size_t length,
         CHARSET_INFO *fromcs, CHARSET_INFO *tocs);
  virtual In_C_you_should_use_my_bool_instead() store(MYSQL_TIME *time);
  virtual In_C_you_should_use_my_bool_instead() store_date(MYSQL_TIME *time);
  virtual In_C_you_should_use_my_bool_instead() store_time(MYSQL_TIME *time);
  virtual In_C_you_should_use_my_bool_instead() store(float nr, uint32 decimals, String *buffer);
  virtual In_C_you_should_use_my_bool_instead() store(double from, uint32 decimals, String *buffer);
  virtual In_C_you_should_use_my_bool_instead() store(Field *field);
  virtual enum enum_protocol_type type() { return PROTOCOL_TEXT; };
};
class Protocol_binary :public Protocol
{
private:
  uint bit_fields;
public:
  Protocol_binary() {}
  Protocol_binary(THD *thd_arg) :Protocol(thd_arg) {}
  virtual In_C_you_should_use_my_bool_instead() prepare_for_send(List<Item> *item_list);
  virtual void prepare_for_resend();
  virtual In_C_you_should_use_my_bool_instead() store_null();
  virtual In_C_you_should_use_my_bool_instead() store_tiny(longlong from);
  virtual In_C_you_should_use_my_bool_instead() store_short(longlong from);
  virtual In_C_you_should_use_my_bool_instead() store_long(longlong from);
  virtual In_C_you_should_use_my_bool_instead() store_longlong(longlong from, In_C_you_should_use_my_bool_instead() unsigned_flag);
  virtual In_C_you_should_use_my_bool_instead() store_decimal(const my_decimal *);
  virtual In_C_you_should_use_my_bool_instead() store(const char *from, size_t length, CHARSET_INFO *cs);
  virtual In_C_you_should_use_my_bool_instead() store(const char *from, size_t length,
         CHARSET_INFO *fromcs, CHARSET_INFO *tocs);
  virtual In_C_you_should_use_my_bool_instead() store(MYSQL_TIME *time);
  virtual In_C_you_should_use_my_bool_instead() store_date(MYSQL_TIME *time);
  virtual In_C_you_should_use_my_bool_instead() store_time(MYSQL_TIME *time);
  virtual In_C_you_should_use_my_bool_instead() store(float nr, uint32 decimals, String *buffer);
  virtual In_C_you_should_use_my_bool_instead() store(double from, uint32 decimals, String *buffer);
  virtual In_C_you_should_use_my_bool_instead() store(Field *field);
  virtual enum enum_protocol_type type() { return PROTOCOL_BINARY; };
};
void send_warning(THD *thd, uint sql_errno, const char *err=0);
void net_send_error(THD *thd, uint sql_errno=0, const char *err=0);
void net_end_statement(THD *thd);
In_C_you_should_use_my_bool_instead() send_old_password_request(THD *thd);
uchar *net_store_data(uchar *to,const uchar *from, size_t length);
uchar *net_store_data(uchar *to,int32 from);
uchar *net_store_data(uchar *to,longlong from);
#include "sql_udf.h"
enum Item_udftype {UDFTYPE_FUNCTION=1,UDFTYPE_AGGREGATE};
typedef void (*Udf_func_clear)(UDF_INIT *, uchar *, uchar *);
typedef void (*Udf_func_add)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *);
typedef void (*Udf_func_deinit)(UDF_INIT*);
typedef my_bool (*Udf_func_init)(UDF_INIT *, UDF_ARGS *, char *);
typedef void (*Udf_func_any)();
typedef double (*Udf_func_double)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *);
typedef longlong (*Udf_func_longlong)(UDF_INIT *, UDF_ARGS *, uchar *,
                                      uchar *);
typedef struct st_udf_func
{
  LEX_STRING name;
  Item_result returns;
  Item_udftype type;
  char *dl;
  void *dlhandle;
  Udf_func_any func;
  Udf_func_init func_init;
  Udf_func_deinit func_deinit;
  Udf_func_clear func_clear;
  Udf_func_add func_add;
  ulong usage_count;
} udf_func;
class Item_result_field;
class udf_handler :public Sql_alloc
{
 protected:
  udf_func *u_d;
  String *buffers;
  UDF_ARGS f_args;
  UDF_INIT initid;
  char *num_buffer;
  uchar error, is_null;
  In_C_you_should_use_my_bool_instead() initialized;
  Item **args;
 public:
  table_map used_tables_cache;
  In_C_you_should_use_my_bool_instead() const_item_cache;
  In_C_you_should_use_my_bool_instead() not_original;
  udf_handler(udf_func *udf_arg) :u_d(udf_arg), buffers(0), error(0),
    is_null(0), initialized(0), not_original(0)
  {}
  ~udf_handler();
  const char *name() const { return u_d ? u_d->name.str : "?"; }
  Item_result result_type () const
  { return u_d ? u_d->returns : STRING_RESULT;}
  In_C_you_should_use_my_bool_instead() get_arguments();
  In_C_you_should_use_my_bool_instead() fix_fields(THD *thd, Item_result_field *item,
    uint arg_count, Item **args);
  void cleanup();
  double val(my_bool *null_value)
  {
    is_null= 0;
    if (get_arguments())
    {
      *null_value=1;
      return 0.0;
    }
    Udf_func_double func= (Udf_func_double) u_d->func;
    double tmp=func(&initid, &f_args, &is_null, &error);
    if (is_null || error)
    {
      *null_value=1;
      return 0.0;
    }
    *null_value=0;
    return tmp;
  }
  longlong val_int(my_bool *null_value)
  {
    is_null= 0;
    if (get_arguments())
    {
      *null_value=1;
      return 0LL;
    }
    Udf_func_longlong func= (Udf_func_longlong) u_d->func;
    longlong tmp=func(&initid, &f_args, &is_null, &error);
    if (is_null || error)
    {
      *null_value=1;
      return 0LL;
    }
    *null_value=0;
    return tmp;
  }
  my_decimal *val_decimal(my_bool *null_value, my_decimal *dec_buf);
  void clear()
  {
    is_null= 0;
    Udf_func_clear func= u_d->func_clear;
    func(&initid, &is_null, &error);
  }
  void add(my_bool *null_value)
  {
    if (get_arguments())
    {
      *null_value=1;
      return;
    }
    Udf_func_add func= u_d->func_add;
    func(&initid, &f_args, &is_null, &error);
    *null_value= (my_bool) (is_null || error);
  }
  String *val_str(String *str,String *save_str);
};
void udf_init(void),udf_free(void);
udf_func *find_udf(const char *name, uint len=0,In_C_you_should_use_my_bool_instead() mark_used=0);
void free_udf(udf_func *udf);
int mysql_create_function(THD *thd,udf_func *udf);
int mysql_drop_function(THD *thd,const LEX_STRING *name);
#include "sql_profile.h"
extern ST_FIELD_INFO query_profile_statistics_info[];
int fill_query_profile_statistics_info(THD *thd, TABLE_LIST *tables, Item *cond);
int make_profile_table_for_show(THD *thd, ST_SCHEMA_TABLE *schema_table);
#include "sql_partition.h"
#pragma interface
typedef struct {
  longlong list_value;
  uint32 partition_id;
} LIST_PART_ENTRY;
typedef struct {
  uint32 start_part;
  uint32 end_part;
} part_id_range;
struct st_partition_iter;
In_C_you_should_use_my_bool_instead() is_partition_in_list(char *part_name, List<char> list_part_names);
char *are_partitions_in_table(partition_info *new_part_info,
                              partition_info *old_part_info);
In_C_you_should_use_my_bool_instead() check_reorganise_list(partition_info *new_part_info,
                           partition_info *old_part_info,
                           List<char> list_part_names);
handler *get_ha_partition(partition_info *part_info);
int get_parts_for_update(const uchar *old_data, uchar *new_data,
                         const uchar *rec0, partition_info *part_info,
                         uint32 *old_part_id, uint32 *new_part_id,
                         longlong *func_value);
int get_part_for_delete(const uchar *buf, const uchar *rec0,
                        partition_info *part_info, uint32 *part_id);
void prune_partition_set(const TABLE *table, part_id_range *part_spec);
In_C_you_should_use_my_bool_instead() check_partition_info(partition_info *part_info,handlerton **eng_type,
                          TABLE *table, handler *file, HA_CREATE_INFO *info);
void set_linear_hash_mask(partition_info *part_info, uint no_parts);
In_C_you_should_use_my_bool_instead() fix_partition_func(THD *thd, TABLE *table, In_C_you_should_use_my_bool_instead() create_table_ind);
char *generate_partition_syntax(partition_info *part_info,
                                uint *buf_length, In_C_you_should_use_my_bool_instead() use_sql_alloc,
                                In_C_you_should_use_my_bool_instead() show_partition_options);
In_C_you_should_use_my_bool_instead() partition_key_modified(TABLE *table, const MY_BITMAP *fields);
void get_partition_set(const TABLE *table, uchar *buf, const uint index,
                       const key_range *key_spec,
                       part_id_range *part_spec);
void get_full_part_id_from_key(const TABLE *table, uchar *buf,
                               KEY *key_info,
                               const key_range *key_spec,
                               part_id_range *part_spec);
In_C_you_should_use_my_bool_instead() mysql_unpack_partition(THD *thd, const char *part_buf,
                            uint part_info_len,
                            const char *part_state, uint part_state_len,
                            TABLE *table, In_C_you_should_use_my_bool_instead() is_create_table_ind,
                            handlerton *default_db_type,
                            In_C_you_should_use_my_bool_instead() *work_part_info_used);
void make_used_partitions_str(partition_info *part_info, String *parts_str);
uint32 get_list_array_idx_for_endpoint(partition_info *part_info,
                                       In_C_you_should_use_my_bool_instead() left_endpoint,
                                       In_C_you_should_use_my_bool_instead() include_endpoint);
uint32 get_partition_id_range_for_endpoint(partition_info *part_info,
                                           In_C_you_should_use_my_bool_instead() left_endpoint,
                                           In_C_you_should_use_my_bool_instead() include_endpoint);
In_C_you_should_use_my_bool_instead() fix_fields_part_func(THD *thd, Item* func_expr, TABLE *table,
                          In_C_you_should_use_my_bool_instead() is_sub_part, In_C_you_should_use_my_bool_instead() is_field_to_be_setup);
In_C_you_should_use_my_bool_instead() check_part_func_fields(Field **ptr, In_C_you_should_use_my_bool_instead() ok_with_charsets);
In_C_you_should_use_my_bool_instead() field_is_partition_charset(Field *field);
typedef uint32 (*partition_iter_func)(st_partition_iter* part_iter);
typedef struct st_partition_iter
{
  partition_iter_func get_next;
  In_C_you_should_use_my_bool_instead() ret_null_part, ret_null_part_orig;
  struct st_part_num_range
  {
    uint32 start;
    uint32 cur;
    uint32 end;
  };
  struct st_field_value_range
  {
    longlong start;
    longlong cur;
    longlong end;
  };
  union
  {
    struct st_part_num_range part_nums;
    struct st_field_value_range field_vals;
  };
  partition_info *part_info;
} PARTITION_ITERATOR;
typedef int (*get_partitions_in_range_iter)(partition_info *part_info,
                                            In_C_you_should_use_my_bool_instead() is_subpart,
                                            uchar *min_val, uchar *max_val,
                                            uint flags,
                                            PARTITION_ITERATOR *part_iter);
#include "partition_info.h"
#include "partition_element.h"
enum partition_type {
  NOT_A_PARTITION= 0,
  RANGE_PARTITION,
  HASH_PARTITION,
  LIST_PARTITION
};
enum partition_state {
  PART_NORMAL= 0,
  PART_IS_DROPPED= 1,
  PART_TO_BE_DROPPED= 2,
  PART_TO_BE_ADDED= 3,
  PART_TO_BE_REORGED= 4,
  PART_REORGED_DROPPED= 5,
  PART_CHANGED= 6,
  PART_IS_CHANGED= 7,
  PART_IS_ADDED= 8
};
typedef struct p_elem_val
{
  longlong value;
  In_C_you_should_use_my_bool_instead() null_value;
  In_C_you_should_use_my_bool_instead() unsigned_flag;
} part_elem_value;
struct st_ddl_log_memory_entry;
class partition_element :public Sql_alloc {
public:
  List<partition_element> subpartitions;
  List<part_elem_value> list_val_list;
  ha_rows part_max_rows;
  ha_rows part_min_rows;
  longlong range_value;
  char *partition_name;
  char *tablespace_name;
  struct st_ddl_log_memory_entry *log_entry;
  char* part_comment;
  char* data_file_name;
  char* index_file_name;
  handlerton *engine_type;
  enum partition_state part_state;
  uint16 nodegroup_id;
  In_C_you_should_use_my_bool_instead() has_null_value;
  In_C_you_should_use_my_bool_instead() signed_flag;
  In_C_you_should_use_my_bool_instead() max_value;
  partition_element()
  : part_max_rows(0), part_min_rows(0), range_value(0),
    partition_name(NULL), tablespace_name(NULL),
    log_entry(NULL), part_comment(NULL),
    data_file_name(NULL), index_file_name(NULL),
    engine_type(NULL), part_state(PART_NORMAL),
    nodegroup_id(65535), has_null_value((0)),
    signed_flag((0)), max_value((0))
  {
  }
  partition_element(partition_element *part_elem)
  : part_max_rows(part_elem->part_max_rows),
    part_min_rows(part_elem->part_min_rows),
    range_value(0), partition_name(NULL),
    tablespace_name(part_elem->tablespace_name),
    part_comment(part_elem->part_comment),
    data_file_name(part_elem->data_file_name),
    index_file_name(part_elem->index_file_name),
    engine_type(part_elem->engine_type),
    part_state(part_elem->part_state),
    nodegroup_id(part_elem->nodegroup_id),
    has_null_value((0))
  {
  }
  ~partition_element() {}
};
class partition_info;
typedef int (*get_part_id_func)(partition_info *part_info,
                                 uint32 *part_id,
                                 longlong *func_value);
typedef uint32 (*get_subpart_id_func)(partition_info *part_info);
struct st_ddl_log_memory_entry;
class partition_info : public Sql_alloc
{
public:
  List<partition_element> partitions;
  List<partition_element> temp_partitions;
  List<char> part_field_list;
  List<char> subpart_field_list;
  get_part_id_func get_partition_id;
  get_part_id_func get_part_partition_id;
  get_subpart_id_func get_subpartition_id;
  get_part_id_func get_partition_id_charset;
  get_part_id_func get_part_partition_id_charset;
  get_subpart_id_func get_subpartition_id_charset;
  Field **part_field_array;
  Field **subpart_field_array;
  Field **part_charset_field_array;
  Field **subpart_charset_field_array;
  Field **full_part_field_array;
  Field **full_part_charset_field_array;
  MY_BITMAP full_part_field_set;
  uchar **part_field_buffers;
  uchar **subpart_field_buffers;
  uchar **full_part_field_buffers;
  uchar **restore_part_field_ptrs;
  uchar **restore_subpart_field_ptrs;
  uchar **restore_full_part_field_ptrs;
  Item *part_expr;
  Item *subpart_expr;
  Item *item_free_list;
  struct st_ddl_log_memory_entry *first_log_entry;
  struct st_ddl_log_memory_entry *exec_log_entry;
  struct st_ddl_log_memory_entry *frm_log_entry;
  MY_BITMAP used_partitions;
  union {
    longlong *range_int_array;
    LIST_PART_ENTRY *list_array;
  };
  get_partitions_in_range_iter get_part_iter_for_interval;
  get_partitions_in_range_iter get_subpart_iter_for_interval;
  longlong err_value;
  char* part_info_string;
  char *part_func_string;
  char *subpart_func_string;
  const char *part_state;
  partition_element *curr_part_elem;
  partition_element *current_partition;
  key_map all_fields_in_PF, all_fields_in_PPF, all_fields_in_SPF;
  key_map some_fields_in_PF;
  handlerton *default_engine_type;
  Item_result part_result_type;
  partition_type part_type;
  partition_type subpart_type;
  uint part_info_len;
  uint part_state_len;
  uint part_func_len;
  uint subpart_func_len;
  uint no_parts;
  uint no_subparts;
  uint count_curr_subparts;
  uint part_error_code;
  uint no_list_values;
  uint no_part_fields;
  uint no_subpart_fields;
  uint no_full_part_fields;
  uint has_null_part_id;
  uint16 linear_hash_mask;
  In_C_you_should_use_my_bool_instead() use_default_partitions;
  In_C_you_should_use_my_bool_instead() use_default_no_partitions;
  In_C_you_should_use_my_bool_instead() use_default_subpartitions;
  In_C_you_should_use_my_bool_instead() use_default_no_subpartitions;
  In_C_you_should_use_my_bool_instead() default_partitions_setup;
  In_C_you_should_use_my_bool_instead() defined_max_value;
  In_C_you_should_use_my_bool_instead() list_of_part_fields;
  In_C_you_should_use_my_bool_instead() list_of_subpart_fields;
  In_C_you_should_use_my_bool_instead() linear_hash_ind;
  In_C_you_should_use_my_bool_instead() fixed;
  In_C_you_should_use_my_bool_instead() is_auto_partitioned;
  In_C_you_should_use_my_bool_instead() from_openfrm;
  In_C_you_should_use_my_bool_instead() has_null_value;
  partition_info()
  : get_partition_id(NULL), get_part_partition_id(NULL),
    get_subpartition_id(NULL),
    part_field_array(NULL), subpart_field_array(NULL),
    part_charset_field_array(NULL),
    subpart_charset_field_array(NULL),
    full_part_field_array(NULL),
    full_part_charset_field_array(NULL),
    part_field_buffers(NULL), subpart_field_buffers(NULL),
    full_part_field_buffers(NULL),
    restore_part_field_ptrs(NULL), restore_subpart_field_ptrs(NULL),
    restore_full_part_field_ptrs(NULL),
    part_expr(NULL), subpart_expr(NULL), item_free_list(NULL),
    first_log_entry(NULL), exec_log_entry(NULL), frm_log_entry(NULL),
    list_array(NULL), err_value(0),
    part_info_string(NULL),
    part_func_string(NULL), subpart_func_string(NULL),
    part_state(NULL),
    curr_part_elem(NULL), current_partition(NULL),
    default_engine_type(NULL),
    part_result_type(INT_RESULT),
    part_type(NOT_A_PARTITION), subpart_type(NOT_A_PARTITION),
    part_info_len(0), part_state_len(0),
    part_func_len(0), subpart_func_len(0),
    no_parts(0), no_subparts(0),
    count_curr_subparts(0), part_error_code(0),
    no_list_values(0), no_part_fields(0), no_subpart_fields(0),
    no_full_part_fields(0), has_null_part_id(0), linear_hash_mask(0),
    use_default_partitions((1)), use_default_no_partitions((1)),
    use_default_subpartitions((1)), use_default_no_subpartitions((1)),
    default_partitions_setup((0)), defined_max_value((0)),
    list_of_part_fields((0)), list_of_subpart_fields((0)),
    linear_hash_ind((0)), fixed((0)),
    is_auto_partitioned((0)), from_openfrm((0)),
    has_null_value((0))
  {
    all_fields_in_PF.clear_all();
    all_fields_in_PPF.clear_all();
    all_fields_in_SPF.clear_all();
    some_fields_in_PF.clear_all();
    partitions.empty();
    temp_partitions.empty();
    part_field_list.empty();
    subpart_field_list.empty();
  }
  ~partition_info() {}
  partition_info *get_clone();
  In_C_you_should_use_my_bool_instead() is_sub_partitioned()
  {
    return (subpart_type == NOT_A_PARTITION ? (0) : (1));
  }
  uint get_tot_partitions()
  {
    return no_parts * (is_sub_partitioned() ? no_subparts : 1);
  }
  In_C_you_should_use_my_bool_instead() set_up_defaults_for_partitioning(handler *file, HA_CREATE_INFO *info,
                                        uint start_no);
  char *has_unique_names();
  In_C_you_should_use_my_bool_instead() check_engine_mix(handlerton *engine_type, In_C_you_should_use_my_bool_instead() default_engine);
  In_C_you_should_use_my_bool_instead() check_range_constants();
  In_C_you_should_use_my_bool_instead() check_list_constants();
  In_C_you_should_use_my_bool_instead() check_partition_info(THD *thd, handlerton **eng_type,
                            handler *file, HA_CREATE_INFO *info,
                            In_C_you_should_use_my_bool_instead() check_partition_function);
  void print_no_partition_found(TABLE *table);
  In_C_you_should_use_my_bool_instead() set_up_charset_field_preps();
private:
  static int list_part_cmp(const void* a, const void* b);
  static int list_part_cmp_unsigned(const void* a, const void* b);
  In_C_you_should_use_my_bool_instead() set_up_default_partitions(handler *file, HA_CREATE_INFO *info,
                                 uint start_no);
  In_C_you_should_use_my_bool_instead() set_up_default_subpartitions(handler *file, HA_CREATE_INFO *info);
  char *create_default_partition_names(uint part_no, uint no_parts,
                                       uint start_no);
  char *create_subpartition_name(uint subpart_no, const char *part_name);
  In_C_you_should_use_my_bool_instead() has_unique_name(partition_element *element);
};
uint32 get_next_partition_id_range(struct st_partition_iter* part_iter);
In_C_you_should_use_my_bool_instead() check_partition_dirs(partition_info *part_info);
static inline void init_single_partition_iterator(uint32 part_id,
                                           PARTITION_ITERATOR *part_iter)
{
  part_iter->part_nums.start= part_iter->part_nums.cur= part_id;
  part_iter->part_nums.end= part_id+1;
  part_iter->get_next= get_next_partition_id_range;
}
static inline
void init_all_partitions_iterator(partition_info *part_info,
                                  PARTITION_ITERATOR *part_iter)
{
  part_iter->part_nums.start= part_iter->part_nums.cur= 0;
  part_iter->part_nums.end= part_info->no_parts;
  part_iter->get_next= get_next_partition_id_range;
}
class user_var_entry;
class Security_context;
enum enum_var_type
{
  OPT_DEFAULT= 0, OPT_SESSION, OPT_GLOBAL
};
class sys_var;
#include "item.h"
class Protocol;
struct TABLE_LIST;
void item_init(void);
class Item_field;
class DTCollation {
public:
  CHARSET_INFO *collation;
  enum Derivation derivation;
  uint repertoire;
  void set_repertoire_from_charset(CHARSET_INFO *cs)
  {
    repertoire= cs->state & 4096 ?
                1 : 3;
  }
  DTCollation()
  {
    collation= &my_charset_bin;
    derivation= DERIVATION_NONE;
    repertoire= 3;
  }
  DTCollation(CHARSET_INFO *collation_arg, Derivation derivation_arg)
  {
    collation= collation_arg;
    derivation= derivation_arg;
    set_repertoire_from_charset(collation_arg);
  }
  void set(DTCollation &dt)
  {
    collation= dt.collation;
    derivation= dt.derivation;
    repertoire= dt.repertoire;
  }
  void set(CHARSET_INFO *collation_arg, Derivation derivation_arg)
  {
    collation= collation_arg;
    derivation= derivation_arg;
    set_repertoire_from_charset(collation_arg);
  }
  void set(CHARSET_INFO *collation_arg,
           Derivation derivation_arg,
           uint repertoire_arg)
  {
    collation= collation_arg;
    derivation= derivation_arg;
    repertoire= repertoire_arg;
  }
  void set(CHARSET_INFO *collation_arg)
  {
    collation= collation_arg;
    set_repertoire_from_charset(collation_arg);
  }
  void set(Derivation derivation_arg)
  { derivation= derivation_arg; }
  In_C_you_should_use_my_bool_instead() aggregate(DTCollation &dt, uint flags= 0);
  In_C_you_should_use_my_bool_instead() set(DTCollation &dt1, DTCollation &dt2, uint flags= 0)
  { set(dt1); return aggregate(dt2, flags); }
  const char *derivation_name() const
  {
    switch(derivation)
    {
      case DERIVATION_IGNORABLE: return "IGNORABLE";
      case DERIVATION_COERCIBLE: return "COERCIBLE";
      case DERIVATION_IMPLICIT: return "IMPLICIT";
      case DERIVATION_SYSCONST: return "SYSCONST";
      case DERIVATION_EXPLICIT: return "EXPLICIT";
      case DERIVATION_NONE: return "NONE";
      default: return "UNKNOWN";
    }
  }
};
struct Hybrid_type_traits;
struct Hybrid_type
{
  longlong integer;
  double real;
  my_decimal dec_buf[3];
  int used_dec_buf_no;
  const Hybrid_type_traits *traits;
  Hybrid_type() {}
  Hybrid_type(const Hybrid_type &rhs) :traits(rhs.traits) {}
};
struct Hybrid_type_traits
{
  virtual Item_result type() const { return REAL_RESULT; }
  virtual void
  fix_length_and_dec(Item *item, Item *arg) const;
  virtual void set_zero(Hybrid_type *val) const { val->real= 0.0; }
  virtual void add(Hybrid_type *val, Field *f) const
  { val->real+= f->val_real(); }
  virtual void div(Hybrid_type *val, ulonglong u) const
  { val->real/= ((double) (ulonglong) (u)); }
  virtual longlong val_int(Hybrid_type *val, In_C_you_should_use_my_bool_instead() unsigned_flag) const
  { return (longlong) rint(val->real); }
  virtual double val_real(Hybrid_type *val) const { return val->real; }
  virtual my_decimal *val_decimal(Hybrid_type *val, my_decimal *buf) const;
  virtual String *val_str(Hybrid_type *val, String *buf, uint8 decimals) const;
  static const Hybrid_type_traits *instance();
  Hybrid_type_traits() {}
  virtual ~Hybrid_type_traits() {}
};
struct Hybrid_type_traits_decimal: public Hybrid_type_traits
{
  virtual Item_result type() const { return DECIMAL_RESULT; }
  virtual void
  fix_length_and_dec(Item *arg, Item *item) const;
  virtual void set_zero(Hybrid_type *val) const;
  virtual void add(Hybrid_type *val, Field *f) const;
  virtual void div(Hybrid_type *val, ulonglong u) const;
  virtual longlong val_int(Hybrid_type *val, In_C_you_should_use_my_bool_instead() unsigned_flag) const;
  virtual double val_real(Hybrid_type *val) const;
  virtual my_decimal *val_decimal(Hybrid_type *val, my_decimal *buf) const
  { return &val->dec_buf[val->used_dec_buf_no]; }
  virtual String *val_str(Hybrid_type *val, String *buf, uint8 decimals) const;
  static const Hybrid_type_traits_decimal *instance();
  Hybrid_type_traits_decimal() {};
};
struct Hybrid_type_traits_integer: public Hybrid_type_traits
{
  virtual Item_result type() const { return INT_RESULT; }
  virtual void
  fix_length_and_dec(Item *arg, Item *item) const;
  virtual void set_zero(Hybrid_type *val) const
  { val->integer= 0; }
  virtual void add(Hybrid_type *val, Field *f) const
  { val->integer+= f->val_int(); }
  virtual void div(Hybrid_type *val, ulonglong u) const
  { val->integer/= (longlong) u; }
  virtual longlong val_int(Hybrid_type *val, In_C_you_should_use_my_bool_instead() unsigned_flag) const
  { return val->integer; }
  virtual double val_real(Hybrid_type *val) const
  { return (double) val->integer; }
  virtual my_decimal *val_decimal(Hybrid_type *val, my_decimal *buf) const
  {
    int2my_decimal(30, val->integer, 0, &val->dec_buf[2]);
    return &val->dec_buf[2];
  }
  virtual String *val_str(Hybrid_type *val, String *buf, uint8 decimals) const
  { buf->set(val->integer, &my_charset_bin); return buf;}
  static const Hybrid_type_traits_integer *instance();
  Hybrid_type_traits_integer() {};
};
void dummy_error_processor(THD *thd, void *data);
void view_error_processor(THD *thd, void *data);
struct Name_resolution_context: Sql_alloc
{
  Name_resolution_context *outer_context;
  TABLE_LIST *table_list;
  TABLE_LIST *first_name_resolution_table;
  TABLE_LIST *last_name_resolution_table;
  st_select_lex *select_lex;
  void (*error_processor)(THD *, void *);
  void *error_processor_data;
  In_C_you_should_use_my_bool_instead() resolve_in_select_list;
  Security_context *security_ctx;
  Name_resolution_context()
    :outer_context(0), table_list(0), select_lex(0),
    error_processor_data(0),
    security_ctx(0)
    {}
  void init()
  {
    resolve_in_select_list= (0);
    error_processor= &dummy_error_processor;
    first_name_resolution_table= NULL;
    last_name_resolution_table= NULL;
  }
  void resolve_in_table_list_only(TABLE_LIST *tables)
  {
    table_list= first_name_resolution_table= tables;
    resolve_in_select_list= (0);
  }
  void process_error(THD *thd)
  {
    (*error_processor)(thd, error_processor_data);
  }
};
class Name_resolution_context_state
{
private:
  TABLE_LIST *save_table_list;
  TABLE_LIST *save_first_name_resolution_table;
  TABLE_LIST *save_next_name_resolution_table;
  In_C_you_should_use_my_bool_instead() save_resolve_in_select_list;
  TABLE_LIST *save_next_local;
public:
  Name_resolution_context_state() {}
public:
  void save_state(Name_resolution_context *context, TABLE_LIST *table_list)
  {
    save_table_list= context->table_list;
    save_first_name_resolution_table= context->first_name_resolution_table;
    save_resolve_in_select_list= context->resolve_in_select_list;
    save_next_local= table_list->next_local;
    save_next_name_resolution_table= table_list->next_name_resolution_table;
  }
  void restore_state(Name_resolution_context *context, TABLE_LIST *table_list)
  {
    table_list->next_local= save_next_local;
    table_list->next_name_resolution_table= save_next_name_resolution_table;
    context->table_list= save_table_list;
    context->first_name_resolution_table= save_first_name_resolution_table;
    context->resolve_in_select_list= save_resolve_in_select_list;
  }
  TABLE_LIST *get_first_name_resolution_table()
  {
    return save_first_name_resolution_table;
  }
};
typedef enum monotonicity_info
{
   NON_MONOTONIC,
   MONOTONIC_INCREASING,
   MONOTONIC_STRICT_INCREASING
} enum_monotonicity_info;
class sp_rcontext;
class Settable_routine_parameter
{
public:
  Settable_routine_parameter() {}
  virtual ~Settable_routine_parameter() {}
  virtual void set_required_privilege(In_C_you_should_use_my_bool_instead() rw) {};
  virtual In_C_you_should_use_my_bool_instead() set_value(THD *thd, sp_rcontext *ctx, Item **it)= 0;
};
typedef In_C_you_should_use_my_bool_instead() (Item::*Item_processor) (uchar *arg);
typedef In_C_you_should_use_my_bool_instead() (Item::*Item_analyzer) (uchar **argp);
typedef Item* (Item::*Item_transformer) (uchar *arg);
typedef void (*Cond_traverser) (const Item *item, void *arg);
class Item {
  Item(const Item &);
  void operator=(Item &);
public:
  static void *operator new(size_t size)
  { return sql_alloc(size); }
  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return alloc_root(mem_root, size); }
  static void operator delete(void *ptr,size_t size) { ; }
  static void operator delete(void *ptr, MEM_ROOT *mem_root) {}
  enum Type {FIELD_ITEM= 0, FUNC_ITEM, SUM_FUNC_ITEM, STRING_ITEM,
      INT_ITEM, REAL_ITEM, NULL_ITEM, VARBIN_ITEM,
      COPY_STR_ITEM, FIELD_AVG_ITEM, DEFAULT_VALUE_ITEM,
      PROC_ITEM,COND_ITEM, REF_ITEM, FIELD_STD_ITEM,
      FIELD_VARIANCE_ITEM, INSERT_VALUE_ITEM,
             SUBSELECT_ITEM, ROW_ITEM, CACHE_ITEM, TYPE_HOLDER,
             PARAM_ITEM, TRIGGER_FIELD_ITEM, DECIMAL_ITEM,
             XPATH_NODESET, XPATH_NODESET_CMP,
             VIEW_FIXER_ITEM};
  enum cond_result { COND_UNDEF,COND_OK,COND_TRUE,COND_FALSE };
  enum traverse_order { POSTFIX, PREFIX };
  uint rsize;
  String str_value;
  char * name;
  char * orig_name;
  Item *next;
  uint32 max_length;
  uint name_length;
  int8 marker;
  uint8 decimals;
  my_bool maybe_null;
  my_bool null_value;
  my_bool unsigned_flag;
  my_bool with_sum_func;
  my_bool fixed;
  my_bool is_autogenerated_name;
  DTCollation collation;
  my_bool with_subselect;
  Item_result cmp_context;
  Item();
  Item(THD *thd, Item *item);
  virtual ~Item()
  {
  }
  void set_name(const char *str, uint length, CHARSET_INFO *cs);
  void rename(char *new_name);
  void init_make_field(Send_field *tmp_field,enum enum_field_types type);
  virtual void cleanup();
  virtual void make_field(Send_field *field);
  Field *make_string_field(TABLE *table);
  virtual In_C_you_should_use_my_bool_instead() fix_fields(THD *, Item **);
  inline void quick_fix_field() { fixed= 1; }
  int save_in_field_no_warnings(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
  virtual int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
  virtual void save_org_in_field(Field *field)
  { (void) save_in_field(field, 1); }
  virtual int save_safe_in_field(Field *field)
  { return save_in_field(field, 1); }
  virtual In_C_you_should_use_my_bool_instead() send(Protocol *protocol, String *str);
  virtual In_C_you_should_use_my_bool_instead() eq(const Item *, In_C_you_should_use_my_bool_instead() binary_cmp) const;
  virtual Item_result result_type() const { return REAL_RESULT; }
  virtual Item_result cast_to_int_type() const { return result_type(); }
  virtual enum_field_types string_field_type() const;
  virtual enum_field_types field_type() const;
  virtual enum Type type() const =0;
  virtual enum_monotonicity_info get_monotonicity_info() const
  { return NON_MONOTONIC; }
  virtual longlong val_int_endpoint(In_C_you_should_use_my_bool_instead() left_endp, In_C_you_should_use_my_bool_instead() *incl_endp)
  { assert(0); return 0; }
  virtual double val_real()=0;
  virtual longlong val_int()=0;
  inline ulonglong val_uint() { return (ulonglong) val_int(); }
  virtual String *val_str(String *str)=0;
  virtual my_decimal *val_decimal(my_decimal *decimal_buffer)= 0;
  virtual In_C_you_should_use_my_bool_instead() val_bool();
  virtual String *val_nodeset(String*) { return 0; }
  String *val_string_from_real(String *str);
  String *val_string_from_int(String *str);
  String *val_string_from_decimal(String *str);
  my_decimal *val_decimal_from_real(my_decimal *decimal_value);
  my_decimal *val_decimal_from_int(my_decimal *decimal_value);
  my_decimal *val_decimal_from_string(my_decimal *decimal_value);
  my_decimal *val_decimal_from_date(my_decimal *decimal_value);
  my_decimal *val_decimal_from_time(my_decimal *decimal_value);
  longlong val_int_from_decimal();
  double val_real_from_decimal();
  int save_time_in_field(Field *field);
  int save_date_in_field(Field *field);
  int save_str_value_in_field(Field *field, String *result);
  virtual Field *get_tmp_table_field() { return 0; }
  virtual Field *tmp_table_field(TABLE *t_arg) { return 0; }
  virtual const char *full_name() const { return name ? name : "???"; }
  virtual double val_result() { return val_real(); }
  virtual longlong val_int_result() { return val_int(); }
  virtual String *str_result(String* tmp) { return val_str(tmp); }
  virtual my_decimal *val_decimal_result(my_decimal *val)
  { return val_decimal(val); }
  virtual In_C_you_should_use_my_bool_instead() val_bool_result() { return val_bool(); }
  virtual table_map used_tables() const { return (table_map) 0L; }
  virtual table_map not_null_tables() const { return used_tables(); }
  virtual In_C_you_should_use_my_bool_instead() basic_const_item() const { return 0; }
  virtual Item *clone_item() { return 0; }
  virtual cond_result eq_cmp_result() const { return COND_OK; }
  inline uint float_length(uint decimals_par) const
  { return decimals != 31 ? (DBL_DIG+2+decimals_par) : DBL_DIG+8;}
  virtual uint decimal_precision() const;
  inline int decimal_int_part() const
  { return my_decimal_int_part(decimal_precision(), decimals); }
  virtual In_C_you_should_use_my_bool_instead() const_item() const { return used_tables() == 0; }
  virtual In_C_you_should_use_my_bool_instead() const_during_execution() const
  { return (used_tables() & ~(((table_map) 1) << (sizeof(table_map)*8-3))) == 0; }
  virtual inline void print(String *str, enum_query_type query_type)
  {
    str->append(full_name());
  }
  void print_item_w_name(String *, enum_query_type query_type);
  virtual void update_used_tables() {}
  virtual void split_sum_func(THD *thd, Item **ref_pointer_array,
                              List<Item> &fields) {}
  void split_sum_func2(THD *thd, Item **ref_pointer_array, List<Item> &fields,
                       Item **ref, In_C_you_should_use_my_bool_instead() skip_registered);
  virtual In_C_you_should_use_my_bool_instead() get_date(MYSQL_TIME *ltime,uint fuzzydate);
  virtual In_C_you_should_use_my_bool_instead() get_time(MYSQL_TIME *ltime);
  virtual In_C_you_should_use_my_bool_instead() get_date_result(MYSQL_TIME *ltime,uint fuzzydate)
  { return get_date(ltime,fuzzydate); }
  virtual In_C_you_should_use_my_bool_instead() is_null() { return 0; }
  virtual void update_null_value () { (void) val_int(); }
  virtual void top_level_item() {}
  virtual void set_result_field(Field *field) {}
  virtual In_C_you_should_use_my_bool_instead() is_result_field() { return 0; }
  virtual In_C_you_should_use_my_bool_instead() is_bool_func() { return 0; }
  virtual void save_in_result_field(In_C_you_should_use_my_bool_instead() no_conversions) {}
  virtual void no_rows_in_result() {}
  virtual Item *copy_or_same(THD *thd) { return this; }
  virtual Item *copy_andor_structure(THD *thd) { return this; }
  virtual Item *real_item() { return this; }
  virtual Item *get_tmp_table_item(THD *thd) { return copy_or_same(thd); }
  static CHARSET_INFO *default_charset();
  virtual CHARSET_INFO *compare_collation() { return NULL; }
  virtual In_C_you_should_use_my_bool_instead() walk(Item_processor processor, In_C_you_should_use_my_bool_instead() walk_subquery, uchar *arg)
  {
    return (this->*processor)(arg);
  }
  virtual Item* transform(Item_transformer transformer, uchar *arg);
  virtual Item* compile(Item_analyzer analyzer, uchar **arg_p,
                        Item_transformer transformer, uchar *arg_t)
  {
    if ((this->*analyzer) (arg_p))
      return ((this->*transformer) (arg_t));
    return 0;
  }
   virtual void traverse_cond(Cond_traverser traverser,
                              void *arg, traverse_order order)
   {
     (*traverser)(this, arg);
   }
  virtual In_C_you_should_use_my_bool_instead() remove_dependence_processor(uchar * arg) { return 0; }
  virtual In_C_you_should_use_my_bool_instead() remove_fixed(uchar * arg) { fixed= 0; return 0; }
  virtual In_C_you_should_use_my_bool_instead() cleanup_processor(uchar *arg);
  virtual In_C_you_should_use_my_bool_instead() collect_item_field_processor(uchar * arg) { return 0; }
  virtual In_C_you_should_use_my_bool_instead() find_item_in_field_list_processor(uchar *arg) { return 0; }
  virtual In_C_you_should_use_my_bool_instead() change_context_processor(uchar *context) { return 0; }
  virtual In_C_you_should_use_my_bool_instead() reset_query_id_processor(uchar *query_id_arg) { return 0; }
  virtual In_C_you_should_use_my_bool_instead() is_expensive_processor(uchar *arg) { return 0; }
  virtual In_C_you_should_use_my_bool_instead() register_field_in_read_map(uchar *arg) { return 0; }
  virtual In_C_you_should_use_my_bool_instead() check_partition_func_processor(uchar *bool_arg) { return (1);}
  virtual In_C_you_should_use_my_bool_instead() subst_argument_checker(uchar **arg)
  {
    if (*arg)
      *arg= NULL;
    return (1);
  }
  virtual Item *equal_fields_propagator(uchar * arg) { return this; }
  virtual In_C_you_should_use_my_bool_instead() set_no_const_sub(uchar *arg) { return (0); }
  virtual Item *replace_equal_field(uchar * arg) { return this; }
  virtual Item *this_item() { return this; }
  virtual const Item *this_item() const { return this; }
  virtual Item **this_item_addr(THD *thd, Item **addr_arg) { return addr_arg; }
  virtual uint cols() { return 1; }
  virtual Item* element_index(uint i) { return this; }
  virtual Item** addr(uint i) { return 0; }
  virtual In_C_you_should_use_my_bool_instead() check_cols(uint c);
  virtual In_C_you_should_use_my_bool_instead() null_inside() { return 0; }
  virtual void bring_value() {}
  Field *tmp_table_field_from_field_type(TABLE *table, In_C_you_should_use_my_bool_instead() fixed_length);
  virtual Item_field *filed_for_view_update() { return 0; }
  virtual Item *neg_transformer(THD *thd) { return NULL; }
  virtual Item *update_value_transformer(uchar *select_arg) { return this; }
  virtual Item *safe_charset_converter(CHARSET_INFO *tocs);
  void delete_self()
  {
    cleanup();
    delete this;
  }
  virtual In_C_you_should_use_my_bool_instead() is_splocal() { return 0; }
  virtual Settable_routine_parameter *get_settable_routine_parameter()
  {
    return 0;
  }
  virtual In_C_you_should_use_my_bool_instead() result_as_longlong() { return (0); }
  In_C_you_should_use_my_bool_instead() is_datetime();
  virtual Field::geometry_type get_geometry_type() const
    { return Field::GEOM_GEOMETRY; };
  String *check_well_formed_result(String *str, In_C_you_should_use_my_bool_instead() send_error= 0);
  In_C_you_should_use_my_bool_instead() eq_by_collation(Item *item, In_C_you_should_use_my_bool_instead() binary_cmp, CHARSET_INFO *cs);
};
class sp_head;
class Item_basic_constant :public Item
{
public:
  void cleanup()
  {
    if (orig_name)
      name= orig_name;
  }
};
class Item_sp_variable :public Item
{
protected:
  THD *m_thd;
public:
  LEX_STRING m_name;
public:
  sp_head *m_sp;
public:
  Item_sp_variable(char *sp_var_name_str, uint sp_var_name_length);
public:
  In_C_you_should_use_my_bool_instead() fix_fields(THD *thd, Item **);
  double val_real();
  longlong val_int();
  String *val_str(String *sp);
  my_decimal *val_decimal(my_decimal *decimal_value);
  In_C_you_should_use_my_bool_instead() is_null();
public:
  inline void make_field(Send_field *field);
  inline In_C_you_should_use_my_bool_instead() const_item() const;
  inline int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
  inline In_C_you_should_use_my_bool_instead() send(Protocol *protocol, String *str);
};
inline void Item_sp_variable::make_field(Send_field *field)
{
  Item *it= this_item();
  if (name)
    it->set_name(name, (uint) strlen(name), system_charset_info);
  else
    it->set_name(m_name.str, m_name.length, system_charset_info);
  it->make_field(field);
}
inline In_C_you_should_use_my_bool_instead() Item_sp_variable::const_item() const
{
  return (1);
}
inline int Item_sp_variable::save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions)
{
  return this_item()->save_in_field(field, no_conversions);
}
inline In_C_you_should_use_my_bool_instead() Item_sp_variable::send(Protocol *protocol, String *str)
{
  return this_item()->send(protocol, str);
}
class Item_splocal :public Item_sp_variable,
                    private Settable_routine_parameter
{
  uint m_var_idx;
  Type m_type;
  Item_result m_result_type;
  enum_field_types m_field_type;
public:
  uint pos_in_query;
  uint len_in_query;
  Item_splocal(const LEX_STRING &sp_var_name, uint sp_var_idx,
               enum_field_types sp_var_type,
               uint pos_in_q= 0, uint len_in_q= 0);
  In_C_you_should_use_my_bool_instead() is_splocal() { return 1; }
  Item *this_item();
  const Item *this_item() const;
  Item **this_item_addr(THD *thd, Item **);
  virtual void print(String *str, enum_query_type query_type);
public:
  inline const LEX_STRING *my_name() const;
  inline uint get_var_idx() const;
  inline enum Type type() const;
  inline Item_result result_type() const;
  inline enum_field_types field_type() const { return m_field_type; }
private:
  In_C_you_should_use_my_bool_instead() set_value(THD *thd, sp_rcontext *ctx, Item **it);
public:
  Settable_routine_parameter *get_settable_routine_parameter()
  {
    return this;
  }
};
inline const LEX_STRING *Item_splocal::my_name() const
{
  return &m_name;
}
inline uint Item_splocal::get_var_idx() const
{
  return m_var_idx;
}
inline enum Item::Type Item_splocal::type() const
{
  return m_type;
}
inline Item_result Item_splocal::result_type() const
{
  return m_result_type;
}
class Item_case_expr :public Item_sp_variable
{
public:
  Item_case_expr(uint case_expr_id);
public:
  Item *this_item();
  const Item *this_item() const;
  Item **this_item_addr(THD *thd, Item **);
  inline enum Type type() const;
  inline Item_result result_type() const;
public:
  virtual void print(String *str, enum_query_type query_type);
private:
  uint m_case_expr_id;
};
inline enum Item::Type Item_case_expr::type() const
{
  return this_item()->type();
}
inline Item_result Item_case_expr::result_type() const
{
  return this_item()->result_type();
}
class Item_name_const : public Item
{
  Item *value_item;
  Item *name_item;
  In_C_you_should_use_my_bool_instead() valid_args;
public:
  Item_name_const(Item *name_arg, Item *val);
  In_C_you_should_use_my_bool_instead() fix_fields(THD *, Item **);
  enum Type type() const;
  double val_real();
  longlong val_int();
  String *val_str(String *sp);
  my_decimal *val_decimal(my_decimal *);
  In_C_you_should_use_my_bool_instead() is_null();
  virtual void print(String *str, enum_query_type query_type);
  Item_result result_type() const
  {
    return value_item->result_type();
  }
  In_C_you_should_use_my_bool_instead() const_item() const
  {
    return (1);
  }
  int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions)
  {
    return value_item->save_in_field(field, no_conversions);
  }
  In_C_you_should_use_my_bool_instead() send(Protocol *protocol, String *str)
  {
    return value_item->send(protocol, str);
  }
};
In_C_you_should_use_my_bool_instead() agg_item_collations(DTCollation &c, const char *name,
                         Item **items, uint nitems, uint flags, int item_sep);
In_C_you_should_use_my_bool_instead() agg_item_collations_for_comparison(DTCollation &c, const char *name,
                                        Item **items, uint nitems, uint flags);
In_C_you_should_use_my_bool_instead() agg_item_charsets(DTCollation &c, const char *name,
                       Item **items, uint nitems, uint flags, int item_sep);
class Item_num: public Item_basic_constant
{
public:
  Item_num() {}
  virtual Item_num *neg()= 0;
  Item *safe_charset_converter(CHARSET_INFO *tocs);
  In_C_you_should_use_my_bool_instead() check_partition_func_processor(uchar *int_arg) { return (0);}
};
class st_select_lex;
class Item_ident :public Item
{
protected:
  const char *orig_db_name;
  const char *orig_table_name;
  const char *orig_field_name;
public:
  Name_resolution_context *context;
  const char *db_name;
  const char *table_name;
  const char *field_name;
  In_C_you_should_use_my_bool_instead() alias_name_used;
  uint cached_field_index;
  TABLE_LIST *cached_table;
  st_select_lex *depended_from;
  Item_ident(Name_resolution_context *context_arg,
             const char *db_name_arg, const char *table_name_arg,
             const char *field_name_arg);
  Item_ident(THD *thd, Item_ident *item);
  const char *full_name() const;
  void cleanup();
  In_C_you_should_use_my_bool_instead() remove_dependence_processor(uchar * arg);
  virtual void print(String *str, enum_query_type query_type);
  virtual In_C_you_should_use_my_bool_instead() change_context_processor(uchar *cntx)
    { context= (Name_resolution_context *)cntx; return (0); }
  friend In_C_you_should_use_my_bool_instead() insert_fields(THD *thd, Name_resolution_context *context,
                            const char *db_name,
                            const char *table_name, List_iterator<Item> *it,
                            In_C_you_should_use_my_bool_instead() any_privileges);
};
class Item_ident_for_show :public Item
{
public:
  Field *field;
  const char *db_name;
  const char *table_name;
  Item_ident_for_show(Field *par_field, const char *db_arg,
                      const char *table_name_arg)
    :field(par_field), db_name(db_arg), table_name(table_name_arg)
  {}
  enum Type type() const { return FIELD_ITEM; }
  double val_real() { return field->val_real(); }
  longlong val_int() { return field->val_int(); }
  String *val_str(String *str) { return field->val_str(str); }
  my_decimal *val_decimal(my_decimal *dec) { return field->val_decimal(dec); }
  void make_field(Send_field *tmp_field);
};
class Item_equal;
class COND_EQUAL;
class Item_field :public Item_ident
{
protected:
  void set_field(Field *field);
public:
  Field *field,*result_field;
  Item_equal *item_equal;
  In_C_you_should_use_my_bool_instead() no_const_subst;
  uint have_privileges;
  In_C_you_should_use_my_bool_instead() any_privileges;
  Item_field(Name_resolution_context *context_arg,
             const char *db_arg,const char *table_name_arg,
      const char *field_name_arg);
  Item_field(THD *thd, Item_field *item);
  Item_field(THD *thd, Name_resolution_context *context_arg, Field *field);
  Item_field(Field *field);
  enum Type type() const { return FIELD_ITEM; }
  In_C_you_should_use_my_bool_instead() eq(const Item *item, In_C_you_should_use_my_bool_instead() binary_cmp) const;
  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String*);
  double val_result();
  longlong val_int_result();
  String *str_result(String* tmp);
  my_decimal *val_decimal_result(my_decimal *);
  In_C_you_should_use_my_bool_instead() val_bool_result();
  In_C_you_should_use_my_bool_instead() send(Protocol *protocol, String *str_arg);
  void reset_field(Field *f);
  In_C_you_should_use_my_bool_instead() fix_fields(THD *, Item **);
  void make_field(Send_field *tmp_field);
  int save_in_field(Field *field,In_C_you_should_use_my_bool_instead() no_conversions);
  void save_org_in_field(Field *field);
  table_map used_tables() const;
  enum Item_result result_type () const
  {
    return field->result_type();
  }
  Item_result cast_to_int_type() const
  {
    return field->cast_to_int_type();
  }
  enum_field_types field_type() const
  {
    return field->type();
  }
  enum_monotonicity_info get_monotonicity_info() const
  {
    return MONOTONIC_STRICT_INCREASING;
  }
  longlong val_int_endpoint(In_C_you_should_use_my_bool_instead() left_endp, In_C_you_should_use_my_bool_instead() *incl_endp);
  Field *get_tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg) { return result_field; }
  In_C_you_should_use_my_bool_instead() get_date(MYSQL_TIME *ltime,uint fuzzydate);
  In_C_you_should_use_my_bool_instead() get_date_result(MYSQL_TIME *ltime,uint fuzzydate);
  In_C_you_should_use_my_bool_instead() get_time(MYSQL_TIME *ltime);
  In_C_you_should_use_my_bool_instead() is_null() { return field->is_null(); }
  void update_null_value();
  Item *get_tmp_table_item(THD *thd);
  In_C_you_should_use_my_bool_instead() collect_item_field_processor(uchar * arg);
  In_C_you_should_use_my_bool_instead() find_item_in_field_list_processor(uchar *arg);
  In_C_you_should_use_my_bool_instead() register_field_in_read_map(uchar *arg);
  In_C_you_should_use_my_bool_instead() check_partition_func_processor(uchar *int_arg) {return (0);}
  void cleanup();
  In_C_you_should_use_my_bool_instead() result_as_longlong()
  {
    return field->can_be_compared_as_longlong();
  }
  Item_equal *find_item_equal(COND_EQUAL *cond_equal);
  In_C_you_should_use_my_bool_instead() subst_argument_checker(uchar **arg);
  Item *equal_fields_propagator(uchar *arg);
  In_C_you_should_use_my_bool_instead() set_no_const_sub(uchar *arg);
  Item *replace_equal_field(uchar *arg);
  inline uint32 max_disp_length() { return field->max_display_length(); }
  Item_field *filed_for_view_update() { return this; }
  Item *safe_charset_converter(CHARSET_INFO *tocs);
  int fix_outer_field(THD *thd, Field **field, Item **reference);
  virtual Item *update_value_transformer(uchar *select_arg);
  virtual void print(String *str, enum_query_type query_type);
  Field::geometry_type get_geometry_type() const
  {
    assert(field_type() == MYSQL_TYPE_GEOMETRY);
    return field->get_geometry_type();
  }
  friend class Item_default_value;
  friend class Item_insert_value;
  friend class st_select_lex_unit;
};
class Item_null :public Item_basic_constant
{
public:
  Item_null(char *name_par=0)
  {
    maybe_null= null_value= (1);
    max_length= 0;
    name= name_par ? name_par : (char*) "NULL";
    fixed= 1;
    collation.set(&my_charset_bin, DERIVATION_IGNORABLE);
  }
  enum Type type() const { return NULL_ITEM; }
  In_C_you_should_use_my_bool_instead() eq(const Item *item, In_C_you_should_use_my_bool_instead() binary_cmp) const;
  double val_real();
  longlong val_int();
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
  int save_safe_in_field(Field *field);
  In_C_you_should_use_my_bool_instead() send(Protocol *protocol, String *str);
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_NULL; }
  In_C_you_should_use_my_bool_instead() basic_const_item() const { return 1; }
  Item *clone_item() { return new Item_null(name); }
  In_C_you_should_use_my_bool_instead() is_null() { return 1; }
  virtual inline void print(String *str, enum_query_type query_type)
  {
    str->append(("NULL"), ((size_t) (sizeof("NULL") - 1)));
  }
  Item *safe_charset_converter(CHARSET_INFO *tocs);
  In_C_you_should_use_my_bool_instead() check_partition_func_processor(uchar *int_arg) {return (0);}
};
class Item_null_result :public Item_null
{
public:
  Field *result_field;
  Item_null_result() : Item_null(), result_field(0) {}
  In_C_you_should_use_my_bool_instead() is_result_field() { return result_field != 0; }
  void save_in_result_field(In_C_you_should_use_my_bool_instead() no_conversions)
  {
    save_in_field(result_field, no_conversions);
  }
  In_C_you_should_use_my_bool_instead() check_partition_func_processor(uchar *int_arg) {return (1);}
};
class Item_param :public Item
{
  char cnvbuf[(255*3 +1)];
  String cnvstr;
  Item *cnvitem;
public:
  enum enum_item_param_state
  {
    NO_VALUE, NULL_VALUE, INT_VALUE, REAL_VALUE,
    STRING_VALUE, TIME_VALUE, LONG_DATA_VALUE,
    DECIMAL_VALUE
  } state;
  String str_value_ptr;
  my_decimal decimal_value;
  union
  {
    longlong integer;
    double real;
    struct CONVERSION_INFO
    {
      CHARSET_INFO *character_set_client;
      CHARSET_INFO *character_set_of_placeholder;
      CHARSET_INFO *final_character_set_of_str_value;
    } cs_info;
    MYSQL_TIME time;
  } value;
  enum Item_result item_result_type;
  enum Type item_type;
  enum enum_field_types param_type;
  uint pos_in_query;
  Item_param(uint pos_in_query_arg);
  enum Item_result result_type () const { return item_result_type; }
  enum Type type() const { return item_type; }
  enum_field_types field_type() const { return param_type; }
  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal*);
  String *val_str(String*);
  In_C_you_should_use_my_bool_instead() get_time(MYSQL_TIME *tm);
  In_C_you_should_use_my_bool_instead() get_date(MYSQL_TIME *tm, uint fuzzydate);
  int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
  void set_null();
  void set_int(longlong i, uint32 max_length_arg);
  void set_double(double i);
  void set_decimal(const char *str, ulong length);
  In_C_you_should_use_my_bool_instead() set_str(const char *str, ulong length);
  In_C_you_should_use_my_bool_instead() set_longdata(const char *str, ulong length);
  void set_time(MYSQL_TIME *tm, timestamp_type type, uint32 max_length_arg);
  In_C_you_should_use_my_bool_instead() set_from_user_var(THD *thd, const user_var_entry *entry);
  void reset();
  void (*set_param_func)(Item_param *param, uchar **pos, ulong len);
  const String *query_val_str(String *str) const;
  In_C_you_should_use_my_bool_instead() convert_str_value(THD *thd);
  virtual table_map used_tables() const
  { return state != NO_VALUE ? (table_map)0 : (((table_map) 1) << (sizeof(table_map)*8-3)); }
  virtual void print(String *str, enum_query_type query_type);
  In_C_you_should_use_my_bool_instead() is_null()
  { assert(state != NO_VALUE); return state == NULL_VALUE; }
  In_C_you_should_use_my_bool_instead() basic_const_item() const;
  Item *safe_charset_converter(CHARSET_INFO *tocs);
  Item *clone_item();
  In_C_you_should_use_my_bool_instead() eq(const Item *item, In_C_you_should_use_my_bool_instead() binary_cmp) const;
  In_C_you_should_use_my_bool_instead() limit_clause_param;
  void set_param_type_and_swap_value(Item_param *from);
};
class Item_int :public Item_num
{
public:
  longlong value;
  Item_int(int32 i,uint length= 11)
    :value((longlong) i)
    { max_length=length; fixed= 1; }
  Item_int(longlong i,uint length= 21)
    :value(i)
    { max_length=length; fixed= 1; }
  Item_int(ulonglong i, uint length= 21)
    :value((longlong)i)
    { max_length=length; fixed= 1; unsigned_flag= 1; }
  Item_int(const char *str_arg,longlong i,uint length) :value(i)
    { max_length=length; name=(char*) str_arg; fixed= 1; }
  Item_int(const char *str_arg, uint length=64);
  enum Type type() const { return INT_ITEM; }
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_LONGLONG; }
  longlong val_int() { assert(fixed == 1); return value; }
  double val_real() { assert(fixed == 1); return (double) value; }
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String*);
  int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
  In_C_you_should_use_my_bool_instead() basic_const_item() const { return 1; }
  Item *clone_item() { return new Item_int(name,value,max_length); }
  virtual void print(String *str, enum_query_type query_type);
  Item_num *neg() { value= -value; return this; }
  uint decimal_precision() const
  { return (uint)(max_length - ((value < 0) ? 1 : 0)); }
  In_C_you_should_use_my_bool_instead() eq(const Item *, In_C_you_should_use_my_bool_instead() binary_cmp) const;
  In_C_you_should_use_my_bool_instead() check_partition_func_processor(uchar *bool_arg) { return (0);}
};
class Item_uint :public Item_int
{
public:
  Item_uint(const char *str_arg, uint length);
  Item_uint(ulonglong i) :Item_int((ulonglong) i, 10) {}
  Item_uint(const char *str_arg, longlong i, uint length);
  double val_real()
    { assert(fixed == 1); return ((double) (ulonglong) ((ulonglong)value)); }
  String *val_str(String*);
  Item *clone_item() { return new Item_uint(name, value, max_length); }
  int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
  virtual void print(String *str, enum_query_type query_type);
  Item_num *neg ();
  uint decimal_precision() const { return max_length; }
  In_C_you_should_use_my_bool_instead() check_partition_func_processor(uchar *bool_arg) { return (0);}
};
class Item_decimal :public Item_num
{
protected:
  my_decimal decimal_value;
public:
  Item_decimal(const char *str_arg, uint length, CHARSET_INFO *charset);
  Item_decimal(const char *str, const my_decimal *val_arg,
               uint decimal_par, uint length);
  Item_decimal(my_decimal *value_par);
  Item_decimal(longlong val, In_C_you_should_use_my_bool_instead() unsig);
  Item_decimal(double val, int precision, int scale);
  Item_decimal(const uchar *bin, int precision, int scale);
  enum Type type() const { return DECIMAL_ITEM; }
  enum Item_result result_type () const { return DECIMAL_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_NEWDECIMAL; }
  longlong val_int();
  double val_real();
  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *val) { return &decimal_value; }
  int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
  In_C_you_should_use_my_bool_instead() basic_const_item() const { return 1; }
  Item *clone_item()
  {
    return new Item_decimal(name, &decimal_value, decimals, max_length);
  }
  virtual void print(String *str, enum_query_type query_type);
  Item_num *neg()
  {
    my_decimal_neg(&decimal_value);
    unsigned_flag= !decimal_value.sign();
    return this;
  }
  uint decimal_precision() const { return decimal_value.precision(); }
  In_C_you_should_use_my_bool_instead() eq(const Item *, In_C_you_should_use_my_bool_instead() binary_cmp) const;
  void set_decimal_value(my_decimal *value_par);
  In_C_you_should_use_my_bool_instead() check_partition_func_processor(uchar *bool_arg) { return (0);}
};
class Item_float :public Item_num
{
  char *presentation;
public:
  double value;
  Item_float(const char *str_arg, uint length);
  Item_float(const char *str,double val_arg,uint decimal_par,uint length)
    :value(val_arg)
  {
    presentation= name=(char*) str;
    decimals=(uint8) decimal_par;
    max_length=length;
    fixed= 1;
  }
  Item_float(double value_par, uint decimal_par) :presentation(0), value(value_par)
  {
    decimals= (uint8) decimal_par;
    fixed= 1;
  }
  int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
  enum Type type() const { return REAL_ITEM; }
  enum_field_types field_type() const { return MYSQL_TYPE_DOUBLE; }
  double val_real() { assert(fixed == 1); return value; }
  longlong val_int()
  {
    assert(fixed == 1);
    if (value <= (double) ((long long) 0x8000000000000000LL))
    {
       return ((long long) 0x8000000000000000LL);
    }
    else if (value >= (double) (ulonglong) ((long long) 0x7FFFFFFFFFFFFFFFLL))
    {
      return ((long long) 0x7FFFFFFFFFFFFFFFLL);
    }
    return (longlong) rint(value);
  }
  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *);
  In_C_you_should_use_my_bool_instead() basic_const_item() const { return 1; }
  Item *clone_item()
  { return new Item_float(name, value, decimals, max_length); }
  Item_num *neg() { value= -value; return this; }
  virtual void print(String *str, enum_query_type query_type);
  In_C_you_should_use_my_bool_instead() eq(const Item *, In_C_you_should_use_my_bool_instead() binary_cmp) const;
};
class Item_static_float_func :public Item_float
{
  const char *func_name;
public:
  Item_static_float_func(const char *str, double val_arg, uint decimal_par,
                        uint length)
    :Item_float((char *) 0, val_arg, decimal_par, length), func_name(str)
  {}
  virtual inline void print(String *str, enum_query_type query_type)
  {
    str->append(func_name);
  }
  Item *safe_charset_converter(CHARSET_INFO *tocs);
};
class Item_string :public Item_basic_constant
{
public:
  Item_string(const char *str,uint length,
              CHARSET_INFO *cs, Derivation dv= DERIVATION_COERCIBLE,
              uint repertoire= 3)
    : m_cs_specified((0))
  {
    str_value.set_or_copy_aligned(str, length, cs);
    collation.set(cs, dv, repertoire);
    max_length= str_value.numchars()*cs->mbmaxlen;
    set_name(str, length, cs);
    decimals=31;
    fixed= 1;
  }
  Item_string(CHARSET_INFO *cs, Derivation dv= DERIVATION_COERCIBLE)
    : m_cs_specified((0))
  {
    collation.set(cs, dv);
    max_length= 0;
    set_name(NULL, 0, cs);
    decimals= 31;
    fixed= 1;
  }
  Item_string(const char *name_par, const char *str, uint length,
              CHARSET_INFO *cs, Derivation dv= DERIVATION_COERCIBLE,
              uint repertoire= 3)
    : m_cs_specified((0))
  {
    str_value.set_or_copy_aligned(str, length, cs);
    collation.set(cs, dv, repertoire);
    max_length= str_value.numchars()*cs->mbmaxlen;
    set_name(name_par, 0, cs);
    decimals=31;
    fixed= 1;
  }
  void set_str_with_copy(const char *str_arg, uint length_arg)
  {
    str_value.copy(str_arg, length_arg, collation.collation);
    max_length= str_value.numchars() * collation.collation->mbmaxlen;
  }
  void set_repertoire_from_value()
  {
    collation.repertoire= my_string_repertoire(str_value.charset(),
                                               str_value.ptr(),
                                               str_value.length());
  }
  enum Type type() const { return STRING_ITEM; }
  double val_real();
  longlong val_int();
  String *val_str(String*)
  {
    assert(fixed == 1);
    return (String*) &str_value;
  }
  my_decimal *val_decimal(my_decimal *);
  int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_VARCHAR; }
  In_C_you_should_use_my_bool_instead() basic_const_item() const { return 1; }
  In_C_you_should_use_my_bool_instead() eq(const Item *item, In_C_you_should_use_my_bool_instead() binary_cmp) const;
  Item *clone_item()
  {
    return new Item_string(name, str_value.ptr(),
          str_value.length(), collation.collation);
  }
  Item *safe_charset_converter(CHARSET_INFO *tocs);
  inline void append(char *str, uint length)
  {
    str_value.append(str, length);
    max_length= str_value.numchars() * collation.collation->mbmaxlen;
  }
  virtual void print(String *str, enum_query_type query_type);
  In_C_you_should_use_my_bool_instead() check_partition_func_processor(uchar *int_arg) {return (0);}
  inline In_C_you_should_use_my_bool_instead() is_cs_specified() const
  {
    return m_cs_specified;
  }
  inline void set_cs_specified(In_C_you_should_use_my_bool_instead() cs_specified)
  {
    m_cs_specified= cs_specified;
  }
private:
  In_C_you_should_use_my_bool_instead() m_cs_specified;
};
class Item_static_string_func :public Item_string
{
  const char *func_name;
public:
  Item_static_string_func(const char *name_par, const char *str, uint length,
                          CHARSET_INFO *cs,
                          Derivation dv= DERIVATION_COERCIBLE)
    :Item_string((char *) 0, str, length, cs, dv), func_name(name_par)
  {}
  Item *safe_charset_converter(CHARSET_INFO *tocs);
  virtual inline void print(String *str, enum_query_type query_type)
  {
    str->append(func_name);
  }
  In_C_you_should_use_my_bool_instead() check_partition_func_processor(uchar *int_arg) {return (1);}
};
class Item_partition_func_safe_string: public Item_string
{
public:
  Item_partition_func_safe_string(const char *name, uint length,
                                  CHARSET_INFO *cs= NULL):
    Item_string(name, length, cs)
  {}
};
class Item_return_date_time :public Item_partition_func_safe_string
{
  enum_field_types date_time_field_type;
public:
  Item_return_date_time(const char *name_arg, enum_field_types field_type_arg)
    :Item_partition_func_safe_string(name_arg, 0, &my_charset_bin),
     date_time_field_type(field_type_arg)
  { }
  enum_field_types field_type() const { return date_time_field_type; }
};
class Item_blob :public Item_partition_func_safe_string
{
public:
  Item_blob(const char *name, uint length) :
    Item_partition_func_safe_string(name, length, &my_charset_bin)
  { max_length= length; }
  enum Type type() const { return TYPE_HOLDER; }
  enum_field_types field_type() const { return MYSQL_TYPE_BLOB; }
};
class Item_empty_string :public Item_partition_func_safe_string
{
public:
  Item_empty_string(const char *header,uint length, CHARSET_INFO *cs= NULL) :
    Item_partition_func_safe_string("",0, cs ? cs : &my_charset_utf8_general_ci)
    { name=(char*) header; max_length= cs ? length * cs->mbmaxlen : length; }
  void make_field(Send_field *field);
};
class Item_return_int :public Item_int
{
  enum_field_types int_field_type;
public:
  Item_return_int(const char *name_arg, uint length,
    enum_field_types field_type_arg, longlong value= 0)
    :Item_int(name_arg, value, length), int_field_type(field_type_arg)
  {
    unsigned_flag=1;
  }
  enum_field_types field_type() const { return int_field_type; }
};
class Item_hex_string: public Item_basic_constant
{
public:
  Item_hex_string() {}
  Item_hex_string(const char *str,uint str_length);
  enum Type type() const { return VARBIN_ITEM; }
  double val_real()
  {
    assert(fixed == 1);
    return (double) (ulonglong) Item_hex_string::val_int();
  }
  longlong val_int();
  In_C_you_should_use_my_bool_instead() basic_const_item() const { return 1; }
  String *val_str(String*) { assert(fixed == 1); return &str_value; }
  my_decimal *val_decimal(my_decimal *);
  int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
  enum Item_result result_type () const { return STRING_RESULT; }
  enum Item_result cast_to_int_type() const { return INT_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_VARCHAR; }
  virtual void print(String *str, enum_query_type query_type);
  In_C_you_should_use_my_bool_instead() eq(const Item *item, In_C_you_should_use_my_bool_instead() binary_cmp) const;
  virtual Item *safe_charset_converter(CHARSET_INFO *tocs);
  In_C_you_should_use_my_bool_instead() check_partition_func_processor(uchar *int_arg) {return (0);}
};
class Item_bin_string: public Item_hex_string
{
public:
  Item_bin_string(const char *str,uint str_length);
};
class Item_result_field :public Item
{
public:
  Field *result_field;
  Item_result_field() :result_field(0) {}
  Item_result_field(THD *thd, Item_result_field *item):
    Item(thd, item), result_field(item->result_field)
  {}
  ~Item_result_field() {}
  Field *get_tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg) { return result_field; }
  table_map used_tables() const { return 1; }
  virtual void fix_length_and_dec()=0;
  void set_result_field(Field *field) { result_field= field; }
  In_C_you_should_use_my_bool_instead() is_result_field() { return 1; }
  void save_in_result_field(In_C_you_should_use_my_bool_instead() no_conversions)
  {
    save_in_field(result_field, no_conversions);
  }
  void cleanup();
};
class Item_ref :public Item_ident
{
protected:
  void set_properties();
public:
  enum Ref_Type { REF, DIRECT_REF, VIEW_REF, OUTER_REF };
  Field *result_field;
  Item **ref;
  Item_ref(Name_resolution_context *context_arg,
           const char *db_arg, const char *table_name_arg,
           const char *field_name_arg)
    :Item_ident(context_arg, db_arg, table_name_arg, field_name_arg),
     result_field(0), ref(0) {}
  Item_ref(Name_resolution_context *context_arg, Item **item,
           const char *table_name_arg, const char *field_name_arg,
           In_C_you_should_use_my_bool_instead() alias_name_used_arg= (0));
  Item_ref(THD *thd, Item_ref *item)
    :Item_ident(thd, item), result_field(item->result_field), ref(item->ref) {}
  enum Type type() const { return REF_ITEM; }
  In_C_you_should_use_my_bool_instead() eq(const Item *item, In_C_you_should_use_my_bool_instead() binary_cmp) const
  {
    Item *it= ((Item *) item)->real_item();
    return ref && (*ref)->eq(it, binary_cmp);
  }
  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal *);
  In_C_you_should_use_my_bool_instead() val_bool();
  String *val_str(String* tmp);
  In_C_you_should_use_my_bool_instead() is_null();
  In_C_you_should_use_my_bool_instead() get_date(MYSQL_TIME *ltime,uint fuzzydate);
  double val_result();
  longlong val_int_result();
  String *str_result(String* tmp);
  my_decimal *val_decimal_result(my_decimal *);
  In_C_you_should_use_my_bool_instead() val_bool_result();
  In_C_you_should_use_my_bool_instead() send(Protocol *prot, String *tmp);
  void make_field(Send_field *field);
  In_C_you_should_use_my_bool_instead() fix_fields(THD *, Item **);
  int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
  void save_org_in_field(Field *field);
  enum Item_result result_type () const { return (*ref)->result_type(); }
  enum_field_types field_type() const { return (*ref)->field_type(); }
  Field *get_tmp_table_field()
  { return result_field ? result_field : (*ref)->get_tmp_table_field(); }
  Item *get_tmp_table_item(THD *thd);
  table_map used_tables() const
  {
    return depended_from ? (((table_map) 1) << (sizeof(table_map)*8-2)) : (*ref)->used_tables();
  }
  void update_used_tables()
  {
    if (!depended_from)
      (*ref)->update_used_tables();
  }
  table_map not_null_tables() const { return (*ref)->not_null_tables(); }
  void set_result_field(Field *field) { result_field= field; }
  In_C_you_should_use_my_bool_instead() is_result_field() { return 1; }
  void save_in_result_field(In_C_you_should_use_my_bool_instead() no_conversions)
  {
    (*ref)->save_in_field(result_field, no_conversions);
  }
  Item *real_item()
  {
    return ref ? (*ref)->real_item() : this;
  }
  In_C_you_should_use_my_bool_instead() walk(Item_processor processor, In_C_you_should_use_my_bool_instead() walk_subquery, uchar *arg)
  { return (*ref)->walk(processor, walk_subquery, arg); }
  virtual void print(String *str, enum_query_type query_type);
  In_C_you_should_use_my_bool_instead() result_as_longlong()
  {
    return (*ref)->result_as_longlong();
  }
  void cleanup();
  Item_field *filed_for_view_update()
    { return (*ref)->filed_for_view_update(); }
  virtual Ref_Type ref_type() { return REF; }
  uint cols()
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->cols() : 1;
  }
  Item* element_index(uint i)
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->element_index(i) : this;
  }
  Item** addr(uint i)
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->addr(i) : 0;
  }
  In_C_you_should_use_my_bool_instead() check_cols(uint c)
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->check_cols(c)
                                              : Item::check_cols(c);
  }
  In_C_you_should_use_my_bool_instead() null_inside()
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->null_inside() : 0;
  }
  void bring_value()
  {
    if (ref && result_type() == ROW_RESULT)
      (*ref)->bring_value();
  }
};
class Item_direct_ref :public Item_ref
{
public:
  Item_direct_ref(Name_resolution_context *context_arg, Item **item,
                  const char *table_name_arg,
                  const char *field_name_arg,
                  In_C_you_should_use_my_bool_instead() alias_name_used_arg= (0))
    :Item_ref(context_arg, item, table_name_arg,
              field_name_arg, alias_name_used_arg)
  {}
  Item_direct_ref(THD *thd, Item_direct_ref *item) : Item_ref(thd, item) {}
  double val_real();
  longlong val_int();
  String *val_str(String* tmp);
  my_decimal *val_decimal(my_decimal *);
  In_C_you_should_use_my_bool_instead() val_bool();
  In_C_you_should_use_my_bool_instead() is_null();
  In_C_you_should_use_my_bool_instead() get_date(MYSQL_TIME *ltime,uint fuzzydate);
  virtual Ref_Type ref_type() { return DIRECT_REF; }
};
class Item_direct_view_ref :public Item_direct_ref
{
public:
  Item_direct_view_ref(Name_resolution_context *context_arg, Item **item,
                  const char *table_name_arg,
                  const char *field_name_arg)
    :Item_direct_ref(context_arg, item, table_name_arg, field_name_arg) {}
  Item_direct_view_ref(THD *thd, Item_direct_ref *item)
    :Item_direct_ref(thd, item) {}
  In_C_you_should_use_my_bool_instead() fix_fields(THD *, Item **);
  In_C_you_should_use_my_bool_instead() eq(const Item *item, In_C_you_should_use_my_bool_instead() binary_cmp) const;
  Item *get_tmp_table_item(THD *thd)
  {
    Item *item= Item_ref::get_tmp_table_item(thd);
    item->name= name;
    return item;
  }
  virtual Ref_Type ref_type() { return VIEW_REF; }
};
class Item_sum;
class Item_outer_ref :public Item_direct_ref
{
public:
  Item *outer_ref;
  Item_sum *in_sum_func;
  In_C_you_should_use_my_bool_instead() found_in_select_list;
  Item_outer_ref(Name_resolution_context *context_arg,
                 Item_field *outer_field_arg)
    :Item_direct_ref(context_arg, 0, outer_field_arg->table_name,
                     outer_field_arg->field_name),
    outer_ref(outer_field_arg), in_sum_func(0),
    found_in_select_list(0)
  {
    ref= &outer_ref;
    set_properties();
    fixed= 0;
  }
  Item_outer_ref(Name_resolution_context *context_arg, Item **item,
                 const char *table_name_arg, const char *field_name_arg,
                 In_C_you_should_use_my_bool_instead() alias_name_used_arg)
    :Item_direct_ref(context_arg, item, table_name_arg, field_name_arg,
                     alias_name_used_arg),
    outer_ref(0), in_sum_func(0), found_in_select_list(1)
  {}
  void save_in_result_field(In_C_you_should_use_my_bool_instead() no_conversions)
  {
    outer_ref->save_org_in_field(result_field);
  }
  In_C_you_should_use_my_bool_instead() fix_fields(THD *, Item **);
  table_map used_tables() const
  {
    return (*ref)->const_item() ? 0 : (((table_map) 1) << (sizeof(table_map)*8-2));
  }
  virtual Ref_Type ref_type() { return OUTER_REF; }
};
class Item_in_subselect;
class Item_ref_null_helper: public Item_ref
{
protected:
  Item_in_subselect* owner;
public:
  Item_ref_null_helper(Name_resolution_context *context_arg,
                       Item_in_subselect* master, Item **item,
         const char *table_name_arg, const char *field_name_arg)
    :Item_ref(context_arg, item, table_name_arg, field_name_arg),
     owner(master) {}
  double val_real();
  longlong val_int();
  String* val_str(String* s);
  my_decimal *val_decimal(my_decimal *);
  In_C_you_should_use_my_bool_instead() val_bool();
  In_C_you_should_use_my_bool_instead() get_date(MYSQL_TIME *ltime, uint fuzzydate);
  virtual void print(String *str, enum_query_type query_type);
  table_map used_tables() const
  {
    return (depended_from ?
            (((table_map) 1) << (sizeof(table_map)*8-2)) :
            (*ref)->used_tables() | (((table_map) 1) << (sizeof(table_map)*8-1)));
  }
};
class Item_int_with_ref :public Item_int
{
  Item *ref;
public:
  Item_int_with_ref(longlong i, Item *ref_arg, my_bool unsigned_arg) :
    Item_int(i), ref(ref_arg)
  {
    unsigned_flag= unsigned_arg;
  }
  int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions)
  {
    return ref->save_in_field(field, no_conversions);
  }
  Item *clone_item();
  virtual Item *real_item() { return ref; }
};
class Item_copy_string :public Item
{
  enum enum_field_types cached_field_type;
public:
  Item *item;
  Item_copy_string(Item *i) :item(i)
  {
    null_value=maybe_null=item->maybe_null;
    decimals=item->decimals;
    max_length=item->max_length;
    name=item->name;
    cached_field_type= item->field_type();
  }
  enum Type type() const { return COPY_STR_ITEM; }
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return cached_field_type; }
  double val_real()
  {
    int err_not_used;
    char *end_not_used;
    return (null_value ? 0.0 :
     ((str_value.charset())->cset->strntod((str_value.charset()),((char*) str_value.ptr()),(str_value.length()),(&end_not_used),(&err_not_used))));
  }
  longlong val_int()
  {
    int err;
    return null_value ? 0LL : ((str_value.charset())->cset->strntoll((str_value.charset()),(str_value.ptr()),(str_value.length()),(10),((char**) 0),(&err)));
  }
  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *);
  void make_field(Send_field *field) { item->make_field(field); }
  void copy();
  int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions)
  {
    return save_str_value_in_field(field, &str_value);
  }
  table_map used_tables() const { return (table_map) 1L; }
  In_C_you_should_use_my_bool_instead() const_item() const { return 0; }
  In_C_you_should_use_my_bool_instead() is_null() { return null_value; }
};
class Cached_item :public Sql_alloc
{
public:
  my_bool null_value;
  Cached_item() :null_value(0) {}
  virtual In_C_you_should_use_my_bool_instead() cmp(void)=0;
  virtual ~Cached_item();
};
class Cached_item_str :public Cached_item
{
  Item *item;
  String value,tmp_value;
public:
  Cached_item_str(THD *thd, Item *arg);
  In_C_you_should_use_my_bool_instead() cmp(void);
  ~Cached_item_str();
};
class Cached_item_real :public Cached_item
{
  Item *item;
  double value;
public:
  Cached_item_real(Item *item_par) :item(item_par),value(0.0) {}
  In_C_you_should_use_my_bool_instead() cmp(void);
};
class Cached_item_int :public Cached_item
{
  Item *item;
  longlong value;
public:
  Cached_item_int(Item *item_par) :item(item_par),value(0) {}
  In_C_you_should_use_my_bool_instead() cmp(void);
};
class Cached_item_decimal :public Cached_item
{
  Item *item;
  my_decimal value;
public:
  Cached_item_decimal(Item *item_par);
  In_C_you_should_use_my_bool_instead() cmp(void);
};
class Cached_item_field :public Cached_item
{
  uchar *buff;
  Field *field;
  uint length;
public:
  Cached_item_field(Item_field *item)
  {
    field= item->field;
    buff= (uchar*) sql_calloc(length=field->pack_length());
  }
  In_C_you_should_use_my_bool_instead() cmp(void);
};
class Item_default_value : public Item_field
{
public:
  Item *arg;
  Item_default_value(Name_resolution_context *context_arg)
    :Item_field(context_arg, (const char *)NULL, (const char *)NULL,
               (const char *)NULL),
     arg(NULL) {}
  Item_default_value(Name_resolution_context *context_arg, Item *a)
    :Item_field(context_arg, (const char *)NULL, (const char *)NULL,
                (const char *)NULL),
     arg(a) {}
  enum Type type() const { return DEFAULT_VALUE_ITEM; }
  In_C_you_should_use_my_bool_instead() eq(const Item *item, In_C_you_should_use_my_bool_instead() binary_cmp) const;
  In_C_you_should_use_my_bool_instead() fix_fields(THD *, Item **);
  virtual void print(String *str, enum_query_type query_type);
  int save_in_field(Field *field_arg, In_C_you_should_use_my_bool_instead() no_conversions);
  table_map used_tables() const { return (table_map)0L; }
  In_C_you_should_use_my_bool_instead() walk(Item_processor processor, In_C_you_should_use_my_bool_instead() walk_subquery, uchar *args)
  {
    return arg->walk(processor, walk_subquery, args) ||
      (this->*processor)(args);
  }
  Item *transform(Item_transformer transformer, uchar *args);
};
class Item_insert_value : public Item_field
{
public:
  Item *arg;
  Item_insert_value(Name_resolution_context *context_arg, Item *a)
    :Item_field(context_arg, (const char *)NULL, (const char *)NULL,
               (const char *)NULL),
     arg(a) {}
  In_C_you_should_use_my_bool_instead() eq(const Item *item, In_C_you_should_use_my_bool_instead() binary_cmp) const;
  In_C_you_should_use_my_bool_instead() fix_fields(THD *, Item **);
  virtual void print(String *str, enum_query_type query_type);
  int save_in_field(Field *field_arg, In_C_you_should_use_my_bool_instead() no_conversions)
  {
    return Item_field::save_in_field(field_arg, no_conversions);
  }
  table_map used_tables() const { return (((table_map) 1) << (sizeof(table_map)*8-1)); }
  In_C_you_should_use_my_bool_instead() walk(Item_processor processor, In_C_you_should_use_my_bool_instead() walk_subquery, uchar *args)
  {
    return arg->walk(processor, walk_subquery, args) ||
     (this->*processor)(args);
  }
};
enum trg_action_time_type
{
  TRG_ACTION_BEFORE= 0, TRG_ACTION_AFTER= 1, TRG_ACTION_MAX
};
class Table_triggers_list;
class Item_trigger_field : public Item_field,
                           private Settable_routine_parameter
{
public:
  enum row_version_type {OLD_ROW, NEW_ROW};
  row_version_type row_version;
  Item_trigger_field *next_trg_field;
  uint field_idx;
  Table_triggers_list *triggers;
  Item_trigger_field(Name_resolution_context *context_arg,
                     row_version_type row_ver_arg,
                     const char *field_name_arg,
                     ulong priv, const In_C_you_should_use_my_bool_instead() ro)
    :Item_field(context_arg,
               (const char *)NULL, (const char *)NULL, field_name_arg),
     row_version(row_ver_arg), field_idx((uint)-1), original_privilege(priv),
     want_privilege(priv), table_grants(NULL), read_only (ro)
  {}
  void setup_field(THD *thd, TABLE *table, GRANT_INFO *table_grant_info);
  enum Type type() const { return TRIGGER_FIELD_ITEM; }
  In_C_you_should_use_my_bool_instead() eq(const Item *item, In_C_you_should_use_my_bool_instead() binary_cmp) const;
  In_C_you_should_use_my_bool_instead() fix_fields(THD *, Item **);
  virtual void print(String *str, enum_query_type query_type);
  table_map used_tables() const { return (table_map)0L; }
  Field *get_tmp_table_field() { return 0; }
  Item *copy_or_same(THD *thd) { return this; }
  Item *get_tmp_table_item(THD *thd) { return copy_or_same(thd); }
  void cleanup();
private:
  void set_required_privilege(In_C_you_should_use_my_bool_instead() rw);
  In_C_you_should_use_my_bool_instead() set_value(THD *thd, sp_rcontext *ctx, Item **it);
public:
  Settable_routine_parameter *get_settable_routine_parameter()
  {
    return (read_only ? 0 : this);
  }
  In_C_you_should_use_my_bool_instead() set_value(THD *thd, Item **it)
  {
    return set_value(thd, NULL, it);
  }
private:
  ulong original_privilege;
  ulong want_privilege;
  GRANT_INFO *table_grants;
  In_C_you_should_use_my_bool_instead() read_only;
};
class Item_cache: public Item_basic_constant
{
protected:
  Item *example;
  table_map used_table_map;
  Field *cached_field;
  enum enum_field_types cached_field_type;
public:
  Item_cache():
    example(0), used_table_map(0), cached_field(0), cached_field_type(MYSQL_TYPE_STRING)
  {
    fixed= 1;
    null_value= 1;
  }
  Item_cache(enum_field_types field_type_arg):
    example(0), used_table_map(0), cached_field(0), cached_field_type(field_type_arg)
  {
    fixed= 1;
    null_value= 1;
  }
  void set_used_tables(table_map map) { used_table_map= map; }
  virtual In_C_you_should_use_my_bool_instead() allocate(uint i) { return 0; }
  virtual In_C_you_should_use_my_bool_instead() setup(Item *item)
  {
    example= item;
    max_length= item->max_length;
    decimals= item->decimals;
    collation.set(item->collation);
    unsigned_flag= item->unsigned_flag;
    if (item->type() == FIELD_ITEM)
      cached_field= ((Item_field *)item)->field;
    return 0;
  };
  virtual void store(Item *)= 0;
  enum Type type() const { return CACHE_ITEM; }
  enum_field_types field_type() const { return cached_field_type; }
  static Item_cache* get_cache(const Item *item);
  table_map used_tables() const { return used_table_map; }
  virtual void keep_array() {}
  virtual void print(String *str, enum_query_type query_type);
  In_C_you_should_use_my_bool_instead() eq_def(Field *field)
  {
    return cached_field ? cached_field->eq_def (field) : (0);
  }
  In_C_you_should_use_my_bool_instead() eq(const Item *item, In_C_you_should_use_my_bool_instead() binary_cmp) const
  {
    return this == item;
  }
};
class Item_cache_int: public Item_cache
{
protected:
  longlong value;
public:
  Item_cache_int(): Item_cache(), value(0) {}
  Item_cache_int(enum_field_types field_type_arg):
    Item_cache(field_type_arg), value(0) {}
  void store(Item *item);
  void store(Item *item, longlong val_arg);
  double val_real() { assert(fixed == 1); return (double) value; }
  longlong val_int() { assert(fixed == 1); return value; }
  String* val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type() const { return INT_RESULT; }
  In_C_you_should_use_my_bool_instead() result_as_longlong() { return (1); }
};
class Item_cache_real: public Item_cache
{
  double value;
public:
  Item_cache_real(): Item_cache(), value(0) {}
  void store(Item *item);
  double val_real() { assert(fixed == 1); return value; }
  longlong val_int();
  String* val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type() const { return REAL_RESULT; }
};
class Item_cache_decimal: public Item_cache
{
protected:
  my_decimal decimal_value;
public:
  Item_cache_decimal(): Item_cache() {}
  void store(Item *item);
  double val_real();
  longlong val_int();
  String* val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type() const { return DECIMAL_RESULT; }
};
class Item_cache_str: public Item_cache
{
  char buffer[80];
  String *value, value_buff;
  In_C_you_should_use_my_bool_instead() is_varbinary;
public:
  Item_cache_str(const Item *item) :
    Item_cache(), value(0),
    is_varbinary(item->type() == FIELD_ITEM &&
                 ((const Item_field *) item)->field->type() ==
                   MYSQL_TYPE_VARCHAR &&
                 !((const Item_field *) item)->field->has_charset())
  {}
  void store(Item *item);
  double val_real();
  longlong val_int();
  String* val_str(String *) { assert(fixed == 1); return value; }
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type() const { return STRING_RESULT; }
  CHARSET_INFO *charset() const { return value->charset(); };
  int save_in_field(Field *field, In_C_you_should_use_my_bool_instead() no_conversions);
};
class Item_cache_row: public Item_cache
{
  Item_cache **values;
  uint item_count;
  In_C_you_should_use_my_bool_instead() save_array;
public:
  Item_cache_row()
    :Item_cache(), values(0), item_count(2), save_array(0) {}
  In_C_you_should_use_my_bool_instead() allocate(uint num);
  In_C_you_should_use_my_bool_instead() setup(Item *item);
  void store(Item *item);
  void illegal_method_call(const char *);
  void make_field(Send_field *)
  {
    illegal_method_call((const char*)"make_field");
  };
  double val_real()
  {
    illegal_method_call((const char*)"val");
    return 0;
  };
  longlong val_int()
  {
    illegal_method_call((const char*)"val_int");
    return 0;
  };
  String *val_str(String *)
  {
    illegal_method_call((const char*)"val_str");
    return 0;
  };
  my_decimal *val_decimal(my_decimal *val)
  {
    illegal_method_call((const char*)"val_decimal");
    return 0;
  };
  enum Item_result result_type() const { return ROW_RESULT; }
  uint cols() { return item_count; }
  Item *element_index(uint i) { return values[i]; }
  Item **addr(uint i) { return (Item **) (values + i); }
  In_C_you_should_use_my_bool_instead() check_cols(uint c);
  In_C_you_should_use_my_bool_instead() null_inside();
  void bring_value();
  void keep_array() { save_array= 1; }
  void cleanup()
  {
    const char *_db_func_, *_db_file_; uint _db_level_; char **_db_framep_; _db_enter_ ("Item_cache_row::cleanup","./sql/item.h",2898,&_db_func_,&_db_file_,&_db_level_, &_db_framep_);
    Item_cache::cleanup();
    if (save_array)
      bzero(values, item_count*sizeof(Item**));
    else
      values= 0;
    do {_db_return_ (2904, &_db_func_, &_db_file_, &_db_level_); return;} while(0);
  }
};
class Item_type_holder: public Item
{
protected:
  TYPELIB *enum_set_typelib;
  enum_field_types fld_type;
  Field::geometry_type geometry_type;
  void get_full_info(Item *item);
  int prev_decimal_int_part;
public:
  Item_type_holder(THD*, Item*);
  Item_result result_type() const;
  enum_field_types field_type() const { return fld_type; };
  enum Type type() const { return TYPE_HOLDER; }
  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String*);
  In_C_you_should_use_my_bool_instead() join_types(THD *thd, Item *);
  Field *make_field_by_type(TABLE *table);
  static uint32 display_length(Item *item);
  static enum_field_types get_real_type(Item *);
  Field::geometry_type get_geometry_type() const { return geometry_type; };
};
class st_select_lex;
void mark_select_range_as_dependent(THD *thd,
                                    st_select_lex *last_select,
                                    st_select_lex *current_sel,
                                    Field *found_field, Item *found_item,
                                    Item_ident *resolved_item);
extern Cached_item *new_Cached_item(THD *thd, Item *item);
extern Item_result item_cmp_type(Item_result a,Item_result b);
extern void resolve_const_item(THD *thd, Item **ref, Item *cmp_item);
extern In_C_you_should_use_my_bool_instead() field_is_equal_to_item(Field *field,Item *item);
extern my_decimal decimal_zero;
void free_items(Item *item);
void cleanup_items(Item *item);
class THD;
void close_thread_tables(THD *thd);
In_C_you_should_use_my_bool_instead() check_one_table_access(THD *thd, ulong privilege, TABLE_LIST *tables);
In_C_you_should_use_my_bool_instead() check_single_table_access(THD *thd, ulong privilege,
      TABLE_LIST *tables, In_C_you_should_use_my_bool_instead() no_errors);
In_C_you_should_use_my_bool_instead() check_routine_access(THD *thd,ulong want_access,char *db,char *name,
     In_C_you_should_use_my_bool_instead() is_proc, In_C_you_should_use_my_bool_instead() no_errors);
In_C_you_should_use_my_bool_instead() check_some_access(THD *thd, ulong want_access, TABLE_LIST *table);
In_C_you_should_use_my_bool_instead() check_some_routine_access(THD *thd, const char *db, const char *name, In_C_you_should_use_my_bool_instead() is_proc);
In_C_you_should_use_my_bool_instead() multi_update_precheck(THD *thd, TABLE_LIST *tables);
In_C_you_should_use_my_bool_instead() multi_delete_precheck(THD *thd, TABLE_LIST *tables);
int mysql_multi_update_prepare(THD *thd);
int mysql_multi_delete_prepare(THD *thd);
In_C_you_should_use_my_bool_instead() mysql_insert_select_prepare(THD *thd);
In_C_you_should_use_my_bool_instead() update_precheck(THD *thd, TABLE_LIST *tables);
In_C_you_should_use_my_bool_instead() delete_precheck(THD *thd, TABLE_LIST *tables);
In_C_you_should_use_my_bool_instead() insert_precheck(THD *thd, TABLE_LIST *tables);
In_C_you_should_use_my_bool_instead() create_table_precheck(THD *thd, TABLE_LIST *tables,
                           TABLE_LIST *create_table);
int append_query_string(CHARSET_INFO *csinfo,
                        String const *from, String *to);
void get_default_definer(THD *thd, LEX_USER *definer);
LEX_USER *create_default_definer(THD *thd);
LEX_USER *create_definer(THD *thd, LEX_STRING *user_name, LEX_STRING *host_name);
LEX_USER *get_current_user(THD *thd, LEX_USER *user);
In_C_you_should_use_my_bool_instead() check_string_byte_length(LEX_STRING *str, const char *err_msg,
                              uint max_byte_length);
In_C_you_should_use_my_bool_instead() check_string_char_length(LEX_STRING *str, const char *err_msg,
                              uint max_char_length, CHARSET_INFO *cs,
                              In_C_you_should_use_my_bool_instead() no_error);
In_C_you_should_use_my_bool_instead() test_if_data_home_dir(const char *dir);
In_C_you_should_use_my_bool_instead() parse_sql(THD *thd,
               class Lex_input_stream *lip,
               class Object_creation_ctx *creation_ctx);
enum enum_mysql_completiontype {
  ROLLBACK_RELEASE=-2, ROLLBACK=1, ROLLBACK_AND_CHAIN=7,
  COMMIT_RELEASE=-1, COMMIT=0, COMMIT_AND_CHAIN=6
};
In_C_you_should_use_my_bool_instead() begin_trans(THD *thd);
In_C_you_should_use_my_bool_instead() end_active_trans(THD *thd);
int end_trans(THD *thd, enum enum_mysql_completiontype completion);
Item *negate_expression(THD *thd, Item *expr);
int vprint_msg_to_log(enum loglevel level, const char *format, va_list args);
void sql_print_error(const char *format, ...) __attribute__((format(printf, 1, 2)));
void sql_print_warning(const char *format, ...) __attribute__((format(printf, 1, 2)));
void sql_print_information(const char *format, ...)
  __attribute__((format(printf, 1, 2)));
typedef void (*sql_print_message_func)(const char *format, ...)
  __attribute__((format(printf, 1, 2)));
extern sql_print_message_func sql_print_message_handlers[];
int error_log_print(enum loglevel level, const char *format,
                    va_list args);
In_C_you_should_use_my_bool_instead() slow_log_print(THD *thd, const char *query, uint query_length,
                    ulonglong current_utime);
In_C_you_should_use_my_bool_instead() general_log_print(THD *thd, enum enum_server_command command,
                       const char *format,...);
In_C_you_should_use_my_bool_instead() general_log_write(THD *thd, enum enum_server_command command,
                       const char *query, uint query_length);
#include "sql_class.h"
#include "log.h"
class Relay_log_info;
class Format_description_log_event;
class TC_LOG
{
  public:
  int using_heuristic_recover();
  TC_LOG() {}
  virtual ~TC_LOG() {}
  virtual int open(const char *opt_name)=0;
  virtual void close()=0;
  virtual int log_xid(THD *thd, my_xid xid)=0;
  virtual void unlog(ulong cookie, my_xid xid)=0;
};
class TC_LOG_DUMMY: public TC_LOG
{
public:
  TC_LOG_DUMMY() {}
  int open(const char *opt_name) { return 0; }
  void close() { }
  int log_xid(THD *thd, my_xid xid) { return 1; }
  void unlog(ulong cookie, my_xid xid) { }
};
class TC_LOG_MMAP: public TC_LOG
{
  public:
  typedef enum {
    POOL,
    ERROR,
    DIRTY
  } PAGE_STATE;
  private:
  typedef struct st_page {
    struct st_page *next;
    my_xid *start, *end;
    my_xid *ptr;
    int size, free;
    int waiters;
    PAGE_STATE state;
    pthread_mutex_t lock;
    pthread_cond_t cond;
  } PAGE;
  char logname[512];
  File fd;
  my_off_t file_length;
  uint npages, inited;
  uchar *data;
  struct st_page *pages, *syncing, *active, *pool, *pool_last;
  pthread_mutex_t LOCK_active, LOCK_pool, LOCK_sync;
  pthread_cond_t COND_pool, COND_active;
  public:
  TC_LOG_MMAP(): inited(0) {}
  int open(const char *opt_name);
  void close();
  int log_xid(THD *thd, my_xid xid);
  void unlog(ulong cookie, my_xid xid);
  int recover();
  private:
  void get_active_from_pool();
  int sync();
  int overflow();
};
extern TC_LOG *tc_log;
extern TC_LOG_MMAP tc_log_mmap;
extern TC_LOG_DUMMY tc_log_dummy;
class Relay_log_info;
typedef struct st_log_info
{
  char log_file_name[512];
  my_off_t index_file_offset, index_file_start_offset;
  my_off_t pos;
  In_C_you_should_use_my_bool_instead() fatal;
  pthread_mutex_t lock;
  st_log_info()
    : index_file_offset(0), index_file_start_offset(0),
      pos(0), fatal(0)
    {
      log_file_name[0] = '\0';
      pthread_mutex_init(&lock, NULL);
    }
  ~st_log_info() { pthread_mutex_destroy(&lock);}
} LOG_INFO;
class Log_event;
class Rows_log_event;
enum enum_log_type { LOG_UNKNOWN, LOG_NORMAL, LOG_BIN };
enum enum_log_state { LOG_OPENED, LOG_CLOSED, LOG_TO_BE_OPENED };
class MYSQL_LOG
{
public:
  MYSQL_LOG();
  void init_pthread_objects();
  void cleanup();
  In_C_you_should_use_my_bool_instead() open(const char *log_name,
            enum_log_type log_type,
            const char *new_name,
            enum cache_type io_cache_type_arg);
  void init(enum_log_type log_type_arg,
            enum cache_type io_cache_type_arg);
  void close(uint exiting);
  inline In_C_you_should_use_my_bool_instead() is_open() { return log_state != LOG_CLOSED; }
  const char *generate_name(const char *log_name, const char *suffix,
                            In_C_you_should_use_my_bool_instead() strip_ext, char *buff);
  int generate_new_name(char *new_name, const char *log_name);
 protected:
  pthread_mutex_t LOCK_log;
  char *name;
  char log_file_name[512];
  char time_buff[20], db[(64*3) + 1];
  In_C_you_should_use_my_bool_instead() write_error, inited;
  IO_CACHE log_file;
  enum_log_type log_type;
  volatile enum_log_state log_state;
  enum cache_type io_cache_type;
  friend class Log_event;
};
class MYSQL_QUERY_LOG: public MYSQL_LOG
{
public:
  MYSQL_QUERY_LOG() : last_time(0) {}
  void reopen_file();
  In_C_you_should_use_my_bool_instead() write(time_t event_time, const char *user_host,
             uint user_host_len, int thread_id,
             const char *command_type, uint command_type_len,
             const char *sql_text, uint sql_text_len);
  In_C_you_should_use_my_bool_instead() write(THD *thd, time_t current_time, time_t query_start_arg,
             const char *user_host, uint user_host_len,
             ulonglong query_utime, ulonglong lock_utime, In_C_you_should_use_my_bool_instead() is_command,
             const char *sql_text, uint sql_text_len);
  In_C_you_should_use_my_bool_instead() open_slow_log(const char *log_name)
  {
    char buf[512];
    return open(generate_name(log_name, "-slow.log", 0, buf), LOG_NORMAL, 0,
                WRITE_CACHE);
  }
  In_C_you_should_use_my_bool_instead() open_query_log(const char *log_name)
  {
    char buf[512];
    return open(generate_name(log_name, ".log", 0, buf), LOG_NORMAL, 0,
                WRITE_CACHE);
  }
private:
  time_t last_time;
};
class MYSQL_BIN_LOG: public TC_LOG, private MYSQL_LOG
{
 private:
  pthread_mutex_t LOCK_index;
  pthread_mutex_t LOCK_prep_xids;
  pthread_cond_t COND_prep_xids;
  pthread_cond_t update_cond;
  ulonglong bytes_written;
  IO_CACHE index_file;
  char index_file_name[512];
  ulong max_size;
  long prepared_xids;
  uint file_id;
  uint open_count;
  int readers_count;
  In_C_you_should_use_my_bool_instead() need_start_event;
  In_C_you_should_use_my_bool_instead() no_auto_events;
  ulonglong m_table_map_version;
  int write_to_file(IO_CACHE *cache);
  void new_file_without_locking();
  void new_file_impl(In_C_you_should_use_my_bool_instead() need_lock);
public:
  MYSQL_LOG::generate_name;
  MYSQL_LOG::is_open;
  Format_description_log_event *description_event_for_exec,
    *description_event_for_queue;
  MYSQL_BIN_LOG();
  int open(const char *opt_name);
  void close();
  int log_xid(THD *thd, my_xid xid);
  void unlog(ulong cookie, my_xid xid);
  int recover(IO_CACHE *log, Format_description_log_event *fdle);
  In_C_you_should_use_my_bool_instead() is_table_mapped(TABLE *table) const
  {
    return table->s->table_map_version == table_map_version();
  }
  ulonglong table_map_version() const { return m_table_map_version; }
  void update_table_map_version() { ++m_table_map_version; }
  int flush_and_set_pending_rows_event(THD *thd, Rows_log_event* event);
  void reset_bytes_written()
  {
    bytes_written = 0;
  }
  void harvest_bytes_written(ulonglong* counter)
  {
    char buf1[22],buf2[22];
    const char *_db_func_, *_db_file_; uint _db_level_; char **_db_framep_; _db_enter_ ("harvest_bytes_written","./sql/log.h",321,&_db_func_,&_db_file_,&_db_level_, &_db_framep_);
    (*counter)+=bytes_written;
    do {_db_pargs_(324,"info"); _db_doprnt_ ("counter: %s  bytes_written: %s", llstr(*counter,buf1), llstr(bytes_written,buf2));} while(0);
    bytes_written=0;
    do {_db_return_ (326, &_db_func_, &_db_file_, &_db_level_); return;} while(0);
  }
  void set_max_size(ulong max_size_arg);
  void signal_update();
  void wait_for_update(THD* thd, In_C_you_should_use_my_bool_instead() master_or_slave);
  void set_need_start_event() { need_start_event = 1; }
  void init(In_C_you_should_use_my_bool_instead() no_auto_events_arg, ulong max_size);
  void init_pthread_objects();
  void cleanup();
  In_C_you_should_use_my_bool_instead() open(const char *log_name,
            enum_log_type log_type,
            const char *new_name,
     enum cache_type io_cache_type_arg,
     In_C_you_should_use_my_bool_instead() no_auto_events_arg, ulong max_size,
            In_C_you_should_use_my_bool_instead() null_created);
  In_C_you_should_use_my_bool_instead() open_index_file(const char *index_file_name_arg,
                       const char *log_name);
  void new_file();
  In_C_you_should_use_my_bool_instead() write(Log_event* event_info);
  In_C_you_should_use_my_bool_instead() write(THD *thd, IO_CACHE *cache, Log_event *commit_event);
  int write_cache(IO_CACHE *cache, In_C_you_should_use_my_bool_instead() lock_log, In_C_you_should_use_my_bool_instead() flush_and_sync);
  void start_union_events(THD *thd, query_id_t query_id_param);
  void stop_union_events(THD *thd);
  In_C_you_should_use_my_bool_instead() is_query_in_union(THD *thd, query_id_t query_id_param);
  In_C_you_should_use_my_bool_instead() appendv(const char* buf,uint len,...);
  In_C_you_should_use_my_bool_instead() append(Log_event* ev);
  void make_log_name(char* buf, const char* log_ident);
  In_C_you_should_use_my_bool_instead() is_active(const char* log_file_name);
  int update_log_index(LOG_INFO* linfo, In_C_you_should_use_my_bool_instead() need_update_threads);
  void rotate_and_purge(uint flags);
  In_C_you_should_use_my_bool_instead() flush_and_sync();
  int purge_logs(const char *to_log, In_C_you_should_use_my_bool_instead() included,
                 In_C_you_should_use_my_bool_instead() need_mutex, In_C_you_should_use_my_bool_instead() need_update_threads,
                 ulonglong *decrease_log_space);
  int purge_logs_before_date(time_t purge_time);
  int purge_first_log(Relay_log_info* rli, In_C_you_should_use_my_bool_instead() included);
  In_C_you_should_use_my_bool_instead() reset_logs(THD* thd);
  void close(uint exiting);
  int find_log_pos(LOG_INFO* linfo, const char* log_name,
     In_C_you_should_use_my_bool_instead() need_mutex);
  int find_next_log(LOG_INFO* linfo, In_C_you_should_use_my_bool_instead() need_mutex);
  int get_current_log(LOG_INFO* linfo);
  int raw_get_current_log(LOG_INFO* linfo);
  uint next_file_id();
  inline char* get_index_fname() { return index_file_name;}
  inline char* get_log_fname() { return log_file_name; }
  inline char* get_name() { return name; }
  inline pthread_mutex_t* get_log_lock() { return &LOCK_log; }
  inline IO_CACHE* get_log_file() { return &log_file; }
  inline void lock_index() { pthread_mutex_lock(&LOCK_index);}
  inline void unlock_index() { pthread_mutex_unlock(&LOCK_index);}
  inline IO_CACHE *get_index_file() { return &index_file;}
  inline uint32 get_open_count() { return open_count; }
};
class Log_event_handler
{
public:
  Log_event_handler() {}
  virtual In_C_you_should_use_my_bool_instead() init()= 0;
  virtual void cleanup()= 0;
  virtual In_C_you_should_use_my_bool_instead() log_slow(THD *thd, time_t current_time,
                        time_t query_start_arg, const char *user_host,
                        uint user_host_len, ulonglong query_utime,
                        ulonglong lock_utime, In_C_you_should_use_my_bool_instead() is_command,
                        const char *sql_text, uint sql_text_len)= 0;
  virtual In_C_you_should_use_my_bool_instead() log_error(enum loglevel level, const char *format,
                         va_list args)= 0;
  virtual In_C_you_should_use_my_bool_instead() log_general(THD *thd, time_t event_time, const char *user_host,
                           uint user_host_len, int thread_id,
                           const char *command_type, uint command_type_len,
                           const char *sql_text, uint sql_text_len,
                           CHARSET_INFO *client_cs)= 0;
  virtual ~Log_event_handler() {}
};
int check_if_log_table(uint db_len, const char *db, uint table_name_len,
                       const char *table_name, uint check_if_opened);
class Log_to_csv_event_handler: public Log_event_handler
{
  friend class LOGGER;
public:
  Log_to_csv_event_handler();
  ~Log_to_csv_event_handler();
  virtual In_C_you_should_use_my_bool_instead() init();
  virtual void cleanup();
  virtual In_C_you_should_use_my_bool_instead() log_slow(THD *thd, time_t current_time,
                        time_t query_start_arg, const char *user_host,
                        uint user_host_len, ulonglong query_utime,
                        ulonglong lock_utime, In_C_you_should_use_my_bool_instead() is_command,
                        const char *sql_text, uint sql_text_len);
  virtual In_C_you_should_use_my_bool_instead() log_error(enum loglevel level, const char *format,
                         va_list args);
  virtual In_C_you_should_use_my_bool_instead() log_general(THD *thd, time_t event_time, const char *user_host,
                           uint user_host_len, int thread_id,
                           const char *command_type, uint command_type_len,
                           const char *sql_text, uint sql_text_len,
                           CHARSET_INFO *client_cs);
  int activate_log(THD *thd, uint log_type);
};
class Log_to_file_event_handler: public Log_event_handler
{
  MYSQL_QUERY_LOG mysql_log;
  MYSQL_QUERY_LOG mysql_slow_log;
  In_C_you_should_use_my_bool_instead() is_initialized;
public:
  Log_to_file_event_handler(): is_initialized((0))
  {}
  virtual In_C_you_should_use_my_bool_instead() init();
  virtual void cleanup();
  virtual In_C_you_should_use_my_bool_instead() log_slow(THD *thd, time_t current_time,
                        time_t query_start_arg, const char *user_host,
                        uint user_host_len, ulonglong query_utime,
                        ulonglong lock_utime, In_C_you_should_use_my_bool_instead() is_command,
                        const char *sql_text, uint sql_text_len);
  virtual In_C_you_should_use_my_bool_instead() log_error(enum loglevel level, const char *format,
                         va_list args);
  virtual In_C_you_should_use_my_bool_instead() log_general(THD *thd, time_t event_time, const char *user_host,
                           uint user_host_len, int thread_id,
                           const char *command_type, uint command_type_len,
                           const char *sql_text, uint sql_text_len,
                           CHARSET_INFO *client_cs);
  void flush();
  void init_pthread_objects();
  MYSQL_QUERY_LOG *get_mysql_slow_log() { return &mysql_slow_log; }
  MYSQL_QUERY_LOG *get_mysql_log() { return &mysql_log; }
};
class LOGGER
{
  pthread_rwlock_t LOCK_logger;
  uint inited;
  Log_to_csv_event_handler *table_log_handler;
  Log_to_file_event_handler *file_log_handler;
  Log_event_handler *error_log_handler_list[3 + 1];
  Log_event_handler *slow_log_handler_list[3 + 1];
  Log_event_handler *general_log_handler_list[3 + 1];
public:
  In_C_you_should_use_my_bool_instead() is_log_tables_initialized;
  LOGGER() : inited(0), table_log_handler(NULL),
             file_log_handler(NULL), is_log_tables_initialized((0))
  {}
  void lock_shared() { pthread_rwlock_rdlock(&LOCK_logger); }
  void lock_exclusive() { pthread_rwlock_wrlock(&LOCK_logger); }
  void unlock() { pthread_rwlock_unlock(&LOCK_logger); }
  In_C_you_should_use_my_bool_instead() is_log_table_enabled(uint log_table_type);
  In_C_you_should_use_my_bool_instead() log_command(THD *thd, enum enum_server_command command);
  void init_base();
  void init_log_tables();
  In_C_you_should_use_my_bool_instead() flush_logs(THD *thd);
  void cleanup_base();
  void cleanup_end();
  In_C_you_should_use_my_bool_instead() error_log_print(enum loglevel level, const char *format,
                      va_list args);
  In_C_you_should_use_my_bool_instead() slow_log_print(THD *thd, const char *query, uint query_length,
                      ulonglong current_utime);
  In_C_you_should_use_my_bool_instead() general_log_print(THD *thd,enum enum_server_command command,
                         const char *format, va_list args);
  In_C_you_should_use_my_bool_instead() general_log_write(THD *thd, enum enum_server_command command,
                         const char *query, uint query_length);
  int set_handlers(uint error_log_printer,
                   uint slow_log_printer,
                   uint general_log_printer);
  void init_error_log(uint error_log_printer);
  void init_slow_log(uint slow_log_printer);
  void init_general_log(uint general_log_printer);
  void deactivate_log_handler(THD* thd, uint log_type);
  In_C_you_should_use_my_bool_instead() activate_log_handler(THD* thd, uint log_type);
  MYSQL_QUERY_LOG *get_slow_log_file_handler()
  {
    if (file_log_handler)
      return file_log_handler->get_mysql_slow_log();
    return NULL;
  }
  MYSQL_QUERY_LOG *get_log_file_handler()
  {
    if (file_log_handler)
      return file_log_handler->get_mysql_log();
    return NULL;
  }
};
enum enum_binlog_format {
  BINLOG_FORMAT_MIXED= 0,
  BINLOG_FORMAT_STMT= 1,
  BINLOG_FORMAT_ROW= 2,
  BINLOG_FORMAT_UNSPEC= 3
};
extern TYPELIB binlog_format_typelib;
#include "rpl_tblmap.h"
struct st_table;
typedef st_table TABLE;
class table_mapping {
private:
  MEM_ROOT m_mem_root;
public:
  enum enum_error {
      ERR_NO_ERROR = 0,
      ERR_LIMIT_EXCEEDED,
      ERR_MEMORY_ALLOCATION
  };
  table_mapping();
  ~table_mapping();
  TABLE* get_table(ulong table_id);
  int set_table(ulong table_id, TABLE* table);
  int remove_table(ulong table_id);
  void clear_tables();
  ulong count() const { return m_table_ids.records; }
private:
  struct entry {
    ulong table_id;
    union {
      TABLE *table;
      entry *next;
    };
  };
  entry *find_entry(ulong table_id)
  {
    return (entry *)hash_search(&m_table_ids,
    (uchar*)&table_id,
    sizeof(table_id));
  }
  int expand();
  entry *m_free;
  HASH m_table_ids;
};
class Reprepare_observer
{
public:
  In_C_you_should_use_my_bool_instead() report_error(THD *thd);
  In_C_you_should_use_my_bool_instead() is_invalidated() const { return m_invalidated; }
  void reset_reprepare_observer() { m_invalidated= (0); }
private:
  In_C_you_should_use_my_bool_instead() m_invalidated;
};
class Relay_log_info;
class Query_log_event;
class Load_log_event;
class Slave_log_event;
class sp_rcontext;
class sp_cache;
class Lex_input_stream;
class Rows_log_event;
enum enum_enable_or_disable { LEAVE_AS_IS, ENABLE, DISABLE };
enum enum_ha_read_modes { RFIRST, RNEXT, RPREV, RLAST, RKEY, RNEXT_SAME };
enum enum_duplicates { DUP_ERROR, DUP_REPLACE, DUP_UPDATE };
enum enum_delay_key_write { DELAY_KEY_WRITE_NONE, DELAY_KEY_WRITE_ON,
       DELAY_KEY_WRITE_ALL };
enum enum_slave_exec_mode { SLAVE_EXEC_MODE_STRICT,
                            SLAVE_EXEC_MODE_IDEMPOTENT,
                            SLAVE_EXEC_MODE_LAST_BIT};
enum enum_mark_columns
{ MARK_COLUMNS_NONE, MARK_COLUMNS_READ, MARK_COLUMNS_WRITE};
extern char internal_table_name[2];
extern char empty_c_string[1];
extern const char **errmesg;
extern uint tc_heuristic_recover;
typedef struct st_user_var_events
{
  user_var_entry *user_var_event;
  char *value;
  ulong length;
  Item_result type;
  uint charset_number;
} BINLOG_USER_VAR_EVENT;
typedef struct st_copy_info {
  ha_rows records;
  ha_rows deleted;
  ha_rows updated;
  ha_rows copied;
  ha_rows error_count;
  ha_rows touched;
  enum enum_duplicates handle_duplicates;
  int escape_char, last_errno;
  In_C_you_should_use_my_bool_instead() ignore;
  List<Item> *update_fields;
  List<Item> *update_values;
  TABLE_LIST *view;
} COPY_INFO;
class Key_part_spec :public Sql_alloc {
public:
  const char *field_name;
  uint length;
  Key_part_spec(const char *name,uint len=0) :field_name(name), length(len) {}
  In_C_you_should_use_my_bool_instead() operator==(const Key_part_spec& other) const;
  Key_part_spec *clone(MEM_ROOT *mem_root) const
  { return new (mem_root) Key_part_spec(*this); }
};
class Alter_drop :public Sql_alloc {
public:
  enum drop_type {KEY, COLUMN };
  const char *name;
  enum drop_type type;
  Alter_drop(enum drop_type par_type,const char *par_name)
    :name(par_name), type(par_type) {}
  Alter_drop *clone(MEM_ROOT *mem_root) const
    { return new (mem_root) Alter_drop(*this); }
};
class Alter_column :public Sql_alloc {
public:
  const char *name;
  Item *def;
  Alter_column(const char *par_name,Item *literal)
    :name(par_name), def(literal) {}
  Alter_column *clone(MEM_ROOT *mem_root) const
    { return new (mem_root) Alter_column(*this); }
};
class Key :public Sql_alloc {
public:
  enum Keytype { PRIMARY, UNIQUE, MULTIPLE, FULLTEXT, SPATIAL, FOREIGN_KEY};
  enum Keytype type;
  KEY_CREATE_INFO key_create_info;
  List<Key_part_spec> columns;
  const char *name;
  In_C_you_should_use_my_bool_instead() generated;
  Key(enum Keytype type_par, const char *name_arg,
      KEY_CREATE_INFO *key_info_arg,
      In_C_you_should_use_my_bool_instead() generated_arg, List<Key_part_spec> &cols)
    :type(type_par), key_create_info(*key_info_arg), columns(cols),
    name(name_arg), generated(generated_arg)
  {}
  Key(const Key &rhs, MEM_ROOT *mem_root);
  virtual ~Key() {}
  friend In_C_you_should_use_my_bool_instead() foreign_key_prefix(Key *a, Key *b);
  virtual Key *clone(MEM_ROOT *mem_root) const
    { return new (mem_root) Key(*this, mem_root); }
};
class Table_ident;
class Foreign_key: public Key {
public:
  enum fk_match_opt { FK_MATCH_UNDEF, FK_MATCH_FULL,
        FK_MATCH_PARTIAL, FK_MATCH_SIMPLE};
  enum fk_option { FK_OPTION_UNDEF, FK_OPTION_RESTRICT, FK_OPTION_CASCADE,
     FK_OPTION_SET_NULL, FK_OPTION_NO_ACTION, FK_OPTION_DEFAULT};
  Table_ident *ref_table;
  List<Key_part_spec> ref_columns;
  uint delete_opt, update_opt, match_opt;
  Foreign_key(const char *name_arg, List<Key_part_spec> &cols,
       Table_ident *table, List<Key_part_spec> &ref_cols,
       uint delete_opt_arg, uint update_opt_arg, uint match_opt_arg)
    :Key(FOREIGN_KEY, name_arg, &default_key_create_info, 0, cols),
    ref_table(table), ref_columns(ref_cols),
    delete_opt(delete_opt_arg), update_opt(update_opt_arg),
    match_opt(match_opt_arg)
  {}
  Foreign_key(const Foreign_key &rhs, MEM_ROOT *mem_root);
  virtual Key *clone(MEM_ROOT *mem_root) const
  { return new (mem_root) Foreign_key(*this, mem_root); }
};
typedef struct st_mysql_lock
{
  TABLE **table;
  uint table_count,lock_count;
  THR_LOCK_DATA **locks;
} MYSQL_LOCK;
class LEX_COLUMN : public Sql_alloc
{
public:
  String column;
  uint rights;
  LEX_COLUMN (const String& x,const uint& y ): column (x),rights (y) {}
};
#include "sql_lex.h"
class Table_ident;
class sql_exchange;
class LEX_COLUMN;
class sp_head;
class sp_name;
class sp_instr;
class sp_pcontext;
class st_alter_tablespace;
class partition_info;
class Event_parse_data;
enum enum_sql_command {
  SQLCOM_SELECT, SQLCOM_CREATE_TABLE, SQLCOM_CREATE_INDEX, SQLCOM_ALTER_TABLE,
  SQLCOM_UPDATE, SQLCOM_INSERT, SQLCOM_INSERT_SELECT,
  SQLCOM_DELETE, SQLCOM_TRUNCATE, SQLCOM_DROP_TABLE, SQLCOM_DROP_INDEX,
  SQLCOM_SHOW_DATABASES, SQLCOM_SHOW_TABLES, SQLCOM_SHOW_FIELDS,
  SQLCOM_SHOW_KEYS, SQLCOM_SHOW_VARIABLES, SQLCOM_SHOW_STATUS,
  SQLCOM_SHOW_ENGINE_LOGS, SQLCOM_SHOW_ENGINE_STATUS, SQLCOM_SHOW_ENGINE_MUTEX,
  SQLCOM_SHOW_PROCESSLIST, SQLCOM_SHOW_MASTER_STAT, SQLCOM_SHOW_SLAVE_STAT,
  SQLCOM_SHOW_GRANTS, SQLCOM_SHOW_CREATE, SQLCOM_SHOW_CHARSETS,
  SQLCOM_SHOW_COLLATIONS, SQLCOM_SHOW_CREATE_DB, SQLCOM_SHOW_TABLE_STATUS,
  SQLCOM_SHOW_TRIGGERS,
  SQLCOM_LOAD,SQLCOM_SET_OPTION,SQLCOM_LOCK_TABLES,SQLCOM_UNLOCK_TABLES,
  SQLCOM_GRANT,
  SQLCOM_CHANGE_DB, SQLCOM_CREATE_DB, SQLCOM_DROP_DB, SQLCOM_ALTER_DB,
  SQLCOM_REPAIR, SQLCOM_REPLACE, SQLCOM_REPLACE_SELECT,
  SQLCOM_CREATE_FUNCTION, SQLCOM_DROP_FUNCTION,
  SQLCOM_REVOKE,SQLCOM_OPTIMIZE, SQLCOM_CHECK,
  SQLCOM_ASSIGN_TO_KEYCACHE, SQLCOM_PRELOAD_KEYS,
  SQLCOM_FLUSH, SQLCOM_KILL, SQLCOM_ANALYZE,
  SQLCOM_ROLLBACK, SQLCOM_ROLLBACK_TO_SAVEPOINT,
  SQLCOM_COMMIT, SQLCOM_SAVEPOINT, SQLCOM_RELEASE_SAVEPOINT,
  SQLCOM_SLAVE_START, SQLCOM_SLAVE_STOP,
  SQLCOM_BEGIN, SQLCOM_LOAD_MASTER_TABLE, SQLCOM_CHANGE_MASTER,
  SQLCOM_RENAME_TABLE, SQLCOM_BACKUP_TABLE, SQLCOM_RESTORE_TABLE,
  SQLCOM_RESET, SQLCOM_PURGE, SQLCOM_PURGE_BEFORE, SQLCOM_SHOW_BINLOGS,
  SQLCOM_SHOW_OPEN_TABLES, SQLCOM_LOAD_MASTER_DATA,
  SQLCOM_HA_OPEN, SQLCOM_HA_CLOSE, SQLCOM_HA_READ,
  SQLCOM_SHOW_SLAVE_HOSTS, SQLCOM_DELETE_MULTI, SQLCOM_UPDATE_MULTI,
  SQLCOM_SHOW_BINLOG_EVENTS, SQLCOM_SHOW_NEW_MASTER, SQLCOM_DO,
  SQLCOM_SHOW_WARNS, SQLCOM_EMPTY_QUERY, SQLCOM_SHOW_ERRORS,
  SQLCOM_SHOW_COLUMN_TYPES, SQLCOM_SHOW_STORAGE_ENGINES, SQLCOM_SHOW_PRIVILEGES,
  SQLCOM_HELP, SQLCOM_CREATE_USER, SQLCOM_DROP_USER, SQLCOM_RENAME_USER,
  SQLCOM_REVOKE_ALL, SQLCOM_CHECKSUM,
  SQLCOM_CREATE_PROCEDURE, SQLCOM_CREATE_SPFUNCTION, SQLCOM_CALL,
  SQLCOM_DROP_PROCEDURE, SQLCOM_ALTER_PROCEDURE,SQLCOM_ALTER_FUNCTION,
  SQLCOM_SHOW_CREATE_PROC, SQLCOM_SHOW_CREATE_FUNC,
  SQLCOM_SHOW_STATUS_PROC, SQLCOM_SHOW_STATUS_FUNC,
  SQLCOM_PREPARE, SQLCOM_EXECUTE, SQLCOM_DEALLOCATE_PREPARE,
  SQLCOM_CREATE_VIEW, SQLCOM_DROP_VIEW,
  SQLCOM_CREATE_TRIGGER, SQLCOM_DROP_TRIGGER,
  SQLCOM_XA_START, SQLCOM_XA_END, SQLCOM_XA_PREPARE,
  SQLCOM_XA_COMMIT, SQLCOM_XA_ROLLBACK, SQLCOM_XA_RECOVER,
  SQLCOM_SHOW_PROC_CODE, SQLCOM_SHOW_FUNC_CODE,
  SQLCOM_ALTER_TABLESPACE,
  SQLCOM_INSTALL_PLUGIN, SQLCOM_UNINSTALL_PLUGIN,
  SQLCOM_SHOW_AUTHORS, SQLCOM_BINLOG_BASE64_EVENT,
  SQLCOM_SHOW_PLUGINS,
  SQLCOM_SHOW_CONTRIBUTORS,
  SQLCOM_CREATE_SERVER, SQLCOM_DROP_SERVER, SQLCOM_ALTER_SERVER,
  SQLCOM_CREATE_EVENT, SQLCOM_ALTER_EVENT, SQLCOM_DROP_EVENT,
  SQLCOM_SHOW_CREATE_EVENT, SQLCOM_SHOW_EVENTS,
  SQLCOM_SHOW_CREATE_TRIGGER,
  SQLCOM_ALTER_DB_UPGRADE,
  SQLCOM_SHOW_PROFILE, SQLCOM_SHOW_PROFILES,
  SQLCOM_END
};
class Delayed_insert;
class select_result;
class Time_zone;
struct system_variables
{
  ulong dynamic_variables_version;
  char* dynamic_variables_ptr;
  uint dynamic_variables_head;
  uint dynamic_variables_size;
  ulonglong myisam_max_extra_sort_file_size;
  ulonglong myisam_max_sort_file_size;
  ulonglong max_heap_table_size;
  ulonglong tmp_table_size;
  ulonglong long_query_time;
  ha_rows select_limit;
  ha_rows max_join_size;
  ulong auto_increment_increment, auto_increment_offset;
  ulong bulk_insert_buff_size;
  ulong join_buff_size;
  ulong max_allowed_packet;
  ulong max_error_count;
  ulong max_length_for_sort_data;
  ulong max_sort_length;
  ulong max_tmp_tables;
  ulong max_insert_delayed_threads;
  ulong min_examined_row_limit;
  ulong multi_range_count;
  ulong myisam_repair_threads;
  ulong myisam_sort_buff_size;
  ulong myisam_stats_method;
  ulong net_buffer_length;
  ulong net_interactive_timeout;
  ulong net_read_timeout;
  ulong net_retry_count;
  ulong net_wait_timeout;
  ulong net_write_timeout;
  ulong optimizer_prune_level;
  ulong optimizer_search_depth;
  ulong preload_buff_size;
  ulong profiling_history_size;
  ulong query_cache_type;
  ulong read_buff_size;
  ulong read_rnd_buff_size;
  ulong div_precincrement;
  ulong sortbuff_size;
  ulong thread_handling;
  ulong tx_isolation;
  ulong completion_type;
  ulong sql_mode;
  ulong max_sp_recursion_depth;
  ulong updatable_views_with_limit;
  ulong default_week_format;
  ulong max_seeks_for_key;
  ulong range_alloc_block_size;
  ulong query_alloc_block_size;
  ulong query_prealloc_size;
  ulong trans_alloc_block_size;
  ulong trans_prealloc_size;
  ulong log_warnings;
  ulong group_concat_max_len;
  ulong ndb_autoincrement_prefetch_sz;
  ulong ndb_index_stat_cache_entries;
  ulong ndb_index_stat_update_freq;
  ulong binlog_format;
  my_thread_id pseudo_thread_id;
  my_bool low_priority_updates;
  my_bool new_mode;
  my_bool old_mode;
  my_bool query_cache_wlock_invalidate;
  my_bool engine_condition_pushdown;
  my_bool keep_files_on_create;
  my_bool ndb_force_send;
  my_bool ndb_use_copying_alter_table;
  my_bool ndb_use_exact_count;
  my_bool ndb_use_transactions;
  my_bool ndb_index_stat_enable;
  my_bool old_alter_table;
  my_bool old_passwords;
  plugin_ref table_plugin;
  CHARSET_INFO *character_set_filesystem;
  CHARSET_INFO *character_set_client;
  CHARSET_INFO *character_set_results;
  CHARSET_INFO *collation_server;
  CHARSET_INFO *collation_database;
  CHARSET_INFO *collation_connection;
  MY_LOCALE *lc_time_names;
  Time_zone *time_zone;
  DATE_TIME_FORMAT *date_format;
  DATE_TIME_FORMAT *datetime_format;
  DATE_TIME_FORMAT *time_format;
  my_bool sysdate_is_now;
};
typedef struct system_status_var
{
  ulonglong bytes_received;
  ulonglong bytes_sent;
  ulong com_other;
  ulong com_stat[(uint) SQLCOM_END];
  ulong created_tmp_disk_tables;
  ulong created_tmp_tables;
  ulong ha_commit_count;
  ulong ha_delete_count;
  ulong ha_read_first_count;
  ulong ha_read_last_count;
  ulong ha_read_key_count;
  ulong ha_read_next_count;
  ulong ha_read_prev_count;
  ulong ha_read_rnd_count;
  ulong ha_read_rnd_next_count;
  ulong ha_rollback_count;
  ulong ha_update_count;
  ulong ha_write_count;
  ulong ha_prepare_count;
  ulong ha_discover_count;
  ulong ha_savepoint_count;
  ulong ha_savepoint_rollback_count;
  ulong key_blocks_changed;
  ulong key_blocks_used;
  ulong key_cache_r_requests;
  ulong key_cache_read;
  ulong key_cache_w_requests;
  ulong key_cache_write;
  ulong net_big_packet_count;
  ulong opened_tables;
  ulong opened_shares;
  ulong select_full_join_count;
  ulong select_full_range_join_count;
  ulong select_range_count;
  ulong select_range_check_count;
  ulong select_scan_count;
  ulong long_query_count;
  ulong filesort_merge_passes;
  ulong filesort_range_count;
  ulong filesort_rows;
  ulong filesort_scan_count;
  ulong com_stmt_prepare;
  ulong com_stmt_reprepare;
  ulong com_stmt_execute;
  ulong com_stmt_send_long_data;
  ulong com_stmt_fetch;
  ulong com_stmt_reset;
  ulong com_stmt_close;
  double last_query_cost;
} STATUS_VAR;
void mark_transaction_to_rollback(THD *thd, In_C_you_should_use_my_bool_instead() all);
#include "sql_acl.h"
#include "slave.h"
#include "log.h"
#include "my_list.h"
#include "rpl_filter.h"
#include "mysql.h"
#include "mysql_version.h"
#include "mysql_com.h"
#include "mysql_time.h"
#include "my_list.h"
extern unsigned int mysql_port;
extern char *mysql_unix_port;
typedef struct st_mysql_field {
  char *name;
  char *org_name;
  char *table;
  char *org_table;
  char *db;
  char *catalog;
  char *def;
  unsigned long length;
  unsigned long max_length;
  unsigned int name_length;
  unsigned int org_name_length;
  unsigned int table_length;
  unsigned int org_table_length;
  unsigned int db_length;
  unsigned int catalog_length;
  unsigned int def_length;
  unsigned int flags;
  unsigned int decimals;
  unsigned int charsetnr;
  enum enum_field_types type;
  void *extension;
} MYSQL_FIELD;
typedef char **MYSQL_ROW;
typedef unsigned int MYSQL_FIELD_OFFSET;
#include "typelib.h"
typedef struct st_mysql_rows {
  struct st_mysql_rows *next;
  MYSQL_ROW data;
  unsigned long length;
} MYSQL_ROWS;
typedef MYSQL_ROWS *MYSQL_ROW_OFFSET;
#include "my_alloc.h"
typedef struct embedded_query_result EMBEDDED_QUERY_RESULT;
typedef struct st_mysql_data {
  MYSQL_ROWS *data;
  struct embedded_query_result *embedded_info;
  MEM_ROOT alloc;
  my_ulonglong rows;
  unsigned int fields;
  void *extension;
} MYSQL_DATA;
enum mysql_option
{
  MYSQL_OPT_CONNECT_TIMEOUT, MYSQL_OPT_COMPRESS, MYSQL_OPT_NAMED_PIPE,
  MYSQL_INIT_COMMAND, MYSQL_READ_DEFAULT_FILE, MYSQL_READ_DEFAULT_GROUP,
  MYSQL_SET_CHARSET_DIR, MYSQL_SET_CHARSET_NAME, MYSQL_OPT_LOCAL_INFILE,
  MYSQL_OPT_PROTOCOL, MYSQL_SHARED_MEMORY_BASE_NAME, MYSQL_OPT_READ_TIMEOUT,
  MYSQL_OPT_WRITE_TIMEOUT, MYSQL_OPT_USE_RESULT,
  MYSQL_OPT_USE_REMOTE_CONNECTION, MYSQL_OPT_USE_EMBEDDED_CONNECTION,
  MYSQL_OPT_GUESS_CONNECTION, MYSQL_SET_CLIENT_IP, MYSQL_SECURE_AUTH,
  MYSQL_REPORT_DATA_TRUNCATION, MYSQL_OPT_RECONNECT,
  MYSQL_OPT_SSL_VERIFY_SERVER_CERT
};
struct st_mysql_options {
  unsigned int connect_timeout, read_timeout, write_timeout;
  unsigned int port, protocol;
  unsigned long client_flag;
  char *host,*user,*password,*unix_socket,*db;
  struct st_dynamic_array *init_commands;
  char *my_cnf_file,*my_cnf_group, *charset_dir, *charset_name;
  char *ssl_key;
  char *ssl_cert;
  char *ssl_ca;
  char *ssl_capath;
  char *ssl_cipher;
  char *shared_memory_base_name;
  unsigned long max_allowed_packet;
  my_bool use_ssl;
  my_bool compress,named_pipe;
  my_bool rpl_probe;
  my_bool rpl_parse;
  my_bool no_master_reads;
  my_bool separate_thread;
  enum mysql_option methods_to_use;
  char *client_ip;
  my_bool secure_auth;
  my_bool report_data_truncation;
  int (*local_infile_init)(void **, const char *, void *);
  int (*local_infile_read)(void *, char *, unsigned int);
  void (*local_infile_end)(void *);
  int (*local_infile_error)(void *, char *, unsigned int);
  void *local_infile_userdata;
  void *extension;
};
enum mysql_status
{
  MYSQL_STATUS_READY,MYSQL_STATUS_GET_RESULT,MYSQL_STATUS_USE_RESULT
};
enum mysql_protocol_type
{
  MYSQL_PROTOCOL_DEFAULT, MYSQL_PROTOCOL_TCP, MYSQL_PROTOCOL_SOCKET,
  MYSQL_PROTOCOL_PIPE, MYSQL_PROTOCOL_MEMORY
};
enum mysql_rpl_type
{
  MYSQL_RPL_MASTER, MYSQL_RPL_SLAVE, MYSQL_RPL_ADMIN
};
typedef struct character_set
{
  unsigned int number;
  unsigned int state;
  const char *csname;
  const char *name;
  const char *comment;
  const char *dir;
  unsigned int mbminlen;
  unsigned int mbmaxlen;
} MY_CHARSET_INFO;
struct st_mysql_methods;
struct st_mysql_stmt;
typedef struct st_mysql
{
  NET net;
  unsigned char *connector_fd;
  char *host,*user,*passwd,*unix_socket,*server_version,*host_info;
  char *info, *db;
  struct charset_info_st *charset;
  MYSQL_FIELD *fields;
  MEM_ROOT field_alloc;
  my_ulonglong affected_rows;
  my_ulonglong insert_id;
  my_ulonglong extra_info;
  unsigned long thread_id;
  unsigned long packet_length;
  unsigned int port;
  unsigned long client_flag,server_capabilities;
  unsigned int protocol_version;
  unsigned int field_count;
  unsigned int server_status;
  unsigned int server_language;
  unsigned int warning_count;
  struct st_mysql_options options;
  enum mysql_status status;
  my_bool free_me;
  my_bool reconnect;
  char scramble[20 +1];
  my_bool rpl_pivot;
  struct st_mysql* master, *next_slave;
  struct st_mysql* last_used_slave;
  struct st_mysql* last_used_con;
  LIST *stmts;
  const struct st_mysql_methods *methods;
  void *thd;
  my_bool *unbuffered_fetch_owner;
  char *info_buffer;
  void *extension;
} MYSQL;
typedef struct st_mysql_res {
  my_ulonglong row_count;
  MYSQL_FIELD *fields;
  MYSQL_DATA *data;
  MYSQL_ROWS *data_cursor;
  unsigned long *lengths;
  MYSQL *handle;
  const struct st_mysql_methods *methods;
  MYSQL_ROW row;
  MYSQL_ROW current_row;
  MEM_ROOT field_alloc;
  unsigned int field_count, current_field;
  my_bool eof;
  my_bool unbuffered_fetch_cancelled;
  void *extension;
} MYSQL_RES;
typedef struct st_mysql_manager
{
  NET net;
  char *host, *user, *passwd;
  char *net_buf, *net_buf_pos, *net_data_end;
  unsigned int port;
  int cmd_status;
  int last_errno;
  int net_buf_size;
  my_bool free_me;
  my_bool eof;
  char last_error[256];
  void *extension;
} MYSQL_MANAGER;
typedef struct st_mysql_parameters
{
  unsigned long *p_max_allowed_packet;
  unsigned long *p_net_buffer_length;
  void *extension;
} MYSQL_PARAMETERS;
int mysql_server_init(int argc, char **argv, char **groups);
void mysql_server_end(void);
MYSQL_PARAMETERS * mysql_get_parameters(void);
my_bool mysql_thread_init(void);
void mysql_thread_end(void);
my_ulonglong mysql_num_rows(MYSQL_RES *res);
unsigned int mysql_num_fields(MYSQL_RES *res);
my_bool mysql_eof(MYSQL_RES *res);
MYSQL_FIELD * mysql_fetch_field_direct(MYSQL_RES *res,
           unsigned int fieldnr);
MYSQL_FIELD * mysql_fetch_fields(MYSQL_RES *res);
MYSQL_ROW_OFFSET mysql_row_tell(MYSQL_RES *res);
MYSQL_FIELD_OFFSET mysql_field_tell(MYSQL_RES *res);
unsigned int mysql_field_count(MYSQL *mysql);
my_ulonglong mysql_affected_rows(MYSQL *mysql);
my_ulonglong mysql_insert_id(MYSQL *mysql);
unsigned int mysql_errno(MYSQL *mysql);
const char * mysql_error(MYSQL *mysql);
const char * mysql_sqlstate(MYSQL *mysql);
unsigned int mysql_warning_count(MYSQL *mysql);
const char * mysql_info(MYSQL *mysql);
unsigned long mysql_thread_id(MYSQL *mysql);
const char * mysql_character_set_name(MYSQL *mysql);
int mysql_set_character_set(MYSQL *mysql, const char *csname);
MYSQL * mysql_init(MYSQL *mysql);
my_bool mysql_ssl_set(MYSQL *mysql, const char *key,
          const char *cert, const char *ca,
          const char *capath, const char *cipher);
const char * mysql_get_ssl_cipher(MYSQL *mysql);
my_bool mysql_change_user(MYSQL *mysql, const char *user,
       const char *passwd, const char *db);
MYSQL * mysql_real_connect(MYSQL *mysql, const char *host,
        const char *user,
        const char *passwd,
        const char *db,
        unsigned int port,
        const char *unix_socket,
        unsigned long clientflag);
int mysql_select_db(MYSQL *mysql, const char *db);
int mysql_query(MYSQL *mysql, const char *q);
int mysql_send_query(MYSQL *mysql, const char *q,
      unsigned long length);
int mysql_real_query(MYSQL *mysql, const char *q,
     unsigned long length);
MYSQL_RES * mysql_store_result(MYSQL *mysql);
MYSQL_RES * mysql_use_result(MYSQL *mysql);
my_bool mysql_master_query(MYSQL *mysql, const char *q,
        unsigned long length);
my_bool mysql_master_send_query(MYSQL *mysql, const char *q,
      unsigned long length);
my_bool mysql_slave_query(MYSQL *mysql, const char *q,
       unsigned long length);
my_bool mysql_slave_send_query(MYSQL *mysql, const char *q,
            unsigned long length);
void mysql_get_character_set_info(MYSQL *mysql,
                           MY_CHARSET_INFO *charset);
void
mysql_set_local_infile_handler(MYSQL *mysql,
                               int (*local_infile_init)(void **, const char *,
                            void *),
                               int (*local_infile_read)(void *, char *,
       unsigned int),
                               void (*local_infile_end)(void *),
                               int (*local_infile_error)(void *, char*,
        unsigned int),
                               void *);
void
mysql_set_local_infile_default(MYSQL *mysql);
void mysql_enable_rpl_parse(MYSQL* mysql);
void mysql_disable_rpl_parse(MYSQL* mysql);
int mysql_rpl_parse_enabled(MYSQL* mysql);
void mysql_enable_reads_from_master(MYSQL* mysql);
void mysql_disable_reads_from_master(MYSQL* mysql);
my_bool mysql_reads_from_master_enabled(MYSQL* mysql);
enum mysql_rpl_type mysql_rpl_query_type(const char* q, int len);
my_bool mysql_rpl_probe(MYSQL* mysql);
int mysql_set_master(MYSQL* mysql, const char* host,
      unsigned int port,
      const char* user,
      const char* passwd);
int mysql_add_slave(MYSQL* mysql, const char* host,
     unsigned int port,
     const char* user,
     const char* passwd);
int mysql_shutdown(MYSQL *mysql,
                                       enum mysql_enum_shutdown_level
                                       shutdown_level);
int mysql_dump_debug_info(MYSQL *mysql);
int mysql_refresh(MYSQL *mysql,
         unsigned int refresh_options);
int mysql_kill(MYSQL *mysql,unsigned long pid);
int mysql_set_server_option(MYSQL *mysql,
      enum enum_mysql_set_option
      option);
int mysql_ping(MYSQL *mysql);
const char * mysql_stat(MYSQL *mysql);
const char * mysql_get_server_info(MYSQL *mysql);
const char * mysql_get_client_info(void);
unsigned long mysql_get_client_version(void);
const char * mysql_get_host_info(MYSQL *mysql);
unsigned long mysql_get_server_version(MYSQL *mysql);
unsigned int mysql_get_proto_info(MYSQL *mysql);
MYSQL_RES * mysql_list_dbs(MYSQL *mysql,const char *wild);
MYSQL_RES * mysql_list_tables(MYSQL *mysql,const char *wild);
MYSQL_RES * mysql_list_processes(MYSQL *mysql);
int mysql_options(MYSQL *mysql,enum mysql_option option,
          const void *arg);
void mysql_free_result(MYSQL_RES *result);
void mysql_data_seek(MYSQL_RES *result,
     my_ulonglong offset);
MYSQL_ROW_OFFSET mysql_row_seek(MYSQL_RES *result,
      MYSQL_ROW_OFFSET offset);
MYSQL_FIELD_OFFSET mysql_field_seek(MYSQL_RES *result,
        MYSQL_FIELD_OFFSET offset);
MYSQL_ROW mysql_fetch_row(MYSQL_RES *result);
unsigned long * mysql_fetch_lengths(MYSQL_RES *result);
MYSQL_FIELD * mysql_fetch_field(MYSQL_RES *result);
MYSQL_RES * mysql_list_fields(MYSQL *mysql, const char *table,
       const char *wild);
unsigned long mysql_escape_string(char *to,const char *from,
         unsigned long from_length);
unsigned long mysql_hex_string(char *to,const char *from,
                                         unsigned long from_length);
unsigned long mysql_real_escape_string(MYSQL *mysql,
            char *to,const char *from,
            unsigned long length);
void mysql_debug(const char *debug);
void myodbc_remove_escape(MYSQL *mysql,char *name);
unsigned int mysql_thread_safe(void);
my_bool mysql_embedded(void);
MYSQL_MANAGER* mysql_manager_init(MYSQL_MANAGER* con);
MYSQL_MANAGER* mysql_manager_connect(MYSQL_MANAGER* con,
           const char* host,
           const char* user,
           const char* passwd,
           unsigned int port);
void mysql_manager_close(MYSQL_MANAGER* con);
int mysql_manager_command(MYSQL_MANAGER* con,
      const char* cmd, int cmd_len);
int mysql_manager_fetch_line(MYSQL_MANAGER* con,
        char* res_buf,
       int res_buf_size);
my_bool mysql_read_query_result(MYSQL *mysql);
enum enum_mysql_stmt_state
{
  MYSQL_STMT_INIT_DONE= 1, MYSQL_STMT_PREPARE_DONE, MYSQL_STMT_EXECUTE_DONE,
  MYSQL_STMT_FETCH_DONE
};
typedef struct st_mysql_bind
{
  unsigned long *length;
  my_bool *is_null;
  void *buffer;
  my_bool *error;
  unsigned char *row_ptr;
  void (*store_param_func)(NET *net, struct st_mysql_bind *param);
  void (*fetch_result)(struct st_mysql_bind *, MYSQL_FIELD *,
                       unsigned char **row);
  void (*skip_result)(struct st_mysql_bind *, MYSQL_FIELD *,
        unsigned char **row);
  unsigned long buffer_length;
  unsigned long offset;
  unsigned long length_value;
  unsigned int param_number;
  unsigned int pack_length;
  enum enum_field_types buffer_type;
  my_bool error_value;
  my_bool is_unsigned;
  my_bool long_data_used;
  my_bool is_null_value;
  void *extension;
} MYSQL_BIND;
typedef struct st_mysql_stmt
{
  MEM_ROOT mem_root;
  LIST list;
  MYSQL *mysql;
  MYSQL_BIND *params;
  MYSQL_BIND *bind;
  MYSQL_FIELD *fields;
  MYSQL_DATA result;
  MYSQL_ROWS *data_cursor;
  int (*read_row_func)(struct st_mysql_stmt *stmt,
                                  unsigned char **row);
  my_ulonglong affected_rows;
  my_ulonglong insert_id;
  unsigned long stmt_id;
  unsigned long flags;
  unsigned long prefetch_rows;
  unsigned int server_status;
  unsigned int last_errno;
  unsigned int param_count;
  unsigned int field_count;
  enum enum_mysql_stmt_state state;
  char last_error[512];
  char sqlstate[5 +1];
  my_bool send_types_to_server;
  my_bool bind_param_done;
  unsigned char bind_result_done;
  my_bool unbuffered_fetch_cancelled;
  my_bool update_max_length;
  void *extension;
} MYSQL_STMT;
enum enum_stmt_attr_type
{
  STMT_ATTR_UPDATE_MAX_LENGTH,
  STMT_ATTR_CURSOR_TYPE,
  STMT_ATTR_PREFETCH_ROWS
};
typedef struct st_mysql_methods
{
  my_bool (*read_query_result)(MYSQL *mysql);
  my_bool (*advanced_command)(MYSQL *mysql,
         enum enum_server_command command,
         const unsigned char *header,
         unsigned long header_length,
         const unsigned char *arg,
         unsigned long arg_length,
         my_bool skip_check,
                              MYSQL_STMT *stmt);
  MYSQL_DATA *(*read_rows)(MYSQL *mysql,MYSQL_FIELD *mysql_fields,
      unsigned int fields);
  MYSQL_RES * (*use_result)(MYSQL *mysql);
  void (*fetch_lengths)(unsigned long *to,
   MYSQL_ROW column, unsigned int field_count);
  void (*flush_use_result)(MYSQL *mysql);
  MYSQL_FIELD * (*list_fields)(MYSQL *mysql);
  my_bool (*read_prepare_result)(MYSQL *mysql, MYSQL_STMT *stmt);
  int (*stmt_execute)(MYSQL_STMT *stmt);
  int (*read_binary_rows)(MYSQL_STMT *stmt);
  int (*unbuffered_fetch)(MYSQL *mysql, char **row);
  void (*free_embedded_thd)(MYSQL *mysql);
  const char *(*read_statistics)(MYSQL *mysql);
  my_bool (*next_result)(MYSQL *mysql);
  int (*read_change_user_result)(MYSQL *mysql, char *buff, const char *passwd);
  int (*read_rows_from_cursor)(MYSQL_STMT *stmt);
} MYSQL_METHODS;
MYSQL_STMT * mysql_stmt_init(MYSQL *mysql);
int mysql_stmt_prepare(MYSQL_STMT *stmt, const char *query,
                               unsigned long length);
int mysql_stmt_execute(MYSQL_STMT *stmt);
int mysql_stmt_fetch(MYSQL_STMT *stmt);
int mysql_stmt_fetch_column(MYSQL_STMT *stmt, MYSQL_BIND *bind_arg,
                                    unsigned int column,
                                    unsigned long offset);
int mysql_stmt_store_result(MYSQL_STMT *stmt);
unsigned long mysql_stmt_param_count(MYSQL_STMT * stmt);
my_bool mysql_stmt_attr_set(MYSQL_STMT *stmt,
                                    enum enum_stmt_attr_type attr_type,
                                    const void *attr);
my_bool mysql_stmt_attr_get(MYSQL_STMT *stmt,
                                    enum enum_stmt_attr_type attr_type,
                                    void *attr);
my_bool mysql_stmt_bind_param(MYSQL_STMT * stmt, MYSQL_BIND * bnd);
my_bool mysql_stmt_bind_result(MYSQL_STMT * stmt, MYSQL_BIND * bnd);
my_bool mysql_stmt_close(MYSQL_STMT * stmt);
my_bool mysql_stmt_reset(MYSQL_STMT * stmt);
my_bool mysql_stmt_free_result(MYSQL_STMT *stmt);
my_bool mysql_stmt_send_long_data(MYSQL_STMT *stmt,
                                          unsigned int param_number,
                                          const char *data,
                                          unsigned long length);
MYSQL_RES * mysql_stmt_result_metadata(MYSQL_STMT *stmt);
MYSQL_RES * mysql_stmt_param_metadata(MYSQL_STMT *stmt);
unsigned int mysql_stmt_errno(MYSQL_STMT * stmt);
const char * mysql_stmt_error(MYSQL_STMT * stmt);
const char * mysql_stmt_sqlstate(MYSQL_STMT * stmt);
MYSQL_ROW_OFFSET mysql_stmt_row_seek(MYSQL_STMT *stmt,
                                             MYSQL_ROW_OFFSET offset);
MYSQL_ROW_OFFSET mysql_stmt_row_tell(MYSQL_STMT *stmt);
void mysql_stmt_data_seek(MYSQL_STMT *stmt, my_ulonglong offset);
my_ulonglong mysql_stmt_num_rows(MYSQL_STMT *stmt);
my_ulonglong mysql_stmt_affected_rows(MYSQL_STMT *stmt);
my_ulonglong mysql_stmt_insert_id(MYSQL_STMT *stmt);
unsigned int mysql_stmt_field_count(MYSQL_STMT *stmt);
my_bool mysql_commit(MYSQL * mysql);
my_bool mysql_rollback(MYSQL * mysql);
my_bool mysql_autocommit(MYSQL * mysql, my_bool auto_mode);
my_bool mysql_more_results(MYSQL *mysql);
int mysql_next_result(MYSQL *mysql);
void mysql_close(MYSQL *sock);
typedef struct st_table_rule_ent
{
  char* db;
  char* tbl_name;
  uint key_len;
} TABLE_RULE_ENT;
class Rpl_filter
{
public:
  Rpl_filter();
  ~Rpl_filter();
  Rpl_filter(Rpl_filter const&);
  Rpl_filter& operator=(Rpl_filter const&);
  In_C_you_should_use_my_bool_instead() tables_ok(const char* db, TABLE_LIST* tables);
  In_C_you_should_use_my_bool_instead() db_ok(const char* db);
  In_C_you_should_use_my_bool_instead() db_ok_with_wild_table(const char *db);
  In_C_you_should_use_my_bool_instead() is_on();
  int add_do_table(const char* table_spec);
  int add_ignore_table(const char* table_spec);
  int add_wild_do_table(const char* table_spec);
  int add_wild_ignore_table(const char* table_spec);
  void add_do_db(const char* db_spec);
  void add_ignore_db(const char* db_spec);
  void add_db_rewrite(const char* from_db, const char* to_db);
  void get_do_table(String* str);
  void get_ignore_table(String* str);
  void get_wild_do_table(String* str);
  void get_wild_ignore_table(String* str);
  const char* get_rewrite_db(const char* db, size_t *new_len);
  I_List<i_string>* get_do_db();
  I_List<i_string>* get_ignore_db();
private:
  In_C_you_should_use_my_bool_instead() table_rules_on;
  void init_table_rule_hash(HASH* h, In_C_you_should_use_my_bool_instead()* h_inited);
  void init_table_rule_array(DYNAMIC_ARRAY* a, In_C_you_should_use_my_bool_instead()* a_inited);
  int add_table_rule(HASH* h, const char* table_spec);
  int add_wild_table_rule(DYNAMIC_ARRAY* a, const char* table_spec);
  void free_string_array(DYNAMIC_ARRAY *a);
  void table_rule_ent_hash_to_str(String* s, HASH* h, In_C_you_should_use_my_bool_instead() inited);
  void table_rule_ent_dynamic_array_to_str(String* s, DYNAMIC_ARRAY* a,
                                           In_C_you_should_use_my_bool_instead() inited);
  TABLE_RULE_ENT* find_wild(DYNAMIC_ARRAY *a, const char* key, int len);
  HASH do_table;
  HASH ignore_table;
  DYNAMIC_ARRAY wild_do_table;
  DYNAMIC_ARRAY wild_ignore_table;
  In_C_you_should_use_my_bool_instead() do_table_inited;
  In_C_you_should_use_my_bool_instead() ignore_table_inited;
  In_C_you_should_use_my_bool_instead() wild_do_table_inited;
  In_C_you_should_use_my_bool_instead() wild_ignore_table_inited;
  I_List<i_string> do_db;
  I_List<i_string> ignore_db;
  I_List<i_string_pair> rewrite_db;
};
extern Rpl_filter *rpl_filter;
extern Rpl_filter *binlog_filter;
#include "rpl_tblmap.h"
class Relay_log_info;
class Master_info;
extern ulong master_retry_count;
extern MY_BITMAP slave_error_mask;
extern In_C_you_should_use_my_bool_instead() use_slave_mask;
extern char *slave_load_tmpdir;
extern char *master_info_file, *relay_log_info_file;
extern char *opt_relay_logname, *opt_relaylog_index_name;
extern my_bool opt_skip_slave_start, opt_reckless_slave;
extern my_bool opt_log_slave_updates;
extern ulonglong relay_log_space_limit;
int init_slave();
void init_slave_skip_errors(const char* arg);
In_C_you_should_use_my_bool_instead() flush_relay_log_info(Relay_log_info* rli);
int register_slave_on_master(MYSQL* mysql);
int terminate_slave_threads(Master_info* mi, int thread_mask,
        In_C_you_should_use_my_bool_instead() skip_lock = 0);
int start_slave_threads(In_C_you_should_use_my_bool_instead() need_slave_mutex, In_C_you_should_use_my_bool_instead() wait_for_start,
   Master_info* mi, const char* master_info_fname,
   const char* slave_info_fname, int thread_mask);
int start_slave_thread(pthread_handler h_func, pthread_mutex_t* start_lock,
         pthread_mutex_t *cond_lock,
         pthread_cond_t* start_cond,
         volatile uint *slave_running,
         volatile ulong *slave_run_id,
         Master_info* mi,
                       In_C_you_should_use_my_bool_instead() high_priority);
int mysql_table_dump(THD* thd, const char* db,
       const char* tbl_name, int fd = -1);
int fetch_master_table(THD* thd, const char* db_name, const char* table_name,
         Master_info* mi, MYSQL* mysql, In_C_you_should_use_my_bool_instead() overwrite);
In_C_you_should_use_my_bool_instead() show_master_info(THD* thd, Master_info* mi);
In_C_you_should_use_my_bool_instead() show_binlog_info(THD* thd);
In_C_you_should_use_my_bool_instead() rpl_master_has_bug(Relay_log_info *rli, uint bug_id, In_C_you_should_use_my_bool_instead() report=(1));
In_C_you_should_use_my_bool_instead() rpl_master_erroneous_autoinc(THD* thd);
const char *print_slave_db_safe(const char *db);
int check_expected_error(THD* thd, Relay_log_info const *rli, int error_code);
void skip_load_data_infile(NET* net);
void end_slave();
void clear_until_condition(Relay_log_info* rli);
void clear_slave_error(Relay_log_info* rli);
void end_relay_log_info(Relay_log_info* rli);
void lock_slave_threads(Master_info* mi);
void unlock_slave_threads(Master_info* mi);
void init_thread_mask(int* mask,Master_info* mi,In_C_you_should_use_my_bool_instead() inverse);
int init_relay_log_pos(Relay_log_info* rli,const char* log,ulonglong pos,
         In_C_you_should_use_my_bool_instead() need_data_lock, const char** errmsg,
                       In_C_you_should_use_my_bool_instead() look_for_description_event);
int purge_relay_logs(Relay_log_info* rli, THD *thd, In_C_you_should_use_my_bool_instead() just_reset,
       const char** errmsg);
void set_slave_thread_options(THD* thd);
void set_slave_thread_default_charset(THD *thd, Relay_log_info const *rli);
void rotate_relay_log(Master_info* mi);
int apply_event_and_update_pos(Log_event* ev, THD* thd, Relay_log_info* rli,
                               In_C_you_should_use_my_bool_instead() skip);
 void * handle_slave_io(void *arg);
 void * handle_slave_sql(void *arg);
extern In_C_you_should_use_my_bool_instead() volatile abort_loop;
extern Master_info main_mi, *active_mi;
extern LIST master_list;
extern my_bool replicate_same_server_id;
extern int disconnect_slave_event_count, abort_slave_event_count ;
extern uint master_port, master_connect_retry, report_port;
extern char * master_user, *master_password, *master_host;
extern char *master_info_file, *relay_log_info_file, *report_user;
extern char *report_host, *report_password;
extern my_bool master_ssl;
extern char *master_ssl_ca, *master_ssl_capath, *master_ssl_cert;
extern char *master_ssl_cipher, *master_ssl_key;
extern I_List<THD> threads;
enum mysql_db_table_field
{
  MYSQL_DB_FIELD_HOST = 0,
  MYSQL_DB_FIELD_DB,
  MYSQL_DB_FIELD_USER,
  MYSQL_DB_FIELD_SELECT_PRIV,
  MYSQL_DB_FIELD_INSERT_PRIV,
  MYSQL_DB_FIELD_UPDATE_PRIV,
  MYSQL_DB_FIELD_DELETE_PRIV,
  MYSQL_DB_FIELD_CREATE_PRIV,
  MYSQL_DB_FIELD_DROP_PRIV,
  MYSQL_DB_FIELD_GRANT_PRIV,
  MYSQL_DB_FIELD_REFERENCES_PRIV,
  MYSQL_DB_FIELD_INDEX_PRIV,
  MYSQL_DB_FIELD_ALTER_PRIV,
  MYSQL_DB_FIELD_CREATE_TMP_TABLE_PRIV,
  MYSQL_DB_FIELD_LOCK_TABLES_PRIV,
  MYSQL_DB_FIELD_CREATE_VIEW_PRIV,
  MYSQL_DB_FIELD_SHOW_VIEW_PRIV,
  MYSQL_DB_FIELD_CREATE_ROUTINE_PRIV,
  MYSQL_DB_FIELD_ALTER_ROUTINE_PRIV,
  MYSQL_DB_FIELD_EXECUTE_PRIV,
  MYSQL_DB_FIELD_EVENT_PRIV,
  MYSQL_DB_FIELD_TRIGGER_PRIV,
  MYSQL_DB_FIELD_COUNT
};
extern TABLE_FIELD_W_TYPE mysql_db_table_fields[];
extern time_t mysql_db_table_last_check;
struct acl_host_and_ip
{
  char *hostname;
  long ip,ip_mask;
};
class ACL_ACCESS {
public:
  ulong sort;
  ulong access;
};
class ACL_HOST :public ACL_ACCESS
{
public:
  acl_host_and_ip host;
  char *db;
};
class ACL_USER :public ACL_ACCESS
{
public:
  acl_host_and_ip host;
  uint hostname_length;
  USER_RESOURCES user_resource;
  char *user;
  uint8 salt[20 +1];
  uint8 salt_len;
  enum SSL_type ssl_type;
  const char *ssl_cipher, *x509_issuer, *x509_subject;
};
class ACL_DB :public ACL_ACCESS
{
public:
  acl_host_and_ip host;
  char *user,*db;
};
In_C_you_should_use_my_bool_instead() hostname_requires_resolving(const char *hostname);
my_bool acl_init(In_C_you_should_use_my_bool_instead() dont_read_acl_tables);
my_bool acl_reload(THD *thd);
void acl_free(In_C_you_should_use_my_bool_instead() end=0);
ulong acl_get(const char *host, const char *ip,
       const char *user, const char *db, my_bool db_is_pattern);
int acl_getroot(THD *thd, USER_RESOURCES *mqh, const char *passwd,
                uint passwd_len);
In_C_you_should_use_my_bool_instead() acl_getroot_no_password(Security_context *sctx, char *user, char *host,
                             char *ip, char *db);
In_C_you_should_use_my_bool_instead() acl_check_host(const char *host, const char *ip);
int check_change_password(THD *thd, const char *host, const char *user,
                           char *password, uint password_len);
In_C_you_should_use_my_bool_instead() change_password(THD *thd, const char *host, const char *user,
       char *password);
In_C_you_should_use_my_bool_instead() mysql_grant(THD *thd, const char *db, List <LEX_USER> &user_list,
                 ulong rights, In_C_you_should_use_my_bool_instead() revoke);
int mysql_table_grant(THD *thd, TABLE_LIST *table, List <LEX_USER> &user_list,
                       List <LEX_COLUMN> &column_list, ulong rights,
                       In_C_you_should_use_my_bool_instead() revoke);
In_C_you_should_use_my_bool_instead() mysql_routine_grant(THD *thd, TABLE_LIST *table, In_C_you_should_use_my_bool_instead() is_proc,
    List <LEX_USER> &user_list, ulong rights,
    In_C_you_should_use_my_bool_instead() revoke, In_C_you_should_use_my_bool_instead() no_error);
my_bool grant_init();
void grant_free(void);
my_bool grant_reload(THD *thd);
In_C_you_should_use_my_bool_instead() check_grant(THD *thd, ulong want_access, TABLE_LIST *tables,
   uint show_command, uint number, In_C_you_should_use_my_bool_instead() dont_print_error);
In_C_you_should_use_my_bool_instead() check_grant_column (THD *thd, GRANT_INFO *grant,
    const char *db_name, const char *table_name,
    const char *name, uint length, Security_context *sctx);
In_C_you_should_use_my_bool_instead() check_column_grant_in_table_ref(THD *thd, TABLE_LIST * table_ref,
                                     const char *name, uint length);
In_C_you_should_use_my_bool_instead() check_grant_all_columns(THD *thd, ulong want_access,
                             Field_iterator_table_ref *fields);
In_C_you_should_use_my_bool_instead() check_grant_routine(THD *thd, ulong want_access,
    TABLE_LIST *procs, In_C_you_should_use_my_bool_instead() is_proc, In_C_you_should_use_my_bool_instead() no_error);
In_C_you_should_use_my_bool_instead() check_grant_db(THD *thd,const char *db);
ulong get_table_grant(THD *thd, TABLE_LIST *table);
ulong get_column_grant(THD *thd, GRANT_INFO *grant,
                       const char *db_name, const char *table_name,
                       const char *field_name);
In_C_you_should_use_my_bool_instead() mysql_show_grants(THD *thd, LEX_USER *user);
void get_privilege_desc(char *to, uint max_length, ulong access);
void get_mqh(const char *user, const char *host, USER_CONN *uc);
In_C_you_should_use_my_bool_instead() mysql_create_user(THD *thd, List <LEX_USER> &list);
In_C_you_should_use_my_bool_instead() mysql_drop_user(THD *thd, List <LEX_USER> &list);
In_C_you_should_use_my_bool_instead() mysql_rename_user(THD *thd, List <LEX_USER> &list);
In_C_you_should_use_my_bool_instead() mysql_revoke_all(THD *thd, List <LEX_USER> &list);
void fill_effective_table_privileges(THD *thd, GRANT_INFO *grant,
                                     const char *db, const char *table);
In_C_you_should_use_my_bool_instead() sp_revoke_privileges(THD *thd, const char *sp_db, const char *sp_name,
                          In_C_you_should_use_my_bool_instead() is_proc);
int sp_grant_privileges(THD *thd, const char *sp_db, const char *sp_name,
                         In_C_you_should_use_my_bool_instead() is_proc);
In_C_you_should_use_my_bool_instead() check_routine_level_acl(THD *thd, const char *db, const char *name,
                             In_C_you_should_use_my_bool_instead() is_proc);
In_C_you_should_use_my_bool_instead() is_acl_user(const char *host, const char *user);
#include "tztime.h"
class Time_zone: public Sql_alloc
{
public:
  Time_zone() {}
  virtual my_time_t TIME_to_gmt_sec(const MYSQL_TIME *t,
                                    my_bool *in_dst_time_gap) const = 0;
  virtual void gmt_sec_to_TIME(MYSQL_TIME *tmp, my_time_t t) const = 0;
  virtual const String * get_name() const = 0;
  virtual ~Time_zone() {};
};
extern Time_zone * my_tz_UTC;
extern Time_zone * my_tz_SYSTEM;
extern Time_zone * my_tz_OFFSET0;
extern Time_zone * my_tz_find(THD *thd, const String *name);
extern my_bool my_tz_init(THD *org_thd, const char *default_tzname, my_bool bootstrap);
extern void my_tz_free();
extern my_time_t sec_since_epoch_TIME(MYSQL_TIME *t);
static const int MY_TZ_TABLES_COUNT= 4;
In_C_you_should_use_my_bool_instead() check_global_access(THD *thd, ulong want_access);
int get_quote_char_for_identifier(THD *thd, const char *name, uint length);
void sql_perror(const char *message);
In_C_you_should_use_my_bool_instead() fn_format_relative_to_data_home(char * to, const char *name,
         const char *dir, const char *extension);
extern uint mysql_data_home_len;
extern char *mysql_data_home,server_version[60],
            mysql_real_data_home[], mysql_unpacked_real_data_home[];
extern CHARSET_INFO *character_set_filesystem;
extern char reg_ext[20];
extern uint reg_ext_length;
extern ulong specialflag;
extern uint lower_case_table_names;
extern In_C_you_should_use_my_bool_instead() mysqld_embedded;
extern my_bool opt_large_pages;
extern uint opt_large_page_size;
extern struct system_variables global_system_variables;
uint strconvert(CHARSET_INFO *from_cs, const char *from,
                CHARSET_INFO *to_cs, char *to, uint to_length, uint *errors);
uint filename_to_tablename(const char *from, char *to, uint to_length);
uint tablename_to_filename(const char *from, char *to, uint to_length);
