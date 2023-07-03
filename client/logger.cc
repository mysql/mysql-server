/*
   Copyright (c) 2015, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "client/logger.h"

#include <time.h>
#include <iostream>
#include <locale>
#include <memory>

using namespace std;

ostream &operator<<(ostream &os, const Datetime &) {
  const char format[] = "%Y-%m-%d %X";
  time_t t(time(nullptr));
  tm tm(*localtime(&t));

  const size_t date_length{50};
  std::unique_ptr<char[]> date{new char[date_length]};
  strftime(date.get(), date_length, format, &tm);

  os << date.get() << " ";
  return os;
}

ostream &operator<<(ostream &os, const Gen_spaces &gen) {
  return os << gen.m_spaces;
}

int Log::Log_buff::sync() {
  string sout(str());
  if (m_enabled && sout.length() > 0) {
    m_os << Datetime() << "[" << m_logc << "]"
         << Gen_spaces(8 - m_logc.length()) << sout;
  }
  str("");
  m_os.flush();
  return 0;
}
