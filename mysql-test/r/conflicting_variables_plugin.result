CALL mtr.add_suppression('\\[ERROR] \\[MY-\\d+] \\[Server] Duplicate variable name ''sql_mode''\.');
CALL mtr.add_suppression('\\[ERROR] \\[MY-\\d+] \\[Server] Plugin ''sql'' has conflicting system variables');
INSTALL PLUGIN `sql` SONAME 'conflicting_variables.so';
Warnings:
Warning	1123	Can't initialize function 'sql'; Plugin is disabled
SHOW VARIABLES LIKE 'sql_mode%';
Variable_name	Value
sql_mode	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
UNINSTALL PLUGIN `sql`;
SELECT @@sql_mode2;
ERROR HY000: Unknown system variable 'sql_mode2'
