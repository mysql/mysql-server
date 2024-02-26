RUN SELECT attr_name,  IF(LENGTH(attr_value) >0, 'not empty', 'EMPTY!') AS attr_value  FROM performance_schema.session_connect_attrs  WHERE processlist_id = $SESSION_ID AND attr_name NOT LIKE '_thread'  ORDER BY ordinal_position
attr_name	attr_value
_client_name	not empty
_client_version	not empty
_os	not empty
_platform	not empty
_client_license	not empty
_pid	not empty
program_name	not empty
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
RUN SELECT attr_name,  IF(LENGTH(attr_value) >0, 'not empty', 'EMPTY!') AS attr_value  FROM performance_schema.session_connect_attrs  WHERE processlist_id = $SESSION_ID AND attr_name NOT LIKE '_thread'  ORDER BY ordinal_position
attr_name	attr_value
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
