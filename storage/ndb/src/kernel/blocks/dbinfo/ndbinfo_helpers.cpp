#include "Dbinfo.hpp"
#include <ndbinfo.h>
#include <AttributeHeader.hpp>
#include <signaldata/DbinfoScan.hpp>

void dbinfo_write_row_init(struct dbinfo_row *r, char* buf, int len)
{
  r->buf= buf;
  r->endrow= 0;
  r->blen= len;
  r->c= 0;
}

int dbinfo_write_row_column(struct dbinfo_row *r, const char* col, int clen)
{
  AttributeHeader ah;
  if(!((r->blen - r->endrow) >= (clen+ah.getHeaderSize()*sizeof(Uint32))))
  {
    return -1; // Not enough room.
  }
  r->buf[r->endrow] = 0;

  ah.setAttributeId(r->c++);
  ah.setByteSize(clen);
  ah.insertHeader((Uint32*)&r->buf[r->endrow]);

  r->endrow+= ah.getHeaderSize()*sizeof(Uint32);

  memcpy(&r->buf[r->endrow], col, clen);

  r->endrow+=clen;
  return 0;
}

int dbinfo_write_row_column_uint32(struct dbinfo_row *r, Uint32 value)
{
  return dbinfo_write_row_column(r, (char*)&value, sizeof(value));
}

void dbinfo_ratelimit_init(struct dbinfo_ratelimit *rl, struct DbinfoScanReq *r)
{
  rl->maxRows= r->maxRows;
  rl->maxBytes= r->maxBytes;
  rl->rows_total= r->rows_total;
  rl->bytes_total= r->word_total;
  rl->rows= 0;
  rl->bytes= 0;
}

int dbinfo_ratelimit_continue(struct dbinfo_ratelimit *rl)
{
  if(((rl->maxRows==0) || (rl->maxRows > rl->rows))
     && ((rl->maxBytes==0) || (rl->maxBytes > rl->bytes)) )
    return 1;
  return 0;
}
