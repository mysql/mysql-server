#ifndef LOGGER_UTIL_INCLUDED
#define LOGGER_UTIL_INCLUDED
/*
   Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

/**
  A trivial placeholder for inserting Time signatures into streams.
 @example cout << Datetime() << "[Info] Today was a sunny day" << endl;
*/

#include <ostream>
#include <string>
#include <sstream>

struct Datetime {};

std::ostream &operator<<(std::ostream &os, const Datetime &dt);

class Gen_spaces
{
public:
  Gen_spaces(int s)
  {
    m_spaces.assign(s,' ');
  }
  std::ostream &operator<<(std::ostream &os)
  {
    return os;
  }
  friend std::ostream &operator<<(std::ostream &os, const Gen_spaces &gen);
private:
  std::string m_spaces;
};

std::ostream &operator<<(std::ostream &os, const Gen_spaces &gen);

class Log : public std::ostream
{
public:
  Log(std::ostream &str, std::string logclass) :
   std::ostream(NULL), m_buffer(str, logclass)
  {
    this->init(&m_buffer);
  }
  void enabled(bool s) { m_buffer.enabled(s); }
private:

  class Log_buff : public std::stringbuf
  {
  public:
    Log_buff(std::ostream &str, std::string &logc)
      :m_os(str),m_logc(logc), m_enabled(true)
    {}
    void set_log_class(std::string &s) { m_logc= s; }
    void enabled(bool s) { m_enabled= s; }
    virtual int sync();
  private:
    std::ostream &m_os;
    std::string m_logc;
    bool m_enabled;
  };

  Log_buff m_buffer;
};

#endif /* LOGGER_UTIL_INCLUDED */
