/*
  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SINGLE_TRANSACTION_CONNECTION_PROVIDER_INCLUDED
#define SINGLE_TRANSACTION_CONNECTION_PROVIDER_INCLUDED

#include <functional>

#include "client/base/message_data.h"
#include "client/base/mutex.h"
#include "client/base/mysql_query_runner.h"
#include "client/dump/i_connection_provider.h"
#include "client/dump/thread_specific_connection_provider.h"

namespace Mysql{
namespace Tools{
namespace Dump{

class Single_transaction_connection_provider
  : public Thread_specific_connection_provider
{
public:
  Single_transaction_connection_provider(
    Mysql::Tools::Base::I_connection_factory* connection_factory,
    unsigned int connections,
    std::function<bool(const Mysql::Tools::Base::Message_data&)>*
    message_handler);

  virtual Mysql::Tools::Base::Mysql_query_runner* create_new_runner(
    std::function<bool(const Mysql::Tools::Base::Message_data&)>*
    message_handler);
private:
  std::vector<Mysql::Tools::Base::Mysql_query_runner*> m_runner_pool;
  my_boost::mutex m_pool_mutex;
  unsigned int m_connections;
};

}
}
}

#endif
