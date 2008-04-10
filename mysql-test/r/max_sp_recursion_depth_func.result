SET @session_max_recursion_depth = @@SESSION.max_sp_recursion_depth;
'#--------------------FN_DYNVARS_099_01-------------------------#'
** Connecting test_con1 using username 'root' **
** Connection test_con1 **
SET @@session.max_sp_recursion_depth = 10;
SELECT @@session.max_sp_recursion_depth;
@@session.max_sp_recursion_depth
10
DROP PROCEDURE IF EXISTS sp_addRecords;
CREATE PROCEDURE sp_addRecords (IN var1 INT,IN var2 INT)
BEGIN
SELECT var1,var2;
IF var1 < var2 THEN
CALL sp_addRecords(var1+1,var2);
SELECT var1,var2;
END IF;
END //
CALL sp_addRecords(0,8);
var1	var2
0	8
var1	var2
1	8
var1	var2
2	8
var1	var2
3	8
var1	var2
4	8
var1	var2
5	8
var1	var2
6	8
var1	var2
7	8
var1	var2
8	8
var1	var2
7	8
var1	var2
6	8
var1	var2
5	8
var1	var2
4	8
var1	var2
3	8
var1	var2
2	8
var1	var2
1	8
var1	var2
0	8
'#--------------------FN_DYNVARS_099_02-------------------------#'
** Connecting test_con2 using username 'root' **
** Connection test_con2 **
SET @@session.max_sp_recursion_depth = 4;
SELECT @@session.max_sp_recursion_depth;
@@session.max_sp_recursion_depth
4
DROP PROCEDURE IF EXISTS sp_addRecords1;
CREATE PROCEDURE sp_addRecords1 (IN var1 INT,IN var2 INT)
BEGIN
SELECT var1,var2;
IF var1 < var2 THEN
CALL sp_addRecords1(var1+1,var2);
SELECT var1,var2;
END IF;
END //
CALL sp_addRecords1(0,4);
var1	var2
0	4
var1	var2
1	4
var1	var2
2	4
var1	var2
3	4
var1	var2
4	4
var1	var2
3	4
var1	var2
2	4
var1	var2
1	4
var1	var2
0	4
'#---------------------FN_DYNVARS_99_03----------------------#'
SET @@max_sp_recursion_depth = 10;
DROP PROCEDURE IF EXISTS sp_addRecords2;
CREATE PROCEDURE sp_addRecords2(IN var1 INT,IN var2 INT)
BEGIN
SELECT var1,var2;
IF var1 < var2 THEN
CALL sp_addRecords2(var1+1,var2);
SELECT var1,var2;
END IF;
END //
CALL sp_addRecords2(0,8);
var1	var2
0	8
var1	var2
1	8
var1	var2
2	8
var1	var2
3	8
var1	var2
4	8
var1	var2
5	8
var1	var2
6	8
var1	var2
7	8
var1	var2
8	8
var1	var2
7	8
var1	var2
6	8
var1	var2
5	8
var1	var2
4	8
var1	var2
3	8
var1	var2
2	8
var1	var2
1	8
var1	var2
0	8
'#---------------------FN_DYNVARS_99_04----------------------#'
SET @@max_sp_recursion_depth = 4;
DROP PROCEDURE IF EXISTS sp_addRecords3;
CREATE PROCEDURE sp_addRecords3 (IN var1 INT,IN var2 INT)
BEGIN
SELECT var1,var2;
IF var1 < var2 THEN
CALL sp_addRecords3(var1+1,var2);
SELECT var1,var2;
END IF;
END //
Expected error 'SP Recursion limit'
CALL sp_addRecords3(0,8);
var1	var2
0	8
var1	var2
1	8
var1	var2
2	8
var1	var2
3	8
var1	var2
4	8
ERROR HY000: Recursive limit 4 (as set by the max_sp_recursion_depth variable) was exceeded for routine sp_addRecords3
** Connection default **
** Disconnecting test_con1, test_con2 **
DROP PROCEDURE IF EXISTS sp_addRecords;
DROP PROCEDURE IF EXISTS sp_addRecords1;
DROP PROCEDURE IF EXISTS sp_addRecords2;
DROP PROCEDURE IF EXISTS sp_addRecords3;
SET @@SESSION.max_sp_recursion_depth = @session_max_recursion_depth;
