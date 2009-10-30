/* 
Copyright (c) 2007, Antony T Curtis
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

    * Neither the name of FederatedX nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


/*#define MYSQL_SERVER 1*/
#include "mysql_priv.h"
#include <mysql/plugin.h>

#include "ha_federatedx.h"

#include "m_string.h"

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation                          // gcc: Class implementation
#endif


#define SAVEPOINT_REALIZED  1
#define SAVEPOINT_RESTRICT  2
#define SAVEPOINT_EMITTED 4


typedef struct federatedx_savepoint
{
  ulong level;
  uint  flags;
} SAVEPT;


class federatedx_io_null :public federatedx_io
{
public:
  federatedx_io_null(FEDERATEDX_SERVER *);
  ~federatedx_io_null();

  int query(const char *buffer, uint length);
  virtual FEDERATEDX_IO_RESULT *store_result();

  virtual size_t max_query_size() const;

  virtual my_ulonglong affected_rows() const;
  virtual my_ulonglong last_insert_id() const;

  virtual int error_code();
  virtual const char *error_str();
  
  void reset();
  int commit();
  int rollback();
  
  int savepoint_set(ulong sp);
  ulong savepoint_release(ulong sp);
  ulong savepoint_rollback(ulong sp);
  void savepoint_restrict(ulong sp);
  
  ulong last_savepoint() const;
  ulong actual_savepoint() const;
  bool is_autocommit() const;

  bool table_metadata(ha_statistics *stats, const char *table_name,
                      uint table_name_length, uint flag);
  
  /* resultset operations */
  
  virtual void free_result(FEDERATEDX_IO_RESULT *io_result);
  virtual unsigned int get_num_fields(FEDERATEDX_IO_RESULT *io_result);
  virtual my_ulonglong get_num_rows(FEDERATEDX_IO_RESULT *io_result);
  virtual FEDERATEDX_IO_ROW *fetch_row(FEDERATEDX_IO_RESULT *io_result);
  virtual ulong *fetch_lengths(FEDERATEDX_IO_RESULT *io_result);
  virtual const char *get_column_data(FEDERATEDX_IO_ROW *row,
                                      unsigned int column);
  virtual bool is_column_null(const FEDERATEDX_IO_ROW *row,
                              unsigned int column) const;
};


federatedx_io *instantiate_io_null(MEM_ROOT *server_root,
                                   FEDERATEDX_SERVER *server)
{
  return new (server_root) federatedx_io_null(server);
}


federatedx_io_null::federatedx_io_null(FEDERATEDX_SERVER *aserver)
  : federatedx_io(aserver)
{
}


federatedx_io_null::~federatedx_io_null()
{
}


void federatedx_io_null::reset()
{
}


int federatedx_io_null::commit()
{
  return 0;
}

int federatedx_io_null::rollback()
{
  return 0;
}


ulong federatedx_io_null::last_savepoint() const
{
  return 0;
}


ulong federatedx_io_null::actual_savepoint() const
{
  return 0;
}

bool federatedx_io_null::is_autocommit() const
{
  return 0;
}


int federatedx_io_null::savepoint_set(ulong sp)
{
  return 0;
}


ulong federatedx_io_null::savepoint_release(ulong sp)
{
  return 0;
}


ulong federatedx_io_null::savepoint_rollback(ulong sp)
{
  return 0;
}


void federatedx_io_null::savepoint_restrict(ulong sp)
{
}


int federatedx_io_null::query(const char *buffer, uint length)
{
  return 0;
}


size_t federatedx_io_null::max_query_size() const
{
  return INT_MAX;
}


my_ulonglong federatedx_io_null::affected_rows() const
{
  return 0;
}


my_ulonglong federatedx_io_null::last_insert_id() const
{
  return 0;
}


int federatedx_io_null::error_code()
{
  return 0;
}


const char *federatedx_io_null::error_str()
{
  return "";
}


FEDERATEDX_IO_RESULT *federatedx_io_null::store_result()
{
  FEDERATEDX_IO_RESULT *result;
  DBUG_ENTER("federatedx_io_null::store_result");
  
  result= NULL;
  
  DBUG_RETURN(result);
}


void federatedx_io_null::free_result(FEDERATEDX_IO_RESULT *)
{
}


unsigned int federatedx_io_null::get_num_fields(FEDERATEDX_IO_RESULT *)
{
  return 0;
}


my_ulonglong federatedx_io_null::get_num_rows(FEDERATEDX_IO_RESULT *)
{
  return 0;
}


FEDERATEDX_IO_ROW *federatedx_io_null::fetch_row(FEDERATEDX_IO_RESULT *)
{
  return NULL;
}


ulong *federatedx_io_null::fetch_lengths(FEDERATEDX_IO_RESULT *)
{
  return NULL;
}


const char *federatedx_io_null::get_column_data(FEDERATEDX_IO_ROW *,
                                                 unsigned int)
{
  return "";
}


bool federatedx_io_null::is_column_null(const FEDERATEDX_IO_ROW *,
                                         unsigned int) const
{
  return true;
}

bool federatedx_io_null::table_metadata(ha_statistics *stats,
                                        const char *table_name,
                                        uint table_name_length, uint flag)
{
  stats->records= (ha_rows) 0;
  stats->mean_rec_length= (ulong) 0;
  stats->data_file_length= 0;

  stats->update_time= (time_t) 0;
  stats->check_time= (time_t) 0;

  return 0;
}
