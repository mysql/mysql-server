/* Copyright (C) 2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_types.h>

#ifndef __NDBINFO_H__
#define __NDBINFO_H__

#ifdef __cplusplus
extern "C" {
#endif

#define NDBINFO_TYPE_STRING 1
#define NDBINFO_TYPE_NUMBER 2

struct ndbinfo_column {
  char name[50];
  int coltype;
};

#define NDBINFO_CONSTANT_TABLE 0x1

struct ndbinfo_table {
  char name[50];
  int ncols;
  int flags;
  struct ndbinfo_column col[];
};

#define DECLARE_NDBINFO_TABLE(var, num)         \
struct ndbinfostruct##var {                      \
  struct ndbinfo_table t;                       \
  struct ndbinfo_column col[num];               \
} var

int ndbinfo_create_sql(struct ndbinfo_table *t, char* sql, int len);

static inline const char* ndbinfo_coltype_to_string(int coltype)
{
  static const char* ndbinfo_type_string[]= {"NONE","VARCHAR(255)","BIGINT"};

  if(coltype>3)
    coltype= 0;

  return ndbinfo_type_string[coltype];
}

struct dbinfo_row {
  char *buf;
  int   endrow;
  int   blen;
  int   c;
};

void dbinfo_write_row_init(struct dbinfo_row *r, char* buf, int len);

int dbinfo_write_row_column(struct dbinfo_row *r, const char* col, int clen);

int dbinfo_write_row_column_uint32(struct dbinfo_row *r, Uint32 value);

/*
 * We need to call protected function of SimulatedBlock (sendSignal)
 * so easier to implement as macro...
 */
#define dbinfo_send_row(signal, r, rl, apiTxnId, senderRef)             \
  do {                                                                  \
  TransIdAI *tidai= (TransIdAI*)signal->getDataPtrSend();               \
  tidai->connectPtr= 0;                                                 \
  tidai->transId[0]= apiTxnId;                                          \
  tidai->transId[1]= 0;                                                 \
  LinearSectionPtr ptr[3];                                              \
  ptr[0].p= (Uint32*)r.buf;                                             \
  ptr[0].sz= (Uint32)r.endrow;                                          \
  rl.rows++;                                                            \
  rl.bytes+=r.endrow;                                                   \
  sendSignal(senderRef, GSN_DBINFO_TRANSID_AI, signal, 3, JBB, ptr, 1); \
} while (0)

#define dbinfo_ratelimit_sendconf(signal, req, rl, itemnumber)          \
  do {                                                                  \
  DbinfoScanConf *conf= (DbinfoScanConf*)signal->getDataPtrSend();      \
  conf->tableId= (req).tableId;                                         \
  conf->senderRef= (req).senderRef;                                     \
  conf->apiTxnId= (req).apiTxnId;                                       \
  conf->colBitmapLo= (req).colBitmapLo;                                 \
  conf->colBitmapHi= (req).colBitmapHi;                                 \
  conf->requestInfo= (req).requestInfo | DbinfoScanConf::MoreData;      \
  conf->cur_requestInfo= 0;                                             \
  conf->cur_node= getOwnNodeId();                                       \
  conf->cur_block= number();                                            \
  conf->cur_item= (itemnumber);                                         \
  conf->maxRows= (rl).maxRows;                                          \
  conf->maxBytes= (rl).maxBytes;                                        \
  conf->rows_total= (rl).rows_total + (rl).rows;                        \
  conf->word_total= (rl).bytes_total+ (rl).bytes;                       \
  sendSignal((req).senderRef, GSN_DBINFO_SCANCONF, signal,              \
             DbinfoScanConf::SignalLengthWithCursor, JBB);              \
} while (0)


struct dbinfo_ratelimit {
  Uint32 maxRows;
  Uint32 maxBytes;
  Uint32 rows_total;
  Uint32 bytes_total;
  Uint32 rows;
  Uint32 bytes;
};

void dbinfo_ratelimit_init(struct dbinfo_ratelimit *rl, struct DbinfoScanReq *r);

int dbinfo_ratelimit_continue(struct dbinfo_ratelimit *rl);

#ifdef __cplusplus
}
#endif

#endif
