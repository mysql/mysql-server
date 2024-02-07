/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef GCS_XCOM_EXPELS_IN_PROGRESS_INCLUDED
#define GCS_XCOM_EXPELS_IN_PROGRESS_INCLUDED

#include <vector>
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

/**
  @class Gcs_xcom_expels_in_progress

  The set of expels we have issued but that have not yet taken effect.
  An expel is identified as the pair (m, c), which means that we expelled the
  member `m` based on view(s) pertaining to the XCom configuration/membership
  `c`.

  Whenever we issue the expel of some member `m`, we take note in this
  structure; see @c remember_expels_issued.

  Whenever we process a view where some member `m` that we have expelled is no
  longer present, we remove `m` from this structure; see @c
  forget_expels_that_have_taken_effect.

  Please note that we keep track of the configuration ID `c` where we expelled
  `m` to guard against the possibility of removing `m` from this structure due
  to receiving a view "from the past," e.g. receiving a view from
  configuration `b` < `c` where `m` is not a member, after having expelled `m`
  due to views from configuration `c`.
  It is unclear whether XCom can sometimes deliver views "from the past," so we
  account for this situation by only removing `m` from this structure if we
  receive a view from a configuration `d` > `c` where `m` is no longer present.
*/
class Gcs_xcom_expels_in_progress {
 public:
  Gcs_xcom_expels_in_progress() = default;
  Gcs_xcom_expels_in_progress(Gcs_xcom_expels_in_progress const &) = default;
  Gcs_xcom_expels_in_progress(Gcs_xcom_expels_in_progress &&) = default;
  Gcs_xcom_expels_in_progress &operator=(Gcs_xcom_expels_in_progress const &) =
      default;
  Gcs_xcom_expels_in_progress &operator=(Gcs_xcom_expels_in_progress &&) =
      default;

  /**
   * @brief Keep track of members we expelled @c expels_issued together with
   * the XCom configuration @c config_id_where_expels_were_issued that triggered
   * the expel.
   *
   * @param config_id_where_expels_were_issued XCom configuration that triggered
   * the expel
   * @param expels_issued members expelled
   */
  void remember_expels_issued(
      synode_no const config_id_where_expels_were_issued,
      Gcs_xcom_nodes const &expels_issued);

  /**
   * @brief Forget about any expel we issued for the nodes in @c
   * members_that_left that have taken effect in the XCom configuration
   * identified by @c config_id_where_members_left.
   *
   * @param config_id_where_members_left XCom configuration where the nodes left
   * @param members_that_left nodes that left
   */
  void forget_expels_that_have_taken_effect(
      synode_no const config_id_where_members_left,
      std::vector<Gcs_member_identifier *> const &members_that_left);

  /**
   * @brief How many of the expels in progress do not pertain to suspected
   * nodes.
   *
   * @param suspected_members suspected nodes that are members in the current
   * GCS view
   * @param suspected_nonmembers suspected nodes that are not yet members in the
   * current GCS view
   * @return how many of the expels in progress do not pertain to suspected
   * nodes
   */
  std::size_t number_of_expels_not_about_suspects(
      std::vector<Gcs_member_identifier *> const &suspected_members,
      std::vector<Gcs_member_identifier *> const &suspected_nonmembers) const;

  /**
   * @brief Whether all expels in progress are for members in @c xcom_nodes.
   *
   * @param xcom_nodes XCom view
   * @retval true if all expels in progress are for members in @c xcom_nodes
   * @retval false otherwise
   */
  bool all_still_in_view(Gcs_xcom_nodes const &xcom_nodes) const;

  /**
   * @brief How many expels are in progress.
   *
   * @return how many expels are in progress
   */
  std::size_t size() const;

  /**
   * @brief Whether there is an expel in progress for @c member issued during
   * the XCom configuration identified by @c synode.
   *
   * @param member member to check
   * @param synode XCom configuration to check
   * @retval true there is an expel in progress for @c member issued during the
   * configuration identified by @c synode
   * @retval false otherwise
   */
  bool contains(Gcs_member_identifier const &member,
                synode_no const synode) const;

 private:
  std::vector<std::pair<Gcs_member_identifier, synode_no>> m_expels_in_progress;
};

#endif /* GCS_XCOM_EXPELS_IN_PROGRESS_INCLUDED */
