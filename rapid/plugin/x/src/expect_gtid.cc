/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "expect_gtid.h"
#include "xerrmsg.h"

using namespace xpl;

Expect_gtid::Expect_gtid(const std::string &data)
: m_timeout(0)
{
  std::string::size_type begin = 0, end = data.find(',');
  while (end != std::string::npos)
  {
    m_gtids.push_back(data.substr(begin, end));
    begin = end+1;
    end = data.find(',', begin);
  }
  m_gtids.push_back(data.substr(begin));
}



ngs::Error_code Expect_gtid::check()
{
  for (;;)
  {

    for (std::list<std::string>::const_iterator g = m_gtids.begin();
         g != m_gtids.end(); ++g)
    {
      
    }

    if (m_timeout > 0)
    {
      
    }
    else
      break;
  }

  return ngs::Error_code(ER_X_EXPECT_FAILED, "Expectation failed: gtid");
}

