/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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

#ifndef GCS_VIEW_INCLUDED
#define GCS_VIEW_INCLUDED

#include <string>
#include <vector>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_group_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_view_identifier.h"

/**
  @class Gcs_view

  This represents the membership view that a member has from a group.

  This objects contains:
  - A list of members that belong to this view.
  - A Gcs_view_identifier identifier object, that uniquely identifies
    this view in time.
  - The members that were in the last view and that left. This shall
    include the local member when it leaves the group.
  - The members that joined, meaning that they were not present in the
    previous view. This includes the local member when it joins a group,
    meaning that a member can be in the list of members and joined.
  - A Gcs_group_identifier to which this view pertains.

  There are two ways to obtain this:
  - In the Gcs_control_interface, one should call, at any moment, the
    method Gcs_control_interface::get_current_view.
  - Cache the value received via Gcs_control_event_listener::on_view_changed.
*/
class Gcs_view {
 public:
  /**
    Define error codes associated to the view.
  */
  enum Gcs_view_error_code { OK = 1, MEMBER_EXPELLED };

  /**
    Gcs_view constructor.

    @param[in] members group members
    @param[in] view_id the view identifier
    @param[in] leaving the members that left from the previous view
    @param[in] joined the new members
    @param[in] group_id the group identifier
  */

  explicit Gcs_view(const std::vector<Gcs_member_identifier> &members,
                    const Gcs_view_identifier &view_id,
                    const std::vector<Gcs_member_identifier> &leaving,
                    const std::vector<Gcs_member_identifier> &joined,
                    const Gcs_group_identifier &group_id);

  /**
    @param[in] members group members
    @param[in] view_id the view identifier
    @param[in] leaving the members that left from the previous view
    @param[in] joined the new members
    @param[in] group_id the group identifier
    @param[in] error_code error code associated to the view.
  */

  explicit Gcs_view(const std::vector<Gcs_member_identifier> &members,
                    const Gcs_view_identifier &view_id,
                    const std::vector<Gcs_member_identifier> &leaving,
                    const std::vector<Gcs_member_identifier> &joined,
                    const Gcs_group_identifier &group_id,
                    Gcs_view::Gcs_view_error_code error_code);

  /**
    Gcs_view constructor which does a deep copy of the object passed
    as parameter.

    @param[in] view reference to a Gcs_view object
  */

  explicit Gcs_view(Gcs_view const &view);

  virtual ~Gcs_view();

  /**
    @return the current view identifier. This identifier marks a snapshot in
            time and should increase monotonically
  */

  const Gcs_view_identifier &get_view_id() const;

  /**
    @return the group where this view pertains
  */

  const Gcs_group_identifier &get_group_id() const;

  /**
    @return the totality of members that currently belong to this group in a
            certain moment in time, denoted by view_id
  */

  const std::vector<Gcs_member_identifier> &get_members() const;

  /**
    @return the members that left from the view n-1 to the current view n
  */

  const std::vector<Gcs_member_identifier> &get_leaving_members() const;

  /**
    @return the new members in view from view n-1 to the current view n
  */

  const std::vector<Gcs_member_identifier> &get_joined_members() const;

  /**
    @return error code associated to the current view.
  */

  Gcs_view::Gcs_view_error_code get_error_code() const;

  /*
    @param[in] address Member's identifier which is usually its address
    @return the member whose identifier matches the one provided as
            parameter
  */

  const Gcs_member_identifier *get_member(const std::string &member_id) const;

  /*
    @param[in] member_id Member's identifier which is usually its address
    @return whether there is a member whose identifier matches the one
            provided as parameter
  */

  bool has_member(const std::string &member_id) const;

 private:
  std::vector<Gcs_member_identifier> *m_members;
  Gcs_view_identifier *m_view_id;
  std::vector<Gcs_member_identifier> *m_leaving;
  std::vector<Gcs_member_identifier> *m_joined;
  Gcs_group_identifier *m_group_id;
  Gcs_view::Gcs_view_error_code m_error_code;

  /*
    Auxiliary function used by constructors.
  */
  void clone(const std::vector<Gcs_member_identifier> &members,
             const Gcs_view_identifier &view_id,
             const std::vector<Gcs_member_identifier> &leaving,
             const std::vector<Gcs_member_identifier> &joined,
             const Gcs_group_identifier &group_id,
             Gcs_view::Gcs_view_error_code error_code);

  /*
    Disabling the assignment operator.
  */
  Gcs_view &operator=(Gcs_view const &);
};

#endif  // GCS_VIEW_INCLUDED
