# ==== Purpose ====
#
#
# Shows relevant messages in the MySQLd log output of a server. The
# messages are loaded from a specific server, $server_num. They are then
# filtered by the specified $pattern and can be limited in scope (to
# avoid repetitions). The relevant messages can be trimmed by a regex
# expression.
#
# ==== Usage ====
# --let $server_num= 1.1
# --let $pattern=%an interesting message%
# --let $limit=1
# [--let $replace_expr=/message/warning//]
#
# Parameters:
#   $server_num
#     The server number from where the messages are to be loaded
#
#   $pattern
#     The pattern of the relevant messages
#
#   $limit
#     Limits the output to this value
#
#   $replace_expr
#     Replaces the output using this regex


--disable_query_log
--echo MySQLD error output for server $server_num matching pattern $pattern
create table errlog (a int auto_increment primary key, txt text) engine=myisam;

# Avoid win path separators being interpreted as escapes
# for next char by having no escape char
--eval load data local infile "$MYSQLTEST_VARDIR/log/mysqld.$server_num.err" into table errlog columns terminated by '\n' escaped by '' (txt);

--eval delete from errlog where txt not like '$pattern';
# Trim time, win CRs and fix win path separators (or any other backslashes)
update errlog set txt= replace(replace( lower( right(txt, length(txt) - locate('[',txt) + 1)), '\r', ''), '\\', '/');
# Remove error id added as part of WL#11009: Logging services: log writers: add error ID to traditional error log
# This id is of no interest to us so it is trimmed from the output. The string functions assume that there are
# only two pairs of [] used in the log messages
update errlog set txt= concat(trim(substring_index(txt, '[', 2)),substring_index(txt, ']', -1));

if ($replace_expr)
{
  --replace_regex $replace_expr
}
--eval select txt as relevant from errlog order by a desc limit $limit

# Cleanup
--let $replace_expr=
drop table errlog;
--enable_query_log
