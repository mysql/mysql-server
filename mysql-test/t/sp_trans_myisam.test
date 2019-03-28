--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# BUG#11758414: Default storage_engine not honored when set
#               from within a stored procedure
#
delimiter |;
SELECT @@GLOBAL.default_storage_engine INTO @old_engine|
SET @@GLOBAL.default_storage_engine=InnoDB|
SET @@SESSION.default_storage_engine=InnoDB|
# show defaults at define-time
SHOW GLOBAL VARIABLES LIKE 'default_storage_engine'|
SHOW SESSION VARIABLES LIKE 'default_storage_engine'|
CREATE PROCEDURE bug11758414()
BEGIN
 SET @@GLOBAL.default_storage_engine="MyISAM";
 SET @@SESSION.default_storage_engine="MyISAM";
 # show defaults at execution time / that setting them worked
 SHOW GLOBAL VARIABLES LIKE 'default_storage_engine';
 SHOW SESSION VARIABLES LIKE 'default_storage_engine';
 CREATE TABLE t1 (id int);
 CREATE TABLE t2 (id int) ENGINE=InnoDB;
 # show we're heeding the default (at run-time, not parse-time!)
 SHOW CREATE TABLE t1;
 # show that we didn't break explicit override with ENGINE=...
 SHOW CREATE TABLE t2;
END;
|
CALL bug11758414|
# show that changing defaults within SP stuck
SHOW GLOBAL VARIABLES LIKE 'default_storage_engine'|
SHOW SESSION VARIABLES LIKE 'default_storage_engine'|
DROP PROCEDURE bug11758414|
DROP TABLE t1, t2|
SET @@GLOBAL.default_storage_engine=@old_engine|

--echo # 
--echo # Bug #35877 Update .. WHERE with function, constraint violation, crash 
--echo #  

-- echo # MyISAM test
CREATE TABLE t1_not_null (f1 BIGINT, f2 BIGINT NOT NULL)|
CREATE TABLE t1_aux (f1 BIGINT, f2 BIGINT)|
INSERT INTO t1_aux VALUES (1,1)|

CREATE FUNCTION f1_two_inserts() returns INTEGER
BEGIN
   INSERT INTO t1_not_null SET f1 = 10, f2 = NULL;
   RETURN 1;
END|

-- error ER_BAD_NULL_ERROR
UPDATE t1_aux SET f2 = 2 WHERE f1 = f1_two_inserts()|

-- echo # InnoDB test
ALTER TABLE t1_not_null ENGINE = InnoDB|
ALTER TABLE t1_aux ENGINE = InnoDB|

-- error ER_BAD_NULL_ERROR
UPDATE t1_aux SET f2 = 2 WHERE f1 = f1_two_inserts()|

DROP TABLE t1_aux, t1_not_null|
DROP FUNCTION f1_two_inserts|

--echo #
--echo # Bug#49938: Failing assertion: inode or deadlock in fsp/fsp0fsp.c
--echo #

--disable_warnings
DROP PROCEDURE IF EXISTS p1|
DROP TABLE IF EXISTS t1|
--enable_warnings

CREATE TABLE t1 (a INT) ENGINE=INNODB|

CREATE PROCEDURE p1()
BEGIN
  TRUNCATE TABLE t1;
END|

LOCK TABLES t1 WRITE|
CALL p1()|
FLUSH TABLES;
UNLOCK TABLES|
CALL p1()|

DROP PROCEDURE p1|
DROP TABLE t1|

delimiter ;|
