
/* Transaction log flags */
#define TRANSLOG_PAGE_CRC              1
#define TRANSLOG_SECTOR_PROTECTION     (1<<1)
#define TRANSLOG_RECORD_CRC            (1<<2)
#define TRANSLOG_FLAGS_NUM ((TRANSLOG_PAGE_CRC | TRANSLOG_SECTOR_PROTECTION | \
                           TRANSLOG_RECORD_CRC) + 1)

/*
  Page size in transaction log
  It should be Power of 2 and multiple of DISK_DRIVE_SECTOR_SIZE
  (DISK_DRIVE_SECTOR_SIZE * 2^N)
*/
#define TRANSLOG_PAGE_SIZE (8*1024)

#include "ma_loghandler_lsn.h"

/* short transaction ID type */
typedef uint16 SHORT_TRANSACTION_ID;

/* Length of CRC at end of pages */
#define CRC_LENGTH 4
/*
  Length of disk drive sector size (we assume that writing it
  to disk is atomic operation)
*/
#define DISK_DRIVE_SECTOR_SIZE 512

/* types of records in the transaction log */
enum translog_record_type
{
  LOGREC_RESERVED_FOR_CHUNKS23= 0,
  LOGREC_REDO_INSERT_ROW_HEAD= 1,
  LOGREC_REDO_INSERT_ROW_TAIL= 2,
  LOGREC_REDO_INSERT_ROW_BLOB= 3,
  LOGREC_REDO_INSERT_ROW_BLOBS= 4,
  LOGREC_REDO_PURGE_ROW= 5,
  eLOGREC_REDO_PURGE_BLOCKS= 6,
  LOGREC_REDO_DELETE_ROW= 7,
  LOGREC_REDO_UPDATE_ROW_HEAD= 8,
  LOGREC_REDO_INDEX= 9,
  LOGREC_REDO_UNDELETE_ROW= 10,
  LOGREC_CLR_END= 11,
  LOGREC_PURGE_END= 12,
  LOGREC_UNDO_ROW_INSERT= 13,
  LOGREC_UNDO_ROW_DELETE= 14,
  LOGREC_UNDO_ROW_UPDATE= 15,
  LOGREC_UNDO_KEY_INSERT= 16,
  LOGREC_UNDO_KEY_DELETE= 17,
  LOGREC_PREPARE= 18,
  LOGREC_PREPARE_WITH_UNDO_PURGE= 19,
  LOGREC_COMMIT= 20,
  LOGREC_COMMIT_WITH_UNDO_PURGE= 21,
  LOGREC_CHECKPOINT_PAGE= 22,
  LOGREC_CHECKPOINT_TRAN= 23,
  LOGREC_CHECKPOINT_TABL= 24,
  LOGREC_REDO_CREATE_TABLE= 25,
  LOGREC_REDO_RENAME_TABLE= 26,
  LOGREC_REDO_DROP_TABLE= 27,
  LOGREC_REDO_TRUNCATE_TABLE= 28,
  LOGREC_FILE_ID= 29,
  LOGREC_LONG_TRANSACTION_ID= 30,
  LOGREC_RESERVED_FUTURE_EXTENSION= 63
};
#define LOGREC_NUMBER_OF_TYPES 64              /* Maximum, can't be extended */

/* Size of log file; One log file is restricted to 4G */
typedef uint32 translog_size_t;

#define TRANSLOG_RECORD_HEADER_MAX_SIZE 1024

typedef struct st_translog_group_descriptor
{
  TRANSLOG_ADDRESS addr;
  uint8 num;
} TRANSLOG_GROUP;


typedef struct st_translog_header_buffer
{
  /* LSN of the read record */
  LSN lsn;
  /* array of groups descriptors, can be used only if groups_no > 0 */
  TRANSLOG_GROUP *groups;
  /* short transaction ID or 0 if it has no sense for the record */
  SHORT_TRANSACTION_ID short_trid;
  /*
     The Record length in buffer (including read header, but excluding
     hidden part of record (type, short TrID, length)
  */
  translog_size_t record_length;
  /*
     Buffer for write decoded header of the record (depend on the record
     type)
  */
  byte header[TRANSLOG_RECORD_HEADER_MAX_SIZE];
  /* number of groups listed in  */
  uint groups_no;
  /* in multi-group number of chunk0 pages (valid only if groups_no > 0) */
  uint chunk0_pages;
  /* type of the read record */
   enum translog_record_type type;
  /* chunk 0 data address (valid only if groups_no > 0) */
  TRANSLOG_ADDRESS chunk0_data_addr;
   /*
     Real compressed LSN(s) size economy (<number of LSN(s)>*7 - <real_size>)
  */
  uint16 compressed_LSN_economy;
  /* short transaction ID or 0 if it has no sense for the record */
  uint16 non_header_data_start_offset;
  /* non read body data length in this first chunk */
  uint16 non_header_data_len;
  /* chunk 0 data size (valid only if groups_no > 0) */
  uint16 chunk0_data_len;
} TRANSLOG_HEADER_BUFFER;


typedef struct st_translog_scanner_data
{
  byte buffer[TRANSLOG_PAGE_SIZE];             /* buffer for page content */
  TRANSLOG_ADDRESS page_addr;                  /* current page address */
  /* end of the log which we saw last time */
  TRANSLOG_ADDRESS horizon;
  TRANSLOG_ADDRESS last_file_page;             /* Last page on in this file */
  byte *page;                                  /* page content pointer */
  /* offset of the chunk in the page */
  translog_size_t page_offset;
  /* set horizon only once at init */
  my_bool fixed_horizon;
} TRANSLOG_SCANNER_DATA;


struct st_translog_reader_data
{
  TRANSLOG_HEADER_BUFFER header;                /* Header */
  TRANSLOG_SCANNER_DATA scanner;                /* chunks scanner */
  translog_size_t body_offset;                  /* current chunk body offset */
  /* data offset from the record beginning */
  translog_size_t current_offset;
  /* number of bytes read in header */
  uint16 read_header;
  uint16 chunk_size;                            /* current chunk size */
  uint current_group;                           /* current group */
  uint current_chunk;                           /* current chunk in the group */
  my_bool eor;                                  /* end of the record */
};


my_bool translog_init(const char *directory, uint32 log_file_max_size,
                      uint32 server_version, uint32 server_id,
                      PAGECACHE *pagecache, uint flags);

my_bool translog_write_record(LSN *lsn,
                              enum translog_record_type type,
                              SHORT_TRANSACTION_ID short_trid,
                              void *tcb,
                              translog_size_t part1_length,
                              byte *part1_buff, ...);

void translog_destroy();

translog_size_t translog_read_record_header(LSN lsn,
                                            TRANSLOG_HEADER_BUFFER *buff);

void translog_free_record_header(TRANSLOG_HEADER_BUFFER *buff);

translog_size_t translog_read_record(LSN lsn,
                                     translog_size_t offset,
                                     translog_size_t length,
                                     byte *buffer,
                                     struct st_translog_reader_data *data);

my_bool translog_flush(LSN lsn);

my_bool translog_init_scanner(LSN lsn,
                              my_bool fixed_horizon,
                              struct st_translog_scanner_data *scanner);

translog_size_t translog_read_next_record_header(TRANSLOG_SCANNER_DATA
                                                 *scanner,
                                                 TRANSLOG_HEADER_BUFFER *buff);

