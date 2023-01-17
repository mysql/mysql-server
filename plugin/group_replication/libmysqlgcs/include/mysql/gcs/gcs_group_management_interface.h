/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#ifndef GCS_GROUP_MANAGEMENT_INTERFACE_INCLUDED
#define GCS_GROUP_MANAGEMENT_INTERFACE_INCLUDED

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"

#include <cstdint>

class Gcs_group_management_interface {
 public:
  /**
    Method that allows sending of a new group configuration. This is
    to be used when the group is blocked due to a loss of majority and
    only on one node from the new group.

    Implementations in each binding might have there specificities but some
    rules must be followed:
    - This should be a non-blocking call;
    - Typically, it shall cause a View Change, since we are forcing a new
      configuration.

    @param[in] reconfigured_group a list containing the new nodes
  */

  virtual enum_gcs_error modify_configuration(
      const Gcs_interface_parameters &reconfigured_group) = 0;

  /**
    Retrieves the minimum supported "write concurrency" value.
  */
  virtual uint32_t get_minimum_write_concurrency() const = 0;

  /**
    Retrieves the maximum supported "write concurrency" value.
  */
  virtual uint32_t get_maximum_write_concurrency() const = 0;

  /**
    Retrieves the group's "write concurrency" value.

    @param[out] write_concurrency A reference to where the group's
                "write concurrency" value will be written to

    @retval GCS_OK if successful and write_concurrency has been written to
    @retval GCS_NOK if GCS was unable to request the current "write concurrency"
                    value from the underlying implementation
  */
  virtual enum_gcs_error get_write_concurrency(
      uint32_t &write_concurrency) const = 0;

  /**
    Reconfigures the group's "write concurrency" value.

    The caller should ensure that the supplied value is between @c
    minimum_write_concurrency and @c maximum_write_concurrency.

    The method is non-blocking, meaning that it shall only send the request to
    an underlying GCS. The final result can be polled via @c
    get_write_concurrency.

    @param write_concurrency The desired "write concurrency" value.

    @retval GCS_OK if successful
    @retval GCS_NOK if unsuccessful
  */
  virtual enum_gcs_error set_write_concurrency(uint32_t write_concurrency) = 0;

  virtual ~Gcs_group_management_interface() = default;

  /**
   * @brief Reconfigures the group's "consensus leaders."
   *
   * Instructs the underlying GCS to use @c leader as the single preferred
   * consensus leader.
   *
   * @param leader The member you desire to act as a consensus leader.
   *
   * @retval GCS_OK if successful
   * @retval GCS_NOK if unsuccessful
   */
  virtual enum_gcs_error set_single_leader(
      Gcs_member_identifier const &leader) = 0;

  /**
   * @brief Reconfigures the group's "consensus leaders."
   *
   * Instructs the underlying GCS to use every member as a consensus leader.
   *
   * @retval GCS_OK if successful
   * @retval GCS_NOK if unsuccessful
   */
  virtual enum_gcs_error set_everyone_leader() = 0;

  /**
   * @brief Inspect the group's "consensus leader" configuration.
   *
   * @param[out] preferred_leaders The members specified as preferred leaders.
   * @param[out] actual_leaders The members actually carrying out the leader
   * role at this moment.
   *
   * @retval GCS_OK if successful, @c preferred_leaders and @c actual_leaders
   * contain the result
   * @retval GCS_NOK if unsuccessful
   */
  virtual enum_gcs_error get_leaders(
      std::vector<Gcs_member_identifier> &preferred_leaders,
      std::vector<Gcs_member_identifier> &actual_leaders) = 0;
};

#endif  // GCS_GROUP_MANAGEMENT_INTERFACE_INCLUDED
