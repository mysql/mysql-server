/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef XA_XID_EXTRACTOR_INCLUDED
#define XA_XID_EXTRACTOR_INCLUDED

#include <iostream>
#include <string>

#include "sql/xa.h"

namespace xa {
/**
  @class XID_extractor

  Processes a string and extracts XIDs of the form  X'...',X'...',0-9.

  Extracted XIDs are stored internally and are iterable through either
  iterator or direct access semantics:

        XID_extractor tokenizer;
        tokenizer.extract("XA COMMIT X'1234',X'123456',1;"
                          "XA ROLLBACK X'1234',X'123456',1;");
        for (auto xid : tokenizer)
          std::cout << xid << std::endl << std::flush;

        if (tokenizer.size() != 0)
          std::cout << tokenizer[0] << std::endl << std::flush

  At each extraction, the internal list of extracted XIDs is cleared.
 */
class XID_extractor {
 public:
  using xid_list = std::vector<xid_t>;

  XID_extractor() = default;
  /**
    Constructs a new instance and tries to extract XIDs from the given
    string. The extracted XID will be stored internally and iterable either
    through iterator or direct access semantics.

    @param source The string containing XID to be extracted.
    @param max_extractions The maximum number of XIDs to be extracted from
                           the string.
   */
  XID_extractor(std::string const &source,
                size_t max_extractions = std::numeric_limits<size_t>::max());
  virtual ~XID_extractor() = default;

  /**
    Processes the given string and extracts well-formed XIDs.

    The extracted XID will be stored internally and iterable either through
    iterator or direct access semantics. Per each invocatian of this
    method, the internal list of extracted XIDs is cleared.

    @param source The string containing XID to be extracted.
    @param max_extractions The maximum number of XIDs to be extracted from
                           the string.

    @return the number of XIDs that were actually extracted.
   */
  size_t extract(std::string const &source,
                 size_t max_extractions = std::numeric_limits<size_t>::max());
  /**
    Retrieves an iterator pointing to the beginning of the extracted XID
    list.

    @return an iterator to the beginning of the XID list.
   */
  xid_list::iterator begin();
  /**
    Retrieves an iterator pointing to the end of the extracted XID list.

    @return an iterator to the end of the XID list.
   */
  xid_list::iterator end();
  /**
    Retrieves the size of the extracted XID list.

    @return an the size of the XID list.
   */
  size_t size();
  /**
    Retrieves the nth XID in the list of extracted XIDs.

    @param idx The index of the XID to be retrieved

    @return a reference to the XID at index idx.
   */
  xid_t &operator[](size_t idx);

 private:
  /** List of extracted XIDs. */
  xid_list m_xids;
};
}  // namespace xa
#endif  // XA_XID_EXTRACTOR_INCLUDED
