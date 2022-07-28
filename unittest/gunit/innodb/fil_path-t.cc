/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include <gtest/gtest.h>
#include <stddef.h>

#include "storage/innobase/include/fil0fil.h"
extern bool lower_case_file_system;

namespace innodb_fil_path_unittest {

static constexpr char SEP = Fil_path::OS_SEPARATOR;

TEST(fil_path, is_absolute_path) {
  Fil_path current_dir(".");
  auto abs_current_dir_str = current_dir.abs_path();
  EXPECT_FALSE(current_dir.is_absolute_path());
  EXPECT_EQ(abs_current_dir_str.find_last_of(SEP),
            abs_current_dir_str.size() - 1);

  Fil_path abs_current_dir(abs_current_dir_str);
  EXPECT_TRUE(abs_current_dir.is_absolute_path());
  EXPECT_EQ(abs_current_dir_str, abs_current_dir.abs_path());
}

void get_existing_path_subtest(std::string abs_path, std::string sub_path) {
  std::string ghost;
  EXPECT_EQ(Fil_path::get_existing_path(abs_path + sub_path, ghost), abs_path);
  EXPECT_EQ(ghost, sub_path);
  EXPECT_EQ(Fil_path::get_existing_path(abs_path + sub_path + SEP, ghost),
            abs_path);
  EXPECT_EQ(ghost, sub_path + SEP);
  EXPECT_EQ(Fil_path::get_existing_path(abs_path + sub_path + SEP + SEP, ghost),
            abs_path);
  EXPECT_EQ(ghost, sub_path + SEP + SEP);
}

static std::string ghost = "ghost";
static std::string ghost2 = ghost + SEP + "ghost2";
static std::string some_ibd = ghost2 + SEP + "some.ibd";
static std::string someibd = ghost2 + SEP + "someibd";
static std::string some_txt = ghost2 + SEP + "some.txt";
static std::string dot_t_dot_t = ghost2 + SEP + "some.t.t";
static std::string dot_t_sep_t = ghost2 + SEP + "some.t" + SEP + "t";

static std::string Ghost = "Ghost";
static std::string Ghost2 = Ghost + SEP + "Ghost2";
static std::string Some_Ibd = Ghost2 + SEP + "Some.Ibd";
static std::string Someibd = Ghost2 + SEP + "Someibd";
static std::string Some_Txt = Ghost2 + SEP + "Some.Txt";
static std::string Dot_t_dot_t = ghost2 + SEP + "Some.t.t";
static std::string Dot_t_sep_t = ghost2 + SEP + "Some.t" + SEP + "t";

TEST(fil_path, get_existing_path) {
  Fil_path current_dir(".");
  auto abs_current_dir_str = current_dir.abs_path();
  EXPECT_FALSE(current_dir.is_absolute_path());
  EXPECT_EQ(abs_current_dir_str.find_last_of(SEP),
            abs_current_dir_str.size() - 1);

  get_existing_path_subtest(abs_current_dir_str, ghost);
  get_existing_path_subtest(abs_current_dir_str, ghost2);
  get_existing_path_subtest(abs_current_dir_str, ghost2 + SEP);
  get_existing_path_subtest(abs_current_dir_str, ghost2 + SEP + SEP);

  get_existing_path_subtest(abs_current_dir_str, some_ibd);

  get_existing_path_subtest(abs_current_dir_str, some_txt);
}

void get_real_path_subsubtest(std::string abs_path, std::string sub_path,
                              std::string expect_path, bool expect_a_file) {
  std::string expected_suffix = expect_a_file ? "" : std::string{SEP};
  std::string relative_path = std::string{"."} + SEP;

  EXPECT_EQ(Fil_path::get_real_path(relative_path + sub_path, false),
            abs_path + expect_path + expected_suffix);
  EXPECT_EQ(Fil_path::get_real_path(abs_path + sub_path, false),
            abs_path + expect_path + expected_suffix);

  EXPECT_EQ(Fil_path::get_real_path(relative_path + sub_path + SEP, false),
            abs_path + expect_path + expected_suffix);
  EXPECT_EQ(Fil_path::get_real_path(abs_path + sub_path + SEP, false),
            abs_path + expect_path + expected_suffix);
}

void get_real_path_subtest(std::string abs_path,
                           bool expect_ghost2_file = false,
                           bool expect_ibd_file = true,
                           bool expect_txt_file = true) {
  get_real_path_subsubtest(abs_path, ghost, ghost, false);
  get_real_path_subsubtest(abs_path, ghost2, ghost2, expect_ghost2_file);
  get_real_path_subsubtest(abs_path, some_ibd, some_ibd, expect_ibd_file);
  get_real_path_subsubtest(abs_path, someibd, someibd, false);
  get_real_path_subsubtest(abs_path, some_txt, some_txt, expect_txt_file);
  get_real_path_subsubtest(abs_path, dot_t_dot_t, dot_t_dot_t, false);
  get_real_path_subsubtest(abs_path, dot_t_sep_t, dot_t_sep_t, false);

  if (lower_case_file_system) {
    get_real_path_subsubtest(abs_path, Ghost, ghost, false);
    get_real_path_subsubtest(abs_path, Ghost2, ghost2, expect_ghost2_file);
    get_real_path_subsubtest(abs_path, Some_Ibd, some_ibd, expect_ibd_file);
    get_real_path_subsubtest(abs_path, Someibd, someibd, false);
    get_real_path_subsubtest(abs_path, Some_Txt, some_txt, expect_txt_file);
    get_real_path_subsubtest(abs_path, Dot_t_dot_t, dot_t_dot_t, false);
    get_real_path_subsubtest(abs_path, Dot_t_sep_t, dot_t_sep_t, false);
  }
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
#ifndef _WIN32
  std::string root{SEP};
  EXPECT_EQ(Fil_path::get_real_path(root), root);
#endif

  Fil_path current_dir(".");
  auto abs_current_dir_str = current_dir.abs_path();
  EXPECT_FALSE(current_dir.is_absolute_path());
  EXPECT_EQ(abs_current_dir_str.find_last_of(SEP),
            abs_current_dir_str.size() - 1);

  std::string relative_path = std::string{"."} + SEP;

  EXPECT_EQ(Fil_path::get_real_path("."), abs_current_dir_str);
  EXPECT_EQ(Fil_path::get_real_path(relative_path), abs_current_dir_str);
  EXPECT_EQ(Fil_path::get_real_path(abs_current_dir_str), abs_current_dir_str);

  // Run the test where only the current dir exists.
  get_real_path_subtest(abs_current_dir_str);

  // Make a sub-directory called 'non-existing'
  mkdir(abs_current_dir_str + ghost);
  get_real_path_subtest(abs_current_dir_str);

  // Make a file called 'ghost2'
  create_file(abs_current_dir_str + ghost2);
  get_real_path_subtest(abs_current_dir_str, true);
  unlink(abs_current_dir_str + ghost2);

  // Make a sub-directory called 'ghost2'
  mkdir(abs_current_dir_str + ghost2);
  get_real_path_subtest(abs_current_dir_str);

  // Make a file called 'some.ibd'
  create_file(abs_current_dir_str + some_ibd);
  get_real_path_subtest(abs_current_dir_str);
  unlink(abs_current_dir_str + some_ibd);

  // Make a sub-directory called 'some.ibd'
  mkdir(abs_current_dir_str + some_ibd);
  get_real_path_subtest(abs_current_dir_str, false, false);
  rmdir(abs_current_dir_str + some_ibd);

  // Make a sub-directory called 'someibd'
  mkdir(abs_current_dir_str + someibd);
  get_real_path_subtest(abs_current_dir_str);
  rmdir(abs_current_dir_str + someibd);

  // Make a file called 'some.txt'
  create_file(abs_current_dir_str + some_txt);
  get_real_path_subtest(abs_current_dir_str);
  unlink(abs_current_dir_str + some_txt);

  // Make a sub-directory called 'some.txt'
  mkdir(abs_current_dir_str + some_txt);
  get_real_path_subtest(abs_current_dir_str, false, true, false);
  rmdir(abs_current_dir_str + some_txt);

  // Make a sub-directory called 'dot.t.t'
  mkdir(abs_current_dir_str + dot_t_dot_t);
  get_real_path_subtest(abs_current_dir_str);
  rmdir(abs_current_dir_str + dot_t_dot_t);

  rmdir(abs_current_dir_str + ghost2);
  rmdir(abs_current_dir_str + ghost);
}

}  // namespace innodb_fil_path_unittest
