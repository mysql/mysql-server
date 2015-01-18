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

#ifndef GCS_VIEW_INCLUDED
#define GCS_VIEW_INCLUDED

#include "gcs_group_identifier.h"
#include "gcs_view_identifier.h"
#include "gcs_member_identifier.h"

#include <vector>

using std::vector;

/**
  @class Gcs_view

  This represents the membership view that a node has from a group
 */
class Gcs_view
{
public:

  /**
    Gcs_view constructor

    @param[in] members group members
    @param[in] view_id the view identifier
    @param[in] left the members that left from the previous view
    @param[in] joined the new members
    @param[in] group_id the group identifier
   */
  Gcs_view(vector<Gcs_member_identifier> *members,
           Gcs_view_identifier *view_id,
           vector<Gcs_member_identifier> *leaving,
           vector<Gcs_member_identifier> *joined,
           Gcs_group_identifier *group_id);

  virtual ~Gcs_view();

  /**
    @return the current view identifier. This identifier marks a snapshot in
            time and should increase monotonically
   */
  Gcs_view_identifier* get_view_id();

  /**
    @return the group where this view pertains
   */
  Gcs_group_identifier* get_group_id();

  /**
    @return the totality of members that currently belong to this group in a
            certain moment in time, denoted by view_id
   */
  vector<Gcs_member_identifier>* get_members();

  /**
    @return the members that left from the view n-1 to the current view n
   */
  vector<Gcs_member_identifier>* get_leaving_members();

  /**
    @return the new members in view from view n-1 to the current view n
   */
  vector<Gcs_member_identifier>* get_joined_members();

private:
  vector<Gcs_member_identifier> *members;
  Gcs_view_identifier *view_id;
  vector<Gcs_member_identifier> *leaving;
  vector<Gcs_member_identifier> *joined;
  Gcs_group_identifier *group_id;
};

#endif // GCS_VIEW_INCLUDED
