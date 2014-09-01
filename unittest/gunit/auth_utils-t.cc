/* Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <fstream>

#include <my_global.h>
#include <my_sys.h>
using namespace std;

namespace auth_utils_ns
{
#include "../client/auth_utils.cc"
class Auth_util : public ::testing::Test {
 protected:
  virtual void TearDown() {
    remove("auth_utils.cnf");
  }
};
TEST_F(Auth_util, parse_cnf_file)
{
  ofstream fout;
  fout.open("auth_utils.cnf");
  fout << "[mysqld]" << endl
       << "trouble= true" << endl
       << endl
       << "[client]" << endl
       << "user= thek" << endl
       << "host=localhost " << endl
       << "[another_client]" << endl
       << "user= foo" << endl
       << "host= 10.0.0.1" << endl
       << endl;
  fout.close();
  ifstream fin;
  map<string, string> options;
  fin.open("auth_utils.cnf");
  parse_cnf_file(fin, &options, "client");
  ASSERT_NE(options.end(), options.find("user"));
  ASSERT_NE(options.end(), options.find("host"));
  EXPECT_EQ((map<string, string>::size_type) 2,options.size());
  EXPECT_STREQ("thek",options["user"].c_str());
  EXPECT_STREQ("localhost",options["host"].c_str());
  fin.close();
}
}

