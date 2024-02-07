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

#include <xcom/xcom_profile.h>
#include <xcom_vp.h>
#include "gcs_base_test.h"

#include "site_def.h"

namespace xcom_site_def_unittest {

class XcomSiteDef : public GcsBaseTest {
 protected:
  XcomSiteDef() = default;
  ~XcomSiteDef() override = default;
};

TEST_F(XcomSiteDef, config_max_boot_key) {
  synode_no const synode_0_1_0{0, 1, 0};
  synode_no const synode_0_2_0{0, 2, 0};
  synode_no const synode_0_3_0{0, 3, 0};
  synode_no const synode_0_3_1{0, 3, 1};
  synode_no const synode_1_2_0{1, 2, 0};
  gcs_snapshot gcs_snap{null_synode, null_synode, {0, nullptr}, {{0, nullptr}}};
  synode_no max_boot_key = null_synode;

  /* `max_boot_key` of empty snapshot is `null_synode`. */
  max_boot_key = config_max_boot_key(&gcs_snap);
  ASSERT_TRUE(synode_eq(max_boot_key, null_synode));

  /* `max_boot_key` of snapshot with one config is that config's `boot_key`. */
  config one_cfg{null_synode,  synode_0_1_0,      {0, nullptr},
                 {0, nullptr}, EVENT_HORIZON_MIN, 0,
                 {0, nullptr}};
  config_ptr one_cfg_ptr = &one_cfg;
  gcs_snap.cfg.configs_len = 1;
  gcs_snap.cfg.configs_val = &one_cfg_ptr;
  max_boot_key = config_max_boot_key(&gcs_snap);
  ASSERT_TRUE(synode_eq(max_boot_key, synode_0_1_0));

  /* `max_boot_key` of snapshot with various configs is highest `boot_key` of a
     config... */
  config three_cfg[3] = {
      {null_synode,
       synode_0_2_0,
       {0, nullptr},
       {0, nullptr},
       EVENT_HORIZON_MIN,
       0,
       {0, nullptr}},
      {null_synode,
       synode_0_3_1,
       {0, nullptr},
       {0, nullptr},
       EVENT_HORIZON_MIN,
       0,
       {0, nullptr}},
      {null_synode,
       synode_0_3_0,
       {0, nullptr},
       {0, nullptr},
       EVENT_HORIZON_MIN,
       0,
       {0, nullptr}},
  };
  config_ptr three_cfg_ptr[3] = {&three_cfg[0], &three_cfg[1], &three_cfg[2]};
  gcs_snap.cfg.configs_len = 3;
  gcs_snap.cfg.configs_val = three_cfg_ptr;
  max_boot_key = config_max_boot_key(&gcs_snap);
  ASSERT_TRUE(synode_eq(max_boot_key, synode_0_3_1));

  /* ...whose group_id's match the snapshot, otherwise `max_boot_key` is
     `null_synode`. */
  config two_cfg[2] = {
      {null_synode,
       synode_1_2_0,
       {0, nullptr},
       {0, nullptr},
       EVENT_HORIZON_MIN,
       0,
       {0, nullptr}},
      {null_synode,
       synode_0_3_0,
       {0, nullptr},
       {0, nullptr},
       EVENT_HORIZON_MIN,
       0,
       {0, nullptr}},
  };
  config_ptr two_cfg_ptr[2] = {&two_cfg[0], &two_cfg[1]};
  gcs_snap.cfg.configs_len = 2;
  gcs_snap.cfg.configs_val = two_cfg_ptr;
  gcs_snap.log_start.group_id = 1;
  max_boot_key = config_max_boot_key(&gcs_snap);
  ASSERT_TRUE(synode_eq(max_boot_key, synode_1_2_0));

  gcs_snap.log_start.group_id = 2;
  max_boot_key = config_max_boot_key(&gcs_snap);
  ASSERT_TRUE(synode_eq(max_boot_key, null_synode));
}

}  // namespace xcom_site_def_unittest
