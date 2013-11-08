/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef XA_H_INCLUDED
#define XA_H_INCLUDED

#include "my_global.h"        // ulonglong
#include "mysql/plugin.h"     // MYSQL_XIDDATASIZE
#include "mysqld.h"           // server_id

#include <string.h>

class Protocol;
class THD;

/**
  Starts an XA transaction with the given xid value.

  @param thd    Current thread

  @retval false  Success
  @retval true   Failure
*/

bool trans_xa_start(THD *thd);


/**
  Put a XA transaction in the IDLE state.

  @param thd    Current thread

  @retval false  Success
  @retval true   Failure
*/

bool trans_xa_end(THD *thd);


/**
  Put a XA transaction in the PREPARED state.

  @param thd    Current thread

  @retval false  Success
  @retval true   Failure
*/

bool trans_xa_prepare(THD *thd);


/**
  Return the list of XID's to a client, the same way SHOW commands do.

  @param thd    Current thread

  @retval false  Success
  @retval true   Failure

  @note
    I didn't find in XA specs that an RM cannot return the same XID twice,
    so trans_xa_recover does not filter XID's to ensure uniqueness.
    It can be easily fixed later, if necessary.
*/

bool trans_xa_recover(THD *thd);


/**
  Commit and terminate the a XA transaction.

  @param thd    Current thread

  @retval false  Success
  @retval true   Failure
*/

bool trans_xa_commit(THD *thd);


/**
  Roll back and terminate a XA transaction.

  @param thd    Current thread

  @retval false  Success
  @retval true   Failure
*/

bool trans_xa_rollback(THD *thd);


typedef ulonglong my_xid; // this line is the same as in log_event.h
#define MYSQL_XID_PREFIX "MySQLXid"
#define XIDDATASIZE MYSQL_XIDDATASIZE
class XID_STATE;

/**
  struct xid_t is binary compatible with the XID structure as
  in the X/Open CAE Specification, Distributed Transaction Processing:
  The XA Specification, X/Open Company Ltd., 1991.
  http://www.opengroup.org/bookstore/catalog/c193.htm

  @see MYSQL_XID in mysql/plugin.h
*/
typedef struct xid_t
{
private:
  static const uint MYSQL_XID_PREFIX_LEN= 8; // must be a multiple of 8
  static const uint MYSQL_XID_OFFSET= MYSQL_XID_PREFIX_LEN + sizeof(server_id);
  static const uint MYSQL_XID_GTRID_LEN= MYSQL_XID_OFFSET + sizeof(my_xid);

  long formatID;
  long gtrid_length;
  long bqual_length;
  char data[XIDDATASIZE];  // not \0-terminated !

public:
  xid_t()
  : formatID(-1),
    gtrid_length(0),
    bqual_length(0)
  {
    memset(data, 0, XIDDATASIZE);
  }

  void set(long f, const char *g, long gl, const char *b, long bl)
  {
    formatID= f;
    memcpy(data, g, gtrid_length= gl);
    memcpy(data + gl, b, bqual_length= bl);
  }

  my_xid get_my_xid() const
  {
    if (gtrid_length == static_cast<long>(MYSQL_XID_GTRID_LEN) &&
        bqual_length == 0 &&
        !memcmp(data, MYSQL_XID_PREFIX, MYSQL_XID_PREFIX_LEN))
    {
      my_xid tmp;
      memcpy(&tmp, data + MYSQL_XID_OFFSET, sizeof(tmp));
      return tmp;
    }
    return 0;
  }

  uchar *key()
  {
    return reinterpret_cast<uchar *>(&gtrid_length);
  }

  const uchar *key() const
  {
    return reinterpret_cast<const uchar*>(&gtrid_length);
  }

  uint key_length() const
  {
    return sizeof(gtrid_length) + sizeof(bqual_length) +
      gtrid_length + bqual_length;
  }

#ifndef DBUG_OFF
  /**
     Get printable XID value.

     @param buf  pointer to the buffer where printable XID value has to be stored

     @return  pointer to the buffer passed in the first argument
  */
  char* xid_to_str(char *buf) const;
#endif

private:
  bool eq(const xid_t *xid) const
  {
    return xid->gtrid_length == gtrid_length &&
      xid->bqual_length == bqual_length &&
      !memcmp(xid->data, data, gtrid_length + bqual_length);
  }

  void set(const xid_t *xid)
  {
    memcpy(this, xid, sizeof(xid->formatID) + xid->key_length());
  }

  void set(my_xid xid)
  {
    formatID= 1;
    memcpy(data, MYSQL_XID_PREFIX, MYSQL_XID_PREFIX_LEN);
    memcpy(data + MYSQL_XID_PREFIX_LEN, &server_id, sizeof(server_id));
    memcpy(data + MYSQL_XID_OFFSET, &xid, sizeof(xid));
    gtrid_length= MYSQL_XID_GTRID_LEN;
    bqual_length= 0;
  }

  bool is_null() const
  {
    return formatID == -1;
  }

  void null()
  {
    formatID= -1;
  }

  /**
     This function checks if the XID consists of all printable characters
     i.e ASCII 32 - 127 and returns true if it is so.
  */
  bool is_printable_xid() const;

  friend class XID_STATE;
} XID;


class XID_STATE
{
public:
  enum xa_states {XA_NOTR=0, XA_ACTIVE, XA_IDLE, XA_PREPARED, XA_ROLLBACK_ONLY};

  /**
     Transaction identifier.
     For now, this is only used to catch duplicated external xids.
  */
private:
  static const char *xa_state_names[];

  XID m_xid;
  /// Used by external XA only
  xa_states xa_state;
  bool in_recovery;
  /// Error reported by the Resource Manager (RM) to the Transaction Manager.
  uint rm_error;

public:
  XID_STATE()
  : xa_state(XA_NOTR),
    in_recovery(false),
    rm_error(0)
  { m_xid.null(); }

  void set_state(xa_states state)
  { xa_state= state; }

  enum xa_states get_state()
  { return xa_state; }

  bool has_state(xa_states state) const
  { return xa_state == state; }

  const char* state_name() const
  { return xa_state_names[xa_state]; }

  const XID *get_xid() const
  { return &m_xid; }

  bool has_same_xid(const XID *xid) const
  { return m_xid.eq(xid); }

  void set_query_id(query_id_t query_id)
  {
    if (m_xid.is_null())
      m_xid.set(query_id);
  }

  void set_error(THD *thd);

  void reset_error()
  { rm_error= 0; }

  void cleanup()
  {
    /*
      If rm_error is raised, it means that this piece of a distributed
      transaction has failed and must be rolled back. But the user must
      rollback it explicitly, so don't start a new distributed XA until
      then.
    */
    if (!rm_error)
      m_xid.null();
  }

  void reset()
  {
    xa_state= XA_NOTR;
    m_xid.null();
    in_recovery= false;
  }

  void start_normal_xa(const XID *xid)
  {
    DBUG_ASSERT(m_xid.is_null());
    xa_state= XA_ACTIVE;
    m_xid.set(xid);
    in_recovery= false;
    rm_error= 0;
  }

  void start_recovery_xa(const XID *xid)
  {
    xa_state= XA_PREPARED;
    m_xid.set(xid);
    in_recovery= true;
    rm_error= 0;
  }

  bool is_in_recovery() const
  { return in_recovery; }

  void store_xid_info(Protocol *protocol) const;

  /**
     Mark a XA transaction as rollback-only if the RM unilaterally
     rolled back the transaction branch.

     @note If a rollback was requested by the RM, this function sets
           the appropriate rollback error code and transits the state
           to XA_ROLLBACK_ONLY.

     @return true if transaction was rolled back or if the transaction
             state is XA_ROLLBACK_ONLY. false otherwise.
  */

  bool xa_trans_rolled_back();


  /**
    Check that XA transaction is in state IDLE or PREPARED.

    @param  report_error  true if state IDLE or PREPARED has to be interpreted
                          as an error, else false

    @return  result of check
      @retval  false  XA transaction is NOT in state IDLE or PREPARED
      @retval  true   XA transaction is in state IDLE or PREPARED
  */

  bool check_xa_idle_or_prepared(bool report_error) const;


  /**
    Check that XA transaction has an uncommitted work. Report an error
    to a mysql user in case when there is an uncommitted work for XA transaction.

    @return  result of check
      @retval  false  XA transaction is NOT in state IDLE, PREPARED
                      or ROLLBACK_ONLY.
      @retval  true   XA transaction is in state IDLE or PREPARED
                      or ROLLBACK_ONLY.
  */

  bool check_has_uncommitted_xa() const;


  /**
    Check if an XA transaction has been started.

    @param  report_error  true if report an error in case when
                          XA transaction has been stared, else false.

    @return  result of check
      @retval  false  XA transaction hasn't been started (XA_NOTR)
      @retval  true   XA transaction has been started (!XA_NOTR)
  */

  bool check_in_xa(bool report_error) const;
};


/**
  Initialize a cache to store xid values and a mutex to protect access
  to the cache

  @return        result of initialization
    @retval false  success
    @retval true   failure
*/

bool xid_cache_init(void);


/**
  Deallocate resources held by a cache for storing xid values
  and by a mutex used to protect access to the cache.
*/

void xid_cache_free(void);


/**
  Delete information about XA transaction from cache.

  @param xid_state  Pointer to a XID_STATE structure that describes
                    an XA transaction.
*/

void xid_cache_delete(XID_STATE *xid_state);


#endif
