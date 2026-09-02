/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#include <xcom/xcom_profile.h>
#include <xcom_vp.h>
#include "gcs_base_test.h"

#include "site_def.h"
#include "xcom_base.h"

namespace xcom_stale_reservation_unittest {

/* Bug#121063: a synode reserved under one node index must be recognised as no
   longer ours once a view change gives this member a different index. */
class XcomStaleReservation : public GcsBaseTest {
 protected:
  void SetUp() override {
    m_addr = new std::string("127.0.0.1:12345");
    char const *names[]{m_addr->c_str()};
    m_na = new_node_address(1, names);

    /* The view in force when the synode is reserved: this member is node 1. */
    m_before = new_site_def();
    init_site_def(1, m_na, m_before);
    m_before->start = m_synode_before;
    m_before->nodeno = 1;
    push_site_def(m_before);

    /* The view installed while the reservation is held: the member is now
       node 0. */
    m_after = new_site_def();
    init_site_def(1, m_na, m_after);
    m_after->start = m_synode_after;
    m_after->nodeno = 0;
    push_site_def(m_after);
  }

  void TearDown() override {
    push_site_def(nullptr);
    free_site_defs();
    delete_node_address(1, m_na);
    delete m_addr;
  }

  std::string *m_addr{nullptr};
  node_address *m_na{nullptr};
  site_def *m_before{nullptr};
  site_def *m_after{nullptr};

  /* A view is active from its start synode on, so a slot below 20 falls under
     the old view and one at or above it under the new one. */
  synode_no const m_synode_before{1, 10, 0};
  synode_no const m_synode_after{1, 20, 0};
};

/* Under the view it was taken in, the reservation is ours. */
TEST_F(XcomStaleReservation, reservation_under_the_current_index_is_fresh) {
  synode_no const reserved{1, 15, 1};
  ASSERT_FALSE(reservation_is_stale(reserved));
}

/* After the renumbering, the same index belongs to another node. */
TEST_F(XcomStaleReservation, reservation_outliving_a_renumbering_is_stale) {
  synode_no const reserved{1, 25, 1};
  ASSERT_TRUE(reservation_is_stale(reserved));
}

/* A slot that carries the index this member holds now is usable. */
TEST_F(XcomStaleReservation, slot_matching_the_new_index_is_fresh) {
  synode_no const reserved{1, 25, 0};
  ASSERT_FALSE(reservation_is_stale(reserved));
}

/* Without a site there is nothing to compare against. */
TEST_F(XcomStaleReservation, unknown_site_is_not_reported_stale) {
  synode_no const other_group{99, 25, 1};
  ASSERT_FALSE(reservation_is_stale(other_group));
}

}  // namespace xcom_stale_reservation_unittest
