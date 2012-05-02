# must return 0, 6
SELECT SUM(ISNULL(ATTR_VALUE)), COUNT(*)
FROM performance_schema.session_connect_attrs
WHERE ATTR_NAME IN ('_os', '_client_name', '_pid',
'_client_version', '_platform', 'program_name')
AND PROCESSLIST_ID = CONNECTION_ID();
SUM(ISNULL(ATTR_VALUE))	COUNT(*)
0	6
# must return 1
SELECT COUNT(DISTINCT PROCESSLIST_ID)
FROM performance_schema.session_connect_attrs;
COUNT(DISTINCT PROCESSLIST_ID)
1
# must return 0, 6
SELECT SUM(ISNULL(ATTR_VALUE)), COUNT(*)
FROM performance_schema.session_account_connect_attrs
WHERE ATTR_NAME IN ('_os', '_client_name', '_pid',
'_client_version', '_platform', 'program_name')
AND PROCESSLIST_ID = CONNECTION_ID();
SUM(ISNULL(ATTR_VALUE))	COUNT(*)
0	6
# must return 1
SELECT COUNT(DISTINCT PROCESSLIST_ID)
FROM performance_schema.session_account_connect_attrs;
COUNT(DISTINCT PROCESSLIST_ID)
1
CREATE USER wl5924@localhost;
# must return 1
SELECT COUNT(DISTINCT PROCESSLIST_ID)
FROM performance_schema.session_account_connect_attrs;
COUNT(DISTINCT PROCESSLIST_ID)
1
# must return 2
SELECT COUNT(DISTINCT PROCESSLIST_ID)
FROM performance_schema.session_connect_attrs;
COUNT(DISTINCT PROCESSLIST_ID)
2
# must return 1
SELECT COUNT(DISTINCT PROCESSLIST_ID)
FROM performance_schema.session_account_connect_attrs;
COUNT(DISTINCT PROCESSLIST_ID)
1
SELECT COUNT(DISTINCT PROCESSLIST_ID)
FROM performance_schema.session_connect_attrs;
ERROR 42000: SELECT command denied to user 'wl5924'@'localhost' for table 'session_connect_attrs'
DROP USER wl5924@localhost;
