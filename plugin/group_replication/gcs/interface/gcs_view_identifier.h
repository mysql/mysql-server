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

#ifndef GCS_VIEW_IDENTIFIER_INCLUDED
#define GCS_VIEW_IDENTIFIER_INCLUDED

/**
  @class Gcs_view_identifier

  This represents the identification of a certain view of a certain group in a
  moment in time.

  This identifier must increase monotonically, in order to provide the
  simplest notion. This can read as "view n-1 happened before view n"
 */
class Gcs_view_identifier
{
public:

  virtual ~Gcs_view_identifier(){}

  virtual char* get_representation() = 0;
};

#endif // GCS_VIEW_IDENTIFIER_INCLUDED
