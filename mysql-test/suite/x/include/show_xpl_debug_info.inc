## Script show_xpl_debug_info.inc
#  This scripts shows state of server, which might be helpful
#  while debugging an issue.
#

SELECT * FROM performance_schema.processlist;
SELECT * FROM performance_schema.threads;
SELECT * FROM performance_schema.socket_instances;
SELECT * FROM performance_schema.global_status WHERE VARIABLE_NAME like "mysqlx%";
SELECT * FROM performance_schema.socket_summary_by_instance;
SHOW PROCESSLIST;
SELECT * FROM information_schema.plugins;
