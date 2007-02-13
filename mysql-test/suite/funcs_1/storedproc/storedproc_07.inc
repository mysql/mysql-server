#### suite/funcs_1/storedproc/storedproc_07.inc
#
--source suite/funcs_1/storedproc/load_sp_tb.inc

# ==============================================================================
# (numbering from requirement document TP v1.0, Last updated: 25 Jan 2005 01:00)
#
# 3.1.7 SQL mode checks:
#
#  1. Ensure that the sql_mode setting in effect at the time a stored procedure is created is the same setting under which the stored procedure runs when it is called/executed.
#  2. Ensure that if the sql_mode setting is changed when a stored procedure is run, that the original setting is restored as soon as the stored procedure execution is complete.
#
# ==============================================================================
let $message= Section 3.1.7 - SQL mode checks:;
--source include/show_msg80.inc

USE db_storedproc;

# ------------------------------------------------------------------------------
let $message= Testcase 3.1.7.1:
              -----------------
Ensure that the sql_mode setting in effect at the time a stored procedure is
created is the same setting under which the stored procedure runs when it is
called/executed.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
DROP TABLE IF EXISTS temp_tbl;
DROP TABLE IF EXISTS result;
--enable_warnings

CREATE TABLE temp_tbl (f1 tinyint);
CREATE TABLE result (f1 text(200), f2 char(20));

set @@sql_mode='traditional';
SHOW VARIABLES LIKE 'sql_mode';

delimiter //;
CREATE PROCEDURE sp1()
BEGIN
    declare a tinyint;
    declare count_ int default 1;
    declare continue handler for sqlstate '22003' set count_=1000;
    SHOW VARIABLES LIKE 'sql_mode';
    SELECT @@sql_mode into @cur_val_sql_mode;
    insert into temp_tbl values (1000);
    if count_ = 1000 THEN
       INSERT INTO result VALUES (@cur_val_sql_mode, 'value restored');
    ELSE
       INSERT INTO result VALUES (@cur_val_sql_mode, 'value not restored');
    END if;
END//
delimiter ;//

SHOW CREATE PROCEDURE sp1;

set @@sql_mode='';
SHOW VARIABLES LIKE 'sql_mode';

CALL sp1();
SELECT * from result;

SHOW VARIABLES LIKE 'sql_mode';
SELECT @@sql_mode;

# cleanup
SET @@sql_mode='TRADITIONAL';
DROP PROCEDURE sp1;
DROP TABLE temp_tbl;
DROP TABLE result;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.7.2:
              -----------------
Ensure that if the sql_mode setting is changed when a stored procedure is run,
that the original setting is restored as soon as the stored procedure execution
is complete.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp2;
--enable_warnings

--echo ... show initial value
SHOW VARIABLES LIKE 'sql_mode';

delimiter //;
CREATE PROCEDURE sp2()
BEGIN
   SET @@sql_mode='MAXDB';
   SHOW VARIABLES LIKE 'sql_mode';
END//
delimiter ;//

SHOW CREATE PROCEDURE sp2;
--echo ... show value prior calling procedure
SHOW VARIABLES LIKE 'sql_mode';

--echo ... call procedure that changes sql_mode
CALL sp2();

--echo ... check whether old value is re-set
SHOW VARIABLES LIKE 'sql_mode';

# cleanup
DROP PROCEDURE sp2;


# ==============================================================================
# USE the same .inc to cleanup before and after the test
--source suite/funcs_1/storedproc/cleanup_sp_tb.inc

# ==============================================================================
let $message= .                               +++ END OF SCRIPT +++;
--source include/show_msg80.inc
# ==============================================================================
