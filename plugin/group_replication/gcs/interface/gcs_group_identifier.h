/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef GCS_GROUP_IDENTIFIER_INCLUDED
#define GCS_GROUP_IDENTIFIER_INCLUDED

#include <string>

using std::string;

/**
  @class Gcs_group_identifier

  This represents the unique identification of a group. The group uniqueness
  is relevant since it is the destination of data, thus, it cannot be ambiguous.
 */
class Gcs_group_identifier
{
public:
  /**
    Gcs_group_identifier constructor

    @param[in] group_id the group identifier
   */
  Gcs_group_identifier(string group_id);
  ~Gcs_group_identifier();

  /**
    @return the group identifier
   */
  string get_group_id();

private:
  string group_id;
};

#endif // GCS_GROUP_IDENTIFIER_INCLUDED
