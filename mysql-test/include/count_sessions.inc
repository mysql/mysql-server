# include/count_sessions.inc
#
# SUMMARY
#
#    Stores the number of current sessions in $count_sessions.
#
#
# USAGE
#
#    Please look into include/wait_until_count_sessions.inc
#    for examples of typical usage.
#
#
# EXAMPLE
#    backup.test, grant3.test
#
#
# Created: 2009-01-14 mleich
#

let $count_sessions= query_get_value(SHOW GLOBAL STATUS LIKE 'Threads_connected', Value, 1);
