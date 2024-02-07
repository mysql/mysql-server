/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RAII_TARGETED_STRINGSTREAM_H_INCLUDED
#define RAII_TARGETED_STRINGSTREAM_H_INCLUDED

#include <functional>
#include <sstream>
#include <string>

namespace raii {

/// Like std::stringstream, copying to a given target string at
/// destruction.
///
/// This is a convenience helper, allowing the use of:
/// @code
///   Targeted_stringstream(s) << foo << bar << baz;
/// @endcode
/// instead of:
/// @code
///   std::stringstream stream;
///   stream << foo << bar << baz;
///   s.assign(stream.str());
/// @endcode
/// This can, for instance, be used by a class to export a stream
/// interface to update private string members.
class Targeted_stringstream {
 public:
  explicit Targeted_stringstream(
      std::string &target, const std::string &suffix = "",
      const std::function<void(const std::string &)> &callback = nullptr);
  Targeted_stringstream(const Targeted_stringstream &) = delete;
  Targeted_stringstream(Targeted_stringstream &&other) noexcept;
  Targeted_stringstream &operator=(const Targeted_stringstream &) = delete;
  Targeted_stringstream &operator=(Targeted_stringstream &&other) noexcept;
  ~Targeted_stringstream();
  template <class T>
  friend Targeted_stringstream &operator<<(Targeted_stringstream &stream,
                                           const T &value);
  template <class T>
  friend Targeted_stringstream &operator<<(Targeted_stringstream &&stream,
                                           const T &value);

 private:
  bool m_active;
  std::string &m_target;
  std::string m_suffix;
  std::ostringstream m_stream;
  std::function<void(std::string &)> m_callback;
};

template <class T>
Targeted_stringstream &operator<<(Targeted_stringstream &&stream,
                                  const T &value) {
  stream.m_stream << value;
  return stream;
}

template <class T>
Targeted_stringstream &operator<<(Targeted_stringstream &stream,
                                  const T &value) {
  stream.m_stream << value;
  return stream;
}

}  // namespace raii

#endif  // ifndef RAII_TARGETED_STRINGSTREAM_H_INCLUDED
