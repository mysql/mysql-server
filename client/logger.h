#ifndef LOGGER_UTIL_INCLUDED
#define LOGGER_UTIL_INCLUDED
/*
   Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

/**
  A trivial placeholder for inserting Time signatures into streams.
 @example cout << Datetime() << "[Info] Today was a sunny day" << endl;
*/

#include <ostream>
#include <string>
#include <sstream>

using namespace std;

struct Datetime {};

ostream &operator<<(ostream &os, const Datetime &dt);

class Gen_spaces
{
public:
  Gen_spaces(int s)
  {
    m_spaces.assign(s,' ');
  }
  ostream &operator<<(ostream &os)
  {
    return os;
  }
  friend ostream &operator<<(ostream &os, const Gen_spaces &gen);
private:
  string m_spaces;
};

ostream &operator<<(ostream &os, const Gen_spaces &gen);

class Log : public ostream
{
public:
  Log(ostream &str, string logclass) :
   ostream(NULL), m_buffer(str, logclass)
  {
    this->init(&m_buffer);
  }
  void enabled(bool s) { m_buffer.enabled(s); }
private:

  class Log_buff : public stringbuf
  {
  public:
    Log_buff(ostream &str, string &logc)
      :m_os(str),m_logc(logc), m_enabled(true)
    {}
    void set_log_class(string &s) { m_logc= s; }
    void enabled(bool s) { m_enabled= s; }
    virtual int sync();
  private:
    ostream &m_os;
    string m_logc;
    bool m_enabled;
  };

  Log_buff m_buffer;
};

#endif /* LOGGER_UTIL_INCLUDED */
