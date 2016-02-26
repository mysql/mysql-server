/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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

