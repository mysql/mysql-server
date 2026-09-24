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
    /* The view in force when the synode is reserved: this member is node 1. */
    char const *names[]{"127.0.0.1:12341", "127.0.0.1:12342",
                        "127.0.0.1:12343"};
    node_address *na = new_node_address(3, names);

    site_def *before = new_site_def();
    init_site_def(3, na, before);
    before->start = synode_no{1, 10, 0};
    before->nodeno = 1;
    push_site_def(before);
    delete_node_address(3, na);
  }

  /* The view installed while the reservation is held: the first member is
     gone, so this one is now node 0. */
  void install_new_view() {
    char const *names[]{"127.0.0.1:12342", "127.0.0.1:12343"};
    node_address *na = new_node_address(2, names);

    site_def *after = new_site_def();
    init_site_def(2, na, after);
    after->start = synode_no{1, 20, 0};
    after->nodeno = 0;
    push_site_def(after);
    delete_node_address(2, na);
  }

  void TearDown() override { free_site_defs(); }
};

/* Under the view it was taken in, the reservation is ours. */
TEST_F(XcomStaleReservation, reservation_under_the_current_index_is_fresh) {
  install_new_view();
  synode_no const reserved{1, 15, 1};
  ASSERT_FALSE(reservation_is_stale(reserved));
}

/* After the renumbering, the same index belongs to another node. */
TEST_F(XcomStaleReservation, reservation_outliving_a_renumbering_is_stale) {
  synode_no const reserved{1, 25, 1};
  ASSERT_FALSE(reservation_is_stale(reserved));
  install_new_view();
  ASSERT_TRUE(reservation_is_stale(reserved));
}

/* A slot that carries the index this member holds now is usable. */
TEST_F(XcomStaleReservation, slot_matching_the_new_index_is_fresh) {
  install_new_view();
  synode_no const reserved{1, 25, 0};
  ASSERT_FALSE(reservation_is_stale(reserved));
}

/* Without a site there is nothing to compare against. */
TEST_F(XcomStaleReservation, unknown_site_is_not_reported_stale) {
  synode_no const other_group{99, 25, 0};
  ASSERT_FALSE(reservation_is_stale(other_group));
}

}  // namespace xcom_stale_reservation_unittest
