/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#pragma once

#include "mysql/components/component_implementation.h"
#include "mysql/components/services/mysql_command_services.h"

namespace hm {

extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_factory, cmd_factory_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_thread, cmd_thread_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_options, cmd_options_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query, cmd_query_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query_result,
                                       cmd_query_result_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_info,
                                       cmd_field_info_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_error_info,
                                       cmd_error_info_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_metadata,
                                       cmd_field_meta_srv);

}  // namespace hm
