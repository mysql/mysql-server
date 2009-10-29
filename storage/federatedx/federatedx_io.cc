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

typedef federatedx_io *(*instantiate_io_type)(MEM_ROOT *server_root,
                                              FEDERATEDX_SERVER *server);
struct io_schemes_st
{
  const char *scheme;
  instantiate_io_type instantiate;
};


static const io_schemes_st federated_io_schemes[] =
{
  { "mysql", &instantiate_io_mysql },
  { "null", instantiate_io_null } /* must be last element */
};

const uint federated_io_schemes_count= array_elements(federated_io_schemes);

federatedx_io::federatedx_io(FEDERATEDX_SERVER *aserver)
  : server(aserver), owner_ptr(0), txn_next(0), idle_next(0),
    active(FALSE), busy(FALSE), readonly(TRUE)
{
  DBUG_ENTER("federatedx_io::federatedx_io");
  DBUG_ASSERT(server);

  safe_mutex_assert_owner(&server->mutex);
  server->io_count++;

  DBUG_VOID_RETURN;
}


federatedx_io::~federatedx_io()
{
  DBUG_ENTER("federatedx_io::~federatedx_io");

  server->io_count--;

  DBUG_VOID_RETURN;
}


bool federatedx_io::handles_scheme(const char *scheme)
{
  const io_schemes_st *ptr = federated_io_schemes;
  const io_schemes_st *end = ptr + array_elements(federated_io_schemes);
  while (ptr != end && strcasecmp(scheme, ptr->scheme))
    ++ptr;
  return ptr != end;
}


federatedx_io *federatedx_io::construct(MEM_ROOT *server_root,
                                        FEDERATEDX_SERVER *server)
{
  const io_schemes_st *ptr = federated_io_schemes;
  const io_schemes_st *end = ptr + (array_elements(federated_io_schemes) - 1);
  while (ptr != end && strcasecmp(server->scheme, ptr->scheme))
    ++ptr;
  return ptr->instantiate(server_root, server);
}


