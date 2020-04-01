/* Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.

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

/* See http://code.google.com/p/googletest/wiki/Primer */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <gtest/gtest.h>
#include <stddef.h>

#include "storage/innobase/include/fil0fil.h"

namespace innodb_fil_path_unittest {

TEST(fil_path, is_absolute_path) {
  Fil_path current_dir(".");
  auto abs_current_dir_str = current_dir.abs_path();
  EXPECT_FALSE(current_dir.is_absolute_path());
  EXPECT_EQ(abs_current_dir_str.find_last_of(Fil_path::OS_SEPARATOR),
            abs_current_dir_str.size() - 1);

  Fil_path abs_current_dir(abs_current_dir_str);
  EXPECT_TRUE(abs_current_dir.is_absolute_path());
  EXPECT_EQ(abs_current_dir_str, abs_current_dir.abs_path());
}

void get_existing_path_subtest(std::string abs_path, std::string sub_path) {
  std::string ghost;
  EXPECT_EQ(Fil_path::get_existing_path(abs_path + sub_path, ghost), abs_path);
  EXPECT_EQ(ghost, sub_path);

  EXPECT_EQ(Fil_path::get_existing_path(
                abs_path + sub_path + Fil_path::OS_SEPARATOR, ghost),
            abs_path);
  EXPECT_EQ(ghost, sub_path + Fil_path::OS_SEPARATOR);

  EXPECT_EQ(
      Fil_path::get_existing_path(
          abs_path + sub_path + Fil_path::OS_SEPARATOR + Fil_path::OS_SEPARATOR,
          ghost),
      abs_path);
  EXPECT_EQ(ghost, sub_path + Fil_path::OS_SEPARATOR + Fil_path::OS_SEPARATOR);
}

static std::string non_existing = "non_existing";
static std::string non2 = non_existing + Fil_path::OS_SEPARATOR + "non2";
static std::string some_ibd = non2 + Fil_path::OS_SEPARATOR + "some.ibd";
static std::string saibd_dir =
    non2 + Fil_path::OS_SEPARATOR + "dir_named_saibd";
static std::string some_txt = non2 + Fil_path::OS_SEPARATOR + "some.txt";

TEST(fil_path, get_existing_path) {
  Fil_path current_dir(".");
  auto abs_current_dir_str = current_dir.abs_path();
  EXPECT_FALSE(current_dir.is_absolute_path());
  EXPECT_EQ(abs_current_dir_str.find_last_of(Fil_path::OS_SEPARATOR),
            abs_current_dir_str.size() - 1);

  get_existing_path_subtest(abs_current_dir_str, non_existing);
  get_existing_path_subtest(abs_current_dir_str, non2);
  get_existing_path_subtest(abs_current_dir_str, non2 + Fil_path::OS_SEPARATOR);
  get_existing_path_subtest(abs_current_dir_str, non2 + Fil_path::OS_SEPARATOR +
                                                     Fil_path::OS_SEPARATOR);

  get_existing_path_subtest(abs_current_dir_str, some_ibd);

  get_existing_path_subtest(abs_current_dir_str, some_txt);
}

void get_real_path_subsubtest(std::string abs_path, std::string sub_path,
                              std::string expect_path, bool expect_dir) {
  std::string expected_suffix =
      expect_dir ? std::string{Fil_path::OS_SEPARATOR} : "";
  std::string relative_path = std::string{"."} + Fil_path::OS_SEPARATOR;

  EXPECT_EQ(Fil_path::get_real_path(relative_path + sub_path, false),
            abs_path + expect_path + expected_suffix);
  EXPECT_EQ(Fil_path::get_real_path(abs_path + sub_path, false),
            abs_path + expect_path + expected_suffix);

  EXPECT_EQ(Fil_path::get_real_path(
                relative_path + sub_path + Fil_path::OS_SEPARATOR, false),
            abs_path + expect_path + Fil_path::OS_SEPARATOR);
  EXPECT_EQ(Fil_path::get_real_path(
                abs_path + sub_path + Fil_path::OS_SEPARATOR, false),
            abs_path + expect_path + Fil_path::OS_SEPARATOR);
}

void get_real_path_subtest(std::string abs_path, bool expect_non2_file = false,
                           bool expect_some_txt_file = false) {
  get_real_path_subsubtest(abs_path, non_existing, non_existing, true);

  get_real_path_subsubtest(abs_path, non2, non2, !expect_non2_file);
  get_real_path_subsubtest(abs_path, some_ibd, some_ibd, false);
  get_real_path_subsubtest(abs_path, saibd_dir, saibd_dir, true);
  get_real_path_subsubtest(abs_path, some_txt, some_txt, !expect_some_txt_file);
}

void mkdir(std::string path) {
  EXPECT_EQ(::my_mkdir(path.c_str(), 0777, MYF(0)), 0);
}

void rmdir(std::string path) { EXPECT_EQ(::rmdir(path.c_str()), 0); }
void unlink(std::string path) { EXPECT_EQ(::unlink(path.c_str()), 0); }

void create_file(std::string path) {
  auto f = fopen(path.c_str(), "w");
  EXPECT_NE(f, nullptr);
  fclose(f);
}

TEST(fil_path, get_real_path) {
  Fil_path current_dir(".");
  auto abs_current_dir_str = current_dir.abs_path();
  EXPECT_FALSE(current_dir.is_absolute_path());
  EXPECT_EQ(abs_current_dir_str.find_last_of(Fil_path::OS_SEPARATOR),
            abs_current_dir_str.size() - 1);

  std::string relative_path = std::string{"."} + Fil_path::OS_SEPARATOR;

  EXPECT_EQ(Fil_path::get_real_path("."), abs_current_dir_str);
  EXPECT_EQ(Fil_path::get_real_path(relative_path), abs_current_dir_str);
  EXPECT_EQ(Fil_path::get_real_path(abs_current_dir_str), abs_current_dir_str);

  get_real_path_subtest(abs_current_dir_str);

  mkdir(abs_current_dir_str + non_existing);
  get_real_path_subtest(abs_current_dir_str);

  create_file(abs_current_dir_str + non2);
  get_real_path_subtest(abs_current_dir_str, true);
  unlink(abs_current_dir_str + non2);

  mkdir(abs_current_dir_str + non2);
  get_real_path_subtest(abs_current_dir_str);

  create_file(abs_current_dir_str + some_ibd);
  get_real_path_subtest(abs_current_dir_str);
  unlink(abs_current_dir_str + some_ibd);

  create_file(abs_current_dir_str + some_txt);
  get_real_path_subtest(abs_current_dir_str, false, true);
  unlink(abs_current_dir_str + some_txt);

  rmdir(abs_current_dir_str + non2);
  rmdir(abs_current_dir_str + non_existing);
}

}  // namespace innodb_fil_path_unittest
