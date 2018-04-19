## Script connection_test_helpers.inc
#
# Extracted queries form 'connection.test' that
# were executed multiple times.
#
# Usage:
#
# [--let $skip_show_mysqlx_sessions_variables=1|0|]
# --source connection_test_helpers.inc
#

show status like 'Mysqlx_connections_closed';
show status like 'Mysqlx_connections_accepted';
show status like 'Mysqlx_connection_accept_errors';
# show status like 'Mysqlx_connection_errors'; Bug #21782329

if (!$skip_show_mysqlx_sessions_variables)
{
  show status where
      `Variable_name`like  'Mysqlx_sessions%' and
       not `Variable_name` like  'Mysqlx_sessions_fatal_error';
}

--let $skip_show_mysqlx_sessions_variables=
