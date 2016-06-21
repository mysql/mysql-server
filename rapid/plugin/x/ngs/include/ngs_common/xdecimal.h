/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#ifndef _MYSQLX_DECIMAL_H_
#define _MYSQLX_DECIMAL_H_

#include <string>
#include <stdint.h>
#include <stdexcept>

namespace mysqlx
{
  class invalid_value : public std::runtime_error
  {
  public:
    invalid_value(const std::string &w) : std::runtime_error(w) {}
    virtual ~invalid_value() throw() {}
  };


  class Decimal
  {
  public:
    Decimal() {}

    Decimal(const std::string &s)
    {
      std::size_t scale = 0;
      std::size_t dot_pos = s.find('.');
      bool dot_skipped = false;
      if (dot_pos != std::string::npos)
      {
        scale = s.length() - dot_pos - 1;
      }
      m_buffer.push_back(static_cast<char>(scale));

      std::string::const_iterator c = s.begin();
      if (c != s.end())
      {
        int sign = (*c == '-') ? 0xd : (*c == '+' ? 0xc : 0);
        if (sign == 0)
          sign = 0xc;
        else
          ++c;

        while (c != s.end())
        {
          int c1 = *(c++);
          if (c1 == '.')
          {
            if (dot_skipped)
            {
              /*more than one dot*/
              throw invalid_value("Invalid decimal value " + s);
            }
            dot_skipped = true;
            continue;
          }
          if (c1 < '0' || c1 > '9')
            throw invalid_value("Invalid decimal value "+s);
          if (c == s.end())
          {
            m_buffer.push_back((c1 - '0') << 4 | sign);
            sign = 0;
            break;
          }
          int c2 = *(c++);
          if (c2 == '.')
          {
            if (dot_skipped)
            {
              /*more than one dot*/
              throw invalid_value("Invalid decimal value " + s);
            }
            dot_skipped = true;
            if (c == s.end())
            {
              m_buffer.push_back((c1 - '0') << 4 | sign);
              sign = 0;
              break;
            }
            else
            {
              c2 = *(c++);
            }
          }
          if (c2 < '0' || c2 > '9')
            throw invalid_value("Invalid decimal value "+s);

          m_buffer.push_back((c1 - '0') << 4 | (c2 - '0'));
        }
        if (m_buffer.length() <= 1) /* only scale */
          throw invalid_value("Invalid decimal value "+s);
        if (sign)
          m_buffer.push_back(sign << 4);
      }
    }

    std::string str() const
    {
      std::string r;

      if (m_buffer.length() < 1)
      {
        throw invalid_value("Invalid decimal value " + m_buffer);
      }
      size_t scale = m_buffer[0];

      for (std::string::const_iterator d = m_buffer.begin()+1; d != m_buffer.end(); ++d)
      {
        uint32_t n1 = ((uint32_t)*d & 0xf0) >> 4;
        uint32_t n2 = (uint32_t)*d & 0xf;

        if (n1 > 9)
        {
          if (n1 == 0xb || n1 == 0xd)
            r = "-" + r;
          break;
        }
        else
          r.push_back('0' + n1);
        if (n2 > 9)
        {
          if (n2 == 0xb || n2 == 0xd)
            r = "-" + r;
          break;
        }
        else
          r.push_back('0' + n2);
      }

      if (scale > r.length())
      {
        throw invalid_value("Invalid decimal value " + m_buffer);
      }

      if (scale > 0)
      {
        r.insert(r.length()-scale, 1, '.');
      }

      return r;
    }

    std::string to_bytes() const
    {
      return m_buffer;
    }


    static Decimal from_str(const std::string &s)
    {
      return Decimal(s);
    }

    operator std::string () const
    {
      return str();
    }

    static Decimal from_bytes(const std::string &buffer)
    {
      Decimal dec;
      dec.m_buffer = buffer;
      return dec;
    }

  private:
    /* first byte stores the scale (number of digits after '.') */
    /* then all digits in BCD */
    std::string m_buffer;
  };

}


#endif
