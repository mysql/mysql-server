/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#ifndef DYNAMIC_LOADER_SCHEME_FILE_H
#define DYNAMIC_LOADER_SCHEME_FILE_H

#include <mysql/components/services/dynamic_loader.h>

/**
  Service for providing Components with a file:// scheme.
  See mysql_service_dynamic_loader_scheme_t.
*/
typedef SERVICE_TYPE_NO_CONST(dynamic_loader_scheme)
  SERVICE_TYPE_NO_CONST(dynamic_loader_scheme_file);

#endif /* DYNAMIC_LOADER_SCHEME_FILE_H */