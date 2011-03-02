# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

# See related script table_io_setup_helper.inc

# Stop table io recording
update performance_schema.setup_consumers set enabled='NO';
eval select event_name,
  left(source, locate(":", source)) as short_source,
  object_type, object_schema,
  if (locate("#sql-", object_name), "#sql-XXXX", object_name)
    as pretty_name,
  operation, number_of_bytes
  from performance_schema.events_waits_history_long
  where event_name like 'wait/io/table/%'
  and object_schema in ($schema_to_dump)
  order by thread_id, event_id;

# In case of failures, this will tell if table io are lost.
show status like 'performance_schema_%';

# Cleanup
truncate performance_schema.events_waits_history_long;
flush status;

