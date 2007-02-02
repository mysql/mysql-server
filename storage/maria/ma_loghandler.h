
#ifndef _ma_loghandler_h
#define _ma_loghandler_h

/* Transaction log flags */
#define TRANSLOG_PAGE_CRC              1
#define TRANSLOG_SECTOR_PROTECTION     (1<<1)
#define TRANSLOG_RECORD_CRC            (1<<2)

/* page size in transaction log */
#define TRANSLOG_PAGE_SIZE (8*1024)

#include "ma_loghandler_lsn.h"

/* short transaction ID type */
typedef uint16 SHORT_TRANSACTION_ID;

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
#define LOGREC_NUMBER_OF_TYPES 64

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
  /* type of the read record */
  enum translog_record_type type;
  /* short transaction ID or 0 if it has no sense for the record */
  SHORT_TRANSACTION_ID short_trid;
  /*
     The Record length in buffer (including read header, but excluding
     hidden part of record (type, short TrID, length)
  */
  translog_size_t record_length;
  /*
     Real compressed LSN(s) size economy (<number of LSN(s)>*7 - <real_size>)
  */
  uint16 compressed_LSN_economy;
  /*
     Buffer for write decoded header of the record (depend on the record
     type)
  */
  uchar header[TRANSLOG_RECORD_HEADER_MAX_SIZE];
  /* non read body data offset on the page */
  uint16 non_header_data_start_offset;
  /* non read body data length in this first chunk */
  uint16 non_header_data_len;
  /* number of groups listed in  */
  uint groups_no;
  /* array of groups descriptors, can be used only if groups_no > 0 */
  TRANSLOG_GROUP *groups;
  /* in multi-group number of chunk0 pages (valid only if groups_no > 0) */
  uint chunk0_pages;
  /* chunk 0 data address (valid only if groups_no > 0) */
  TRANSLOG_ADDRESS chunk0_data_addr;
  /* chunk 0 data size (valid only if groups_no > 0) */
  uint16 chunk0_data_len;
} TRANSLOG_HEADER_BUFFER;


struct st_translog_scanner_data
{
  uchar buffer[TRANSLOG_PAGE_SIZE];             /* buffer for page content */
  TRANSLOG_ADDRESS page_addr;                   /* current page address */
  TRANSLOG_ADDRESS horizon;                     /* end of the log which we saw
                                                   last time */
  TRANSLOG_ADDRESS last_file_page;              /* Last page on in this file */
  uchar *page;                                  /* page content pointer */
  translog_size_t page_offset;                  /* offset of the chunk in the
                                                   page */
  my_bool fixed_horizon;                        /* set horizon only once at
                                                   init */
};


struct st_translog_reader_data
{
  TRANSLOG_HEADER_BUFFER header;                /* Header */
  struct st_translog_scanner_data scanner;      /* chunks scanner */
  translog_size_t body_offset;                  /* current chunk body offset */
  translog_size_t current_offset;               /* data offset from the record
                                                   beginning */
  uint16 read_header;                           /* number of bytes read in
                                                   header */
  uint16 chunk_size;                            /* current chunk size */
  uint current_group;                           /* current group */
  uint current_chunk;                           /* current chunk in the group */
  my_bool eor;                                  /* end of the record */
};


/*
  Initialize transaction log

  SYNOPSIS
    translog_init()
    directory            Directory where log files are put
    log_file_max_size    max size of one log size (for new logs creation)
    server_version       version of MySQL servger (MYSQL_VERSION_ID)
    server_id            server ID (replication & Co)
    pagecache            Page cache for the log reads
    flags                flags (TRANSLOG_PAGE_CRC, TRANSLOG_SECTOR_PROTECTION
                           TRANSLOG_RECORD_CRC)

  RETURN
    0 - OK
    1 - Error
*/

my_bool translog_init(const char *directory, uint32 log_file_max_size,
                      uint32 server_version,
                      uint32 server_id, PAGECACHE *pagecache, uint flags);


/*
  Write the log record

  SYNOPSIS
    translog_write_record()
    lsn                  LSN of the record will be writen here
    type                 the log record type
    short_trid           Sort transaction ID or 0 if it has no sense
    tcb                  Transaction control block pointer for hooks by
                         record log type
    partN_length         length of Ns part of the log
    partN_buffer         pointer on Ns part buffer
    0                    sign of the end of parts

  RETURN
    0 - OK
    1 - Error
*/

my_bool translog_write_record(LSN *lsn,
                              enum translog_record_type type,
                              SHORT_TRANSACTION_ID short_trid,
                              void *tcb,
                              translog_size_t part1_length,
                              uchar *part1_buff, ...);


/*
  Free log handler resources

  SYNOPSIS
    translog_destroy()
*/

void translog_destroy();


/*
  Read record header and some fixed part of a record (the part depend on
  record type).

  SYNOPSIS
    translog_read_record_header()
    lsn                  log record serial number (address of the record)
    buff                 log record header buffer

  NOTE
    - lsn can point to TRANSLOG_HEADER_BUFFER::lsn and it will be processed
      correctly.
    - Some type of record can be read completely by this call
    - "Decoded" header stored in TRANSLOG_HEADER_BUFFER::header (relative
      LSN can be translated to absolute one), some fields can be added
      (like actual header length in the record if the header has variable
      length)

  RETURN
    0 - error
    number of bytes in TRANSLOG_HEADER_BUFFER::header where stored decoded
      part of the header
*/

translog_size_t translog_read_record_header(LSN *lsn,
                                            TRANSLOG_HEADER_BUFFER *buff);


/*
  Free resources used by TRANSLOG_HEADER_BUFFER

  SYNOPSIS
    translog_free_record_header();
*/

void translog_free_record_header(TRANSLOG_HEADER_BUFFER *buff);


/*
  Read a part of the record.

  SYNOPSIS
    translog_read_record_header()
    lsn                  log record serial number (address of the record)
    offset               from the beginning of the record beginning (read
                         by translog_read_record_header).
    length               length of record part which have to be read.
    buffer               buffer where to read the record part (have to be at
                         least 'length' bytes length)

  RETURN
    0 - error (or read out of the record)
    length of data actually read
*/

translog_size_t translog_read_record(LSN *lsn,
                                     translog_size_t offset,
                                     translog_size_t length,
                                     uchar *buffer,
                                     struct st_translog_reader_data *data);


/*
  Flush the log up to given LSN (included)

  SYNOPSIS
    translog_flush()
    lsn                  log record serial number up to which (inclusive)
                         the log have to be flushed

  RETURN
    0 - OK
    1 - Error
*/

my_bool translog_flush(LSN *lsn);


/*
  Read record header and some fixed part of the next record (the part
  depend on record type).

  SYNOPSIS
    translog_read_next_record_header()
    lsn                  log record serial number (address of the record)
                         previous to  the record which will be read
                         If LSN present scanner will be initialized from it,
                         do not use LSN after initialization for fast scanning.
    buff                 log record header buffer
    fixed_horizon        true if it is OK do not read records which was written
                         after scaning begining
    scanner              data for scaning if lsn is NULL scanner data
                         will be used for continue scaning.
                         scanner can be NULL.

  NOTE
    - lsn can point to TRANSLOG_HEADER_BUFFER::lsn and it will be processed
      correctly (lsn in buffer will be replaced by next record, but initial
      lsn will be read correctly).
    - it is like translog_read_record_header, but read next record, so see
      its NOTES.
    - in case of end of the log buff->lsn will be set to
      (CONTROL_FILE_IMPOSSIBLE_LOGNO, 0)
  RETURN
    0                                    - error
    TRANSLOG_RECORD_HEADER_MAX_SIZE + 1  - End of the log
    number of bytes in TRANSLOG_HEADER_BUFFER::header where stored decoded
      part of the header
*/

translog_size_t translog_read_next_record_header(LSN *lsn,
                                                 TRANSLOG_HEADER_BUFFER *buff,
                                                 my_bool fixed_horizon,
                                                 struct
                                                 st_translog_scanner_data
                                                 *scanner);

#endif
