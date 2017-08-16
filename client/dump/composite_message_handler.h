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

#ifndef COMPOSITE_MESSAGE_HANDLER_INCLUDED
#define COMPOSITE_MESSAGE_HANDLER_INCLUDED

#include <functional>
#include <vector>

#include "client/base/message_data.h"

namespace Mysql{
namespace Tools{
namespace Dump{

class Composite_message_handler
{
public:
  static std::function<bool(const Mysql::Tools::Base::Message_data&)>*
    create_composite_handler
    (const std::vector<std::function<
        bool(const Mysql::Tools::Base::Message_data&)>*>&
      message_handlers);

private:
  Composite_message_handler(
    const std::vector<std::function<
      bool(const Mysql::Tools::Base::Message_data&)>*>&
    message_handlers);
  /**
    Passes message to message callbacks in reverse order, stopping on first
    handler to declare message consumed.
   */
  bool pass_message(const Mysql::Tools::Base::Message_data& message_data);

  std::vector<std::function<
      bool(const Mysql::Tools::Base::Message_data&)>*>
    m_message_handlers;
};

}
}
}

#endif
