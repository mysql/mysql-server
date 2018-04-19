/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_view.h"

#include <stddef.h>

Gcs_view::Gcs_view(const std::vector<Gcs_member_identifier> &members,
                   const Gcs_view_identifier &view_id,
                   const std::vector<Gcs_member_identifier> &leaving,
                   const std::vector<Gcs_member_identifier> &joined,
                   const Gcs_group_identifier &group_id)
    : m_members(NULL),
      m_view_id(NULL),
      m_leaving(NULL),
      m_joined(NULL),
      m_group_id(NULL),
      m_error_code(Gcs_view::OK) {
  clone(members, view_id, leaving, joined, group_id, m_error_code);
}

Gcs_view::Gcs_view(const std::vector<Gcs_member_identifier> &members,
                   const Gcs_view_identifier &view_id,
                   const std::vector<Gcs_member_identifier> &leaving,
                   const std::vector<Gcs_member_identifier> &joined,
                   const Gcs_group_identifier &group_id,
                   Gcs_view::Gcs_view_error_code error_code)
    : m_members(NULL),
      m_view_id(NULL),
      m_leaving(NULL),
      m_joined(NULL),
      m_group_id(NULL),
      m_error_code(Gcs_view::OK) {
  clone(members, view_id, leaving, joined, group_id, error_code);
}

Gcs_view::Gcs_view(Gcs_view const &view)
    : m_members(NULL),
      m_view_id(NULL),
      m_leaving(NULL),
      m_joined(NULL),
      m_group_id(NULL),
      m_error_code(Gcs_view::OK) {
  clone(view.get_members(), view.get_view_id(), view.get_leaving_members(),
        view.get_joined_members(), view.get_group_id(), view.get_error_code());
}

void Gcs_view::clone(const std::vector<Gcs_member_identifier> &members,
                     const Gcs_view_identifier &view_id,
                     const std::vector<Gcs_member_identifier> &leaving,
                     const std::vector<Gcs_member_identifier> &joined,
                     const Gcs_group_identifier &group_id,
                     Gcs_view::Gcs_view_error_code error_code) {
  m_members = new std::vector<Gcs_member_identifier>();
  std::vector<Gcs_member_identifier>::const_iterator members_it;
  for (members_it = members.begin(); members_it != members.end();
       ++members_it) {
    m_members->push_back(Gcs_member_identifier(*members_it));
  }

  m_leaving = new std::vector<Gcs_member_identifier>();
  std::vector<Gcs_member_identifier>::const_iterator leaving_it;
  for (leaving_it = leaving.begin(); leaving_it != leaving.end();
       ++leaving_it) {
    m_leaving->push_back(Gcs_member_identifier(*leaving_it));
  }

  m_joined = new std::vector<Gcs_member_identifier>();
  std::vector<Gcs_member_identifier>::const_iterator joined_it;
  for (joined_it = joined.begin(); joined_it != joined.end(); ++joined_it) {
    m_joined->push_back(Gcs_member_identifier(*joined_it));
  }

  m_group_id = new Gcs_group_identifier(group_id);

  m_view_id = view_id.clone();

  m_error_code = error_code;
}

Gcs_view::~Gcs_view() {
  delete m_members;
  delete m_leaving;
  delete m_joined;
  delete m_group_id;
  delete m_view_id;
}

const Gcs_view_identifier &Gcs_view::get_view_id() const { return *m_view_id; }

const Gcs_group_identifier &Gcs_view::get_group_id() const {
  return *m_group_id;
}

const std::vector<Gcs_member_identifier> &Gcs_view::get_members() const {
  return *m_members;
}

const std::vector<Gcs_member_identifier> &Gcs_view::get_leaving_members()
    const {
  return *m_leaving;
}

const std::vector<Gcs_member_identifier> &Gcs_view::get_joined_members() const {
  return *m_joined;
}

Gcs_view::Gcs_view_error_code Gcs_view::get_error_code() const {
  return m_error_code;
}

const Gcs_member_identifier *Gcs_view::get_member(
    const std::string &member_id) const {
  std::vector<Gcs_member_identifier>::const_iterator members_it;
  for (members_it = m_members->begin(); members_it != m_members->end();
       ++members_it) {
    if ((*members_it).get_member_id() == member_id) {
      return &(*members_it);
    }
  }

  return NULL;
}

bool Gcs_view::has_member(const std::string &member_id) const {
  return get_member(member_id) != NULL;
}
