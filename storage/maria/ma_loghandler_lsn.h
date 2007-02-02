#ifndef _ma_loghandler_lsn_h
#define _ma_loghandler_lsn_h

/* Transaction log record address (file_no is int24 on the disk) */
typedef struct st_translog_address
{
  uint32 file_no;
  uint32 rec_offset;
} TRANSLOG_ADDRESS;

/*
  Compare addresses
    A1 >  A2 -> result  > 0
    A1 == A2 -> 0
    A1 <  A2 -> result < 0
*/
#define cmp_translog_addr(A1,A2) \
  ((A1).file_no == (A2).file_no ? \
   ((int64)(A1).rec_offset) - (int64)(A2).rec_offset : \
   ((int64)(A1).file_no - (int64)(A2).file_no))

/* LSN type (address of certain log record chank */
typedef TRANSLOG_ADDRESS LSN;

/* Puts LSN into buffer (dst) */
#define lsn7store(dst, lsn) \
  do { \
    int3store((dst), (lsn)->file_no); \
    int4store((dst) + 3, (lsn)->rec_offset); \
  } while (0)

/* Unpacks LSN from the buffer (P) */
#define lsn7korr(lsn, P) \
  do { \
    (lsn)->file_no= uint3korr(P); \
    (lsn)->rec_offset= uint4korr((P) + 3); \
  } while (0)

#endif
