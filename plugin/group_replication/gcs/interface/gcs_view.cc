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

#include "gcs_view.h"

Gcs_view::Gcs_view(vector<Gcs_member_identifier> *members,
                   Gcs_view_identifier *view_id,
                   vector<Gcs_member_identifier> *left,
                   vector<Gcs_member_identifier> *joined,
                   Gcs_group_identifier *group_id)
{
  this->members= members;
  this->view_id= view_id;
  this->leaving= left;
  this->joined= joined;
  this->group_id= group_id;
}

Gcs_view::~Gcs_view()
{
  if(this->members != NULL)
    delete this->members;

  if(this->view_id != NULL)
    delete this->view_id;

  if(this->leaving != NULL)
    delete this->leaving;

  if(this->joined != NULL)
    delete this->joined;

  if(this->group_id != NULL)
    delete this->group_id;
}


Gcs_view_identifier* Gcs_view::get_view_id()
{
  return view_id;
}

Gcs_group_identifier* Gcs_view::get_group_id()
{
  return group_id;
}

vector<Gcs_member_identifier>* Gcs_view::get_members()
{
  return members;
}

vector<Gcs_member_identifier>* Gcs_view::get_leaving_members()
{
  return leaving;
}

vector<Gcs_member_identifier>* Gcs_view::get_joined_members()
{
  return joined;
}
