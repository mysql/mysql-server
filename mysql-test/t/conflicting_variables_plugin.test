--source include/have_conflicting_variables_plugin.inc

CALL mtr.add_suppression('\\[ERROR] \\[MY-\\d+] \\[Server] Duplicate variable name ''sql_mode''\.');
CALL mtr.add_suppression('\\[ERROR] \\[MY-\\d+] \\[Server] Plugin ''sql'' has conflicting system variables');

--replace_regex /\.dll/.so/
eval INSTALL PLUGIN `sql` SONAME '$CONFLICTING_VARIABLES';

SHOW VARIABLES LIKE 'sql_mode%';

UNINSTALL PLUGIN `sql`;

--error ER_UNKNOWN_SYSTEM_VARIABLE
SELECT @@sql_mode2;
