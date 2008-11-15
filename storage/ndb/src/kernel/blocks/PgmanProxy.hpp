/* Copyright (C) 2003 MySQL AB

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

#ifndef NDB_PGMAN_PROXY_HPP
#define NDB_PGMAN_PROXY_HPP

#include <LocalProxy.hpp>
#include "pgman.hpp"

class PgmanProxy : public LocalProxy {
public:
  PgmanProxy(Block_context& ctx);
  virtual ~PgmanProxy();
  BLOCK_DEFINES(PgmanProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);

  // client methods
  friend class Page_cache_client;

  int get_page(Page_cache_client& caller,
               Signal*, Page_cache_client::Request& req, Uint32 flags);

  void update_lsn(Page_cache_client& caller,
                  Local_key key, Uint64 lsn);

  int drop_page(Page_cache_client& caller,
                Local_key key, Uint32 page_id);

  Uint32 create_data_file();

  Uint32 alloc_data_file(Uint32 file_no);

  void map_file_no(Uint32 file_no, Uint32 fd);

  void free_data_file(Uint32 file_no, Uint32 fd);
};

#endif
