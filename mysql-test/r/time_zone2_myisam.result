#
# Bug #21840241 "IMPROPER SE REFERENCE COUNTING IN ATTACHABLE TRANSACTIONS"
#
CREATE TABLE t1 (i INT PRIMARY KEY) ENGINE=MyISAM;
CREATE TABLE t2 (i INT PRIMARY KEY) ENGINE=MyISAM;
# Decrease Table Cache size and fill it with MyISAM tables.
SET @save_table_open_cache= @@global.table_open_cache;
SET @@GLOBAL.table_open_cache=32;
SELECT * FROM t1;
i
SELECT * FROM t2;
i
# Usage of non-existent time zone will start attachable transaction
# which will access time zone tables. This will expel MyISAM tables
# from the Table Cache and creation of MyISAM specific connection
# local data. Without proper cleanup at the end of attachable
# transaction this data was reported as leaked memory by valgrind.
SELECT CONVERT_TZ('2015-01-01 00:00:00', 'UTC', 'No-such-time-zone');
CONVERT_TZ('2015-01-01 00:00:00', 'UTC', 'No-such-time-zone')
NULL
SET @@global.table_open_cache= @save_table_open_cache;
DROP TABLES t1, t2;
