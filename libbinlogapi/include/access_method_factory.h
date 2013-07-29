/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/

#ifndef ACCESS_METHOD_FACTORY_INCLUDED
#define	ACCESS_METHOD_FACTORY_INCLUDED

#include "binlog_driver.h"

namespace mysql {
namespace system {
Binary_log_driver *create_transport(const char *url);
Binary_log_driver *parse_mysql_url(char *url, const char
                                   *mysql_access_method);
Binary_log_driver *parse_file_url(char *url, const char
                                  *file_access_method);
}
}

#endif	/* ACCESS_METHOD_FACTORY_H */
