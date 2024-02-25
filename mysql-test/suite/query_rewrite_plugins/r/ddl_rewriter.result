#
# Test for the DDL rewrite plugin.
#
#
# Create a table with a DATA DIRECTORY clause, to be dumped and
# restored when the plugin is installed.
#
CREATE TABLE t(i int) DATA DIRECTORY = 'MYSQLTEST_VARDIR/tmp';;
SHOW CREATE TABLE t;
Table	Create Table
t	CREATE TABLE `t` (
  `i` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQLTEST_VARDIR/tmp/'
INSERT INTO t VALUES (1);
#
# Install plugin.
#
INSTALL PLUGIN ddl_rewriter SONAME 'ddl_rewriter.xxx';
SELECT PLUGIN_NAME, PLUGIN_TYPE, PLUGIN_VERSION, PLUGIN_TYPE_VERSION
FROM information_schema.plugins
WHERE plugin_name LIKE 'ddl_rewriter';
PLUGIN_NAME	PLUGIN_TYPE	PLUGIN_VERSION	PLUGIN_TYPE_VERSION
ddl_rewriter	AUDIT	1.0	4.1
#
# Restore dump file.
#
DROP TABLE t;
SHOW CREATE TABLE t;
Table	Create Table
t	CREATE TABLE `t` (
  `i` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
include/assert.inc [Expected table contents]
DROP TABLE t;
#
# False positives.
#
CREATE TABLE t(i int) /* data directory = 'x' */ ;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) /* data directory = 'x' */' rewritten to 'CREATE TABLE t(i int) /* */' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int, `j encryption='n'j` int);
Warnings:
Note	1105	Query 'CREATE TABLE t(i int, `j encryption='n'j` int)' rewritten to 'CREATE TABLE t(i int, `j j` int)' by a query rewrite plugin
DROP TABLE t;
#
# Prefix comments make the statement not be rewritten.
#
/* */ CREATE TABLE t(i int) /* data directory = 'x' */ ;
DROP TABLE t;
/* */
CREATE TABLE t(i int) /* data directory = 'x' */ ;
DROP TABLE t;
#
# Double quotes.
#
CREATE TABLE t(i int) DATA DIRECTORY = "/tmp";
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) DATA DIRECTORY = "/tmp"' rewritten to 'CREATE TABLE t(i int) ' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENCRYPTION = "n";
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION = "n"' rewritten to 'CREATE TABLE t(i int) ' by a query rewrite plugin
DROP TABLE t;
#
# Multi-statement queries are separated before pre-parse,
# so a second non-CREATE TABLE statement will be treated
# separately.
#
CREATE SCHEMA s;
CREATE TABLE s.t(i int) DATA DIRECTORY = '/tmp';
Warnings:
Note	1105	Query 'CREATE TABLE s.t(i int) DATA DIRECTORY = '/tmp'' rewritten to 'CREATE TABLE s.t(i int) ' by a query rewrite plugin
DROP SCHEMA s;
CREATE TABLE t(i int);
CREATE SCHEMA s /* DATA DIRECTORY = '/tmp' */;
DROP SCHEMA s;
DROP TABLE t;
#
# Statements with prefix != 'CREATE TABLE' are not rewritten.
#
CREATE SCHEMA s /* data directory = 'x' */ ;
DROP SCHEMA s;
CREATE SCHEMA `s encryption='n' s`;
DROP SCHEMA `s encryption='n' s`;
#
# Case sensitivity.
#
CREATE TABLE t(i int) DATA DIRECTORY = '/tmp', ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) DATA DIRECTORY = '/tmp', ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
create table t(i int) DATA DIRECTORY = '/tmp', ENGINE = MyISAM;
Warnings:
Note	1105	Query 'create table t(i int) DATA DIRECTORY = '/tmp', ENGINE = MyISAM' rewritten to 'create table t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) data directory = '/tmp', ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) data directory = '/tmp', ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) INDEX DIRECTORY = '/tmp', ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) INDEX DIRECTORY = '/tmp', ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) index directory = '/tmp', ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) index directory = '/tmp', ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENCRYPTION = 'N', ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION = 'N', ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) encryption = 'n', ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) encryption = 'n', ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENCRYPTION = 'Y', ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION = 'Y', ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENCRYPTION = 'y', ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION = 'y', ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
#
# Optional preceding comma with optional surrounding whitespace.
#
CREATE TABLE t(i int) ENGINE = MyISAM ENCRYPTION = 'N';
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENGINE = MyISAM ENCRYPTION = 'N'' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM ' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENGINE = MyISAM,ENCRYPTION = 'N';
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENGINE = MyISAM,ENCRYPTION = 'N'' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM ' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENGINE = MyISAM, ENCRYPTION = 'N';
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENGINE = MyISAM, ENCRYPTION = 'N'' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM ' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENGINE = MyISAM ,ENCRYPTION = 'N';
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENGINE = MyISAM ,ENCRYPTION = 'N'' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM ' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENGINE = MyISAM , ENCRYPTION = 'N';
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENGINE = MyISAM , ENCRYPTION = 'N'' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM ' by a query rewrite plugin
DROP TABLE t;
#
# Optional succeeding comma with optional surrounding whitespace.
#
CREATE TABLE t(i int) ENCRYPTION = 'N' ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION = 'N' ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENCRYPTION = 'N',ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION = 'N',ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENCRYPTION = 'N', ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION = 'N', ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENCRYPTION = 'N' ,ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION = 'N' ,ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENCRYPTION = 'N' , ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION = 'N' , ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
#
# Optional assignment operator with optional surrounding whitespace.
#
CREATE TABLE t(i int) ENCRYPTION 'N' ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION 'N' ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENCRYPTION='N' ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION='N' ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENCRYPTION= 'N' ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION= 'N' ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENCRYPTION ='N' ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION ='N' ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
CREATE TABLE t(i int) ENCRYPTION = 'N' ENGINE = MyISAM;
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION = 'N' ENGINE = MyISAM' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM' by a query rewrite plugin
DROP TABLE t;
#
# Multi line statements.
#
CREATE TABLE t(i int) ENCRYPTION ='N'
                      ,ENGINE = MyISAM
DATA
DIRECTORY = '/tmp'
                      DEFAULT CHARSET latin1
INDEX
DIRECTORY '/t
                      mp';
Warnings:
Note	1105	Query 'CREATE TABLE t(i int) ENCRYPTION ='N'
                      ,ENGINE = MyISAM
DATA
DIRECTORY = '/tmp'
                      DEFAULT CHARSET latin1
INDEX
DIRECTORY '/t
                      mp'' rewritten to 'CREATE TABLE t(i int) ENGINE = MyISAM DEFAULT CHARSET latin1 ' by a query rewrite plugin
DROP TABLE t;
#
# Partitioned tables.
#
CREATE TABLE t (
i  int NOT NULL AUTO_INCREMENT,
mt mediumtext NOT NULL,
c  varchar(10) NOT NULL,
ti tinyint(1) unsigned NOT NULL,
PRIMARY KEY (i, ti)
)
ENGINE = InnoDB AUTO_INCREMENT = 0
DEFAULT CHARSET = latin1, DATA DIRECTORY = '/tmp'
  PARTITION BY LIST (ti) (
PARTITION p0 VALUES IN (0) ENGINE = InnoDB,
PARTITION p1 VALUES IN (1) DATA DIRECTORY = '/tmp' ,ENGINE = InnoDB
);
Warnings:
Note	1105	Query 'CREATE TABLE t (
i  int NOT NULL AUTO_INCREMENT,
mt mediumtext NOT NULL,
c  varchar(10) NOT NULL,
ti tinyint(1) unsigned NOT NULL,
PRIMARY KEY (i, ti)
)
ENGINE = InnoDB AUTO_INCREMENT = 0
DEFAULT CHARSET = latin1, DATA DIRECTORY = '/tmp'
  PARTITION BY LIST (ti) (
PARTITION p0 VALUES IN (0) ENGINE = InnoDB,
PARTITION p1 VALUES IN (1) DATA DIRECTORY = '/tmp' ,ENGINE = InnoDB
)' rewritten to 'CREATE TABLE t (
i  int NOT NULL AUTO_INCREMENT,
mt mediumtext NOT NULL,
c  varchar(10) NOT NULL,
ti tinyint(1
Warning	1681	Integer display width is deprecated and will be removed in a future release.
SHOW CREATE TABLE t;
Table	Create Table
t	CREATE TABLE `t` (
  `i` int NOT NULL AUTO_INCREMENT,
  `mt` mediumtext NOT NULL,
  `c` varchar(10) NOT NULL,
  `ti` tinyint unsigned NOT NULL,
  PRIMARY KEY (`i`,`ti`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
/*!50100 PARTITION BY LIST (`ti`)
(PARTITION p0 VALUES IN (0) ENGINE = InnoDB,
 PARTITION p1 VALUES IN (1) ENGINE = InnoDB) */
DROP TABLE t;
#
# Stored programs.
#
CREATE PROCEDURE p() CREATE TABLE test.t(i int) ENCRYPTION = 'N';
CALL p();
SHOW CREATE TABLE test.t;
Table	Create Table
t	CREATE TABLE `t` (
  `i` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE test.t;
DROP PROCEDURE p;
#
# Prepared statements.
#
PREPARE stmt FROM 'CREATE TABLE test.t(i int) ENCRYPTION = "N"';
Warnings:
Note	1105	Query 'CREATE TABLE test.t(i int) ENCRYPTION = "N"' rewritten to 'CREATE TABLE test.t(i int) ' by a query rewrite plugin
EXECUTE stmt;
SHOW CREATE TABLE test.t;
Table	Create Table
t	CREATE TABLE `t` (
  `i` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE test.t;
DROP PREPARE stmt;
#
# Uninstall plugin and verify that queries are not rewritten anymore.
#
UNINSTALL PLUGIN ddl_rewriter;
Warnings:
Warning	1620	Plugin is busy and will be uninstalled on shutdown
CREATE TABLE t(i int) /* data directory = 'x' */ ;
DROP TABLE t;
