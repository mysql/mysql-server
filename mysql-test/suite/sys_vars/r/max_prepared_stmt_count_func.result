** Setup **

SET @global_max_prepared_stmt_count = @@global.max_prepared_stmt_count;
'#---------------------FN_DYNVARS_031_01----------------------#'
SET GLOBAL max_prepared_stmt_count=2;
** Prepare statements **
PREPARE stmt  from "SELECT * FROM information_schema.CHARACTER_SETS C";
PREPARE stmt1 from "SELECT * FROM information_schema.CHARACTER_SETS C";
PREPARE stmt2 from "SELECT * FROM information_schema.CHARACTER_SETS C";
ERROR 42000: Can't create more than max_prepared_stmt_count statements (current value: 2)
Expected error "Max prepared statements count reached"
SHOW STATUS like 'Prepared_stmt_count';
Variable_name	Value
Prepared_stmt_count	2
2 Expected
'#---------------------FN_DYNVARS_031_02----------------------#'
SET GLOBAL max_prepared_stmt_count=0;
PREPARE stmt3  from "SELECT * FROM information_schema.CHARACTER_SETS C";
ERROR 42000: Can't create more than max_prepared_stmt_count statements (current value: 0)
Expected error "Max prepared statements count reached"
SHOW STATUS like 'Prepared_stmt_count';
Variable_name	Value
Prepared_stmt_count	2
2 Expected
PREPARE stmt  from "SELECT * FROM information_schema.CHARACTER_SETS C";
ERROR 42000: Can't create more than max_prepared_stmt_count statements (current value: 0)
Expected error "Max prepared statements count reached"
SHOW STATUS like 'Prepared_stmt_count';
Variable_name	Value
Prepared_stmt_count	1
2 Expected
'Bug#35389 A pre existing valid prepared statement DROPS if a PREPARE'
'STATEMENT command is issued with the same name that'
'causes ER_MAX_PREPARED_STMT_COUNT_REACHED error'
'#---------------------FN_DYNVARS_031_03----------------------#'
SHOW STATUS like 'Prepared_stmt_count';
Variable_name	Value
Prepared_stmt_count	1
SET GLOBAL max_prepared_stmt_count=4;
PREPARE stmt from "SELECT * FROM information_schema.CHARACTER_SETS C";
PREPARE stmt1 from "SELECT * FROM information_schema.CHARACTER_SETS C";
PREPARE stmt2 from "SELECT * FROM information_schema.CHARACTER_SETS C";
PREPARE stmt3 from "SELECT * FROM information_schema.CHARACTER_SETS C";
** Value of prepared stmt'
SHOW STATUS LIKE 'Prepared_stmt_count';
Variable_name	Value
Prepared_stmt_count	4
4 Expected
'#---------------------FN_DYNVARS_031_04----------------------#'
** preparing stmts **
PREPARE stmt from "SELECT * FROM information_schema.CHARACTER_SETS C";
PREPARE stmt1 from "SELECT * FROM information_schema.CHARACTER_SETS C";
PREPARE stmt2 from "SELECT * FROM information_schema.CHARACTER_SETS C";
** setting value **
SET GLOBAL max_prepared_stmt_count=3;
** Check wether any more statements can be  prepared **
PREPARE stmt5 from "SELECT * FROM information_schema.CHARACTER_SETS C";
ERROR 42000: Can't create more than max_prepared_stmt_count statements (current value: 3)
Expected error "Max prepared statements count reached"
SHOW STATUS LIKE 'Prepared_stmt_count';
Variable_name	Value
Prepared_stmt_count	4
4 Expected
'#---------------------FN_DYNVARS_031_05----------------------#'
SET GLOBAL max_prepared_stmt_count=3;
** Creating procedure **
DROP PROCEDURE  IF EXISTS  sp_checkstmts;
Warnings:
Note	1305	PROCEDURE test.sp_checkstmts does not exist
CREATE PROCEDURE sp_checkstmts ()
BEGIN
PREPARE newstmt from "SELECT * FROM information_schema.CHARACTER_SETS C";
END //
CALL sp_checkstmts();
ERROR 42000: Can't create more than max_prepared_stmt_count statements (current value: 3)
Expected error "Max prepared statements count reached"
DROP PREPARE stmt;
DROP PREPARE stmt1;
DROP PREPARE stmt2;
DROP PREPARE stmt3;
DROP PROCEDURE sp_checkstmts;
SET @@global.max_prepared_stmt_count = @global_max_prepared_stmt_count;
