#ifndef _ma_loghandler_lsn_h
#define _ma_loghandler_lsn_h

/* Transaction log record address (file_no is int24 on the disk) */
typedef int64 TRANSLOG_ADDRESS;

/*
  Compare addresses
    A1 >  A2 -> result  > 0
    A1 == A2 -> 0
    A1 <  A2 -> result < 0
*/
#define cmp_translog_addr(A1,A2) ((A1) - (A2))

/* LSN type (address of certain log record chank */
typedef TRANSLOG_ADDRESS LSN;

/* Gets file number part of a LSN/log address */
#define LSN_FILE_NO(L) ((L) >> 32)

/* Gets raw file number part of a LSN/log address */
#define LSN_FINE_NO_PART(L) ((L) & ((int64)0xFFFFFF00000000LL))

/* Gets record offset of a LSN/log address */
#define LSN_OFFSET(L) ((L) & 0xFFFFFFFFL)

/* Makes lsn/log address from file number and record offset */
#define MAKE_LSN(F,S) ((((uint64)(F)) << 32) | (S))

/* checks LSN */
#define LSN_VALID(L) DBUG_ASSERT((L) >= 0 && (L) < (uint64)0xFFFFFFFFFFFFFFLL)

/* size of stored LSN on a disk */
#define LSN_STORE_SIZE 7

/* Puts LSN into buffer (dst) */
#define lsn_store(dst, lsn) \
  do { \
    int3store((dst), LSN_FILE_NO(lsn)); \
    int4store((dst) + 3, LSN_OFFSET(lsn)); \
  } while (0)

/* Unpacks LSN from the buffer (P) */
#define lsn_korr(P) MAKE_LSN(uint3korr(P), uint4korr((P) + 3))

/* what we need to add to LSN to increase it on one file */
#define LSN_ONE_FILE ((int64)0x100000000LL)

#define LSN_REPLACE_OFFSET(L, S) (LSN_FINE_NO_PART(L) | (S))

#endif
