/*
   Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef MANIFEST_FILE_OPTION_PARSER_HELPER_INCLUDED
#define MANIFEST_FILE_OPTION_PARSER_HELPER_INCLUDED

#define FN_REFLEN 512
extern char mysql_real_data_home[FN_REFLEN];
extern char opt_plugin_dir[FN_REFLEN];

/**
  Helper class for loading keyring component
  Keyring component is loaded after minimal chassis initialization.
  At this time, home dir and plugin dir may not be initialized.

  This helper class sets them temporarily by reading configurations
  and resets them in destructor.
*/
class Manifest_file_option_parser_helper final {
 public:
  Manifest_file_option_parser_helper(int argc, char **argv);

  ~Manifest_file_option_parser_helper();

  bool valid() const { return valid_; }

 private:
  /* Ensure the backup buffers have the same size as the source ones.*/
  static constexpr size_t mysql_real_data_home_size{
      std::size(mysql_real_data_home)};
  static constexpr size_t opt_plugin_dir_size{std::size(opt_plugin_dir)};
  char save_datadir_[mysql_real_data_home_size];
  char save_plugindir_[opt_plugin_dir_size];
  bool valid_;

  static bool get_one_option(int optid, const struct my_option *opt,
                             char *argument);
};

#endif  // MANIFEST_FILE_OPTION_PARSER_HELPER_INCLUDED
