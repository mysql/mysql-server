/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */



enum plugin_log_level
{
  MY_ERROR_LEVEL,
  MY_WARNING_LEVEL,
  MY_INFORMATION_LEVEL
};

#ifdef __cplusplus
extern "C" {
#endif

int my_plugin_log_message(void *, enum plugin_log_level, const char *, ...)
{
  return 0;
}

struct my_plugin_log_service
{
  /** write a message to the log */
  int (*my_plugin_log_message)(void *, enum plugin_log_level, const char *, ...);
};

struct my_plugin_log_service log_service = {my_plugin_log_message};
struct my_plugin_log_service *my_plugin_log_service = &log_service;

#ifdef __cplusplus
}
#endif
