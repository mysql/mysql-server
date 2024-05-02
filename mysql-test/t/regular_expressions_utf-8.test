--echo #
--echo # Tests of regular expression functions.
--echo #

--source ../mysql-test/include/icu_utils.inc

CREATE TABLE t1 (
  subject char(10),
  pattern char(10)
);

--echo #
--echo # REGEXP_INSTR
--echo #

--echo # REGEXP_INSTR(), two arguments.
SELECT regexp_instr( 'abc', 'a' );
SELECT regexp_instr( 'abc', 'b' );
SELECT regexp_instr( 'abc', 'c' );
SELECT regexp_instr( 'abc', 'd' );
SELECT regexp_instr( NULL, 'a' );
SELECT regexp_instr( 'a', NULL );
SELECT regexp_instr( NULL, NULL );
--echo # This tests that the correct character set is declared. If we don't
--echo # declare it correctly, the UTF-16 sequence will be interpreted as a
--echo # two-byte string consisting of the ASCII for '1' and NUL, and the
--echo # result will be 3100.
SET NAMES binary;
SELECT hex( concat(regexp_instr( 'a', 'a' )) );
SET NAMES DEFAULT;

--echo # REGEXP_INSTR(), illegal types.
SELECT regexp_instr( 1, 'a' );
SELECT regexp_instr( 1.1, 'a' );
SELECT regexp_instr( 'a', 1 );
SELECT regexp_instr( 'a', 1.1 );

SELECT regexp_instr( subject, pattern ) FROM t1;

--error ER_REGEXP_ILLEGAL_ARGUMENT
SELECT regexp_instr( 'a', '[[:invalid_bracket_expression:]]' );

--echo # REGEXP_INSTR(), three arguments.
--echo # subject, pattern, position.
SELECT regexp_instr( 'abcabcabc', 'a+', 1 );
SELECT regexp_instr( 'abcabcabc', 'a+', 2 );
SELECT regexp_instr( 'abcabcabc', 'b+', 1 );
SELECT regexp_instr( 'abcabcabc', 'b+', 2 );
SELECT regexp_instr( 'abcabcabc', 'b+', 3 );

--error ER_REGEXP_INDEX_OUTOFBOUNDS_ERROR
SELECT regexp_instr( 'a', 'a+', 3 );

--echo # REGEXP_INSTR(), four arguments.
--echo # subject, pattern, position, occurence.
SELECT regexp_instr( 'abcabcabc', 'a+', 1, 2 );
SELECT regexp_instr( 'abcabcabc', 'a+', 1, 3 );
SELECT regexp_instr( 'abcabcabc', 'a+', 1, 4 );
SELECT regexp_instr( 'abcabcabc', 'a+', 4, 2 );

--echo # REGEXP_INSTR(), five arguments.
--echo # subject, pattern, position, occurence, return_option
--error ER_WRONG_ARGUMENTS
SELECT regexp_instr( 'a', 'a+', 1, 1, 2 );
--error ER_WRONG_ARGUMENTS
SELECT regexp_instr( 'a', 'a+', 1, 1, -1 );
SELECT regexp_instr( 'a', 'a+', 1, 1, NULL );
SELECT regexp_instr( 'abcabcabc', 'a+', 1, 1, 0 );
SELECT regexp_instr( 'abcabcabc', 'a+', 1, 1, 1 );
SELECT regexp_instr( 'aaabcabcabc', 'a+', 1, 1, 1 );

--echo # REGEXP_INSTR(), six arguments.
--echo # subject, pattern, position, occurence, return_option, match_parameter
SELECT regexp_instr( 'aaabcabcabc', 'A+', 1, 1, 1, 'c' );
SELECT regexp_instr( 'aaabcabcabc', 'A+', 1, 1, 1, 'i' );
SELECT regexp_instr( 'aaabcabcabc', 'A+', 1, 1, 1, 'ci' );
SELECT regexp_instr( 'aaabcabcabc', 'A+', 1, 1, 1, 'cic' );

--error ER_WRONG_ARGUMENTS
SELECT regexp_instr( 'a', 'a+', 1, 1, 1, 'x' );
SELECT regexp_instr( 'a', 'a+', 1, 1, 1, NULL );


--echo #
--echo # REGEXP_LIKE
--echo #

--echo # REGEXP_LIKE(), two arguments.
SELECT regexp_like( 'abc', 'a' );
SELECT regexp_like( 'abc', 'b' );
SELECT regexp_like( 'abc', 'c' );
SELECT regexp_like( 'abc', 'd' );
SELECT regexp_like( 'a', 'a.*' );
SELECT regexp_like( 'ab', 'a.*' );
SELECT regexp_like( NULL, 'a' );
SELECT regexp_like( 'a', NULL );
SELECT regexp_like( NULL, NULL );
--echo # This tests that the correct character set is declared. If we don't
--echo # declare it correctly, the UTF-16 sequence will be interpreted as a
--echo # two-byte string consisting of the ASCII for '1' and NUL, and the
--echo # result will be 3100.
SET NAMES binary;
SELECT hex( concat(regexp_like( 'a', 'a' )) );
SET NAMES DEFAULT;

--echo # REGEXP_LIKE(), three arguments.
SELECT regexp_like( 'abc', 'A', 'i' );
SELECT regexp_like( 'abc', 'A', 'c' );

--error ER_WRONG_ARGUMENTS
SELECT regexp_like( 'a', 'a+', 'x' );
--error ER_WRONG_ARGUMENTS
SELECT regexp_like( 'a', 'a+', 'cmnux' );
SELECT regexp_like( 'a', 'a+', NULL );

--echo # REGEXP_LIKE(), illegal types.
SELECT regexp_like( 1, 'a' );
SELECT regexp_like( 1.1, 'a' );
SELECT regexp_like( 'a', 1 );
SELECT regexp_like( 'a', 1.1 );

--error ER_REGEXP_ILLEGAL_ARGUMENT
SELECT regexp_like('a', '[[:invalid_bracket_expression:]]');


--echo #
--echo # REGEXP_REPLACE
--echo #

--echo # REGEXP_REPLACE(), three arguments.
SELECT regexp_replace( 'aaa', 'a', 'X' );
SELECT regexp_replace( 'abc', 'b', 'X' );
SELECT regexp_replace( NULL, 'a', 'X' );
SELECT regexp_replace( 'aaa', NULL, 'X' );
SELECT regexp_replace( 'aaa', 'a', NULL );
--echo # This tests that the correct character set is declared. If we don't
--echo # declare it correctly, the UTF-16 sequence will be interpreted as a
--echo # zero-terminated string consisting of 'X', and the
--echo # result will thus be 'X'.
SELECT concat( regexp_replace( 'aaa', 'a', 'X' ), 'x' );

--echo # REGEXP_REPLACE(), four arguments.
--error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
SELECT regexp_replace( 'aaa', 'a', 'X', 0 );
SELECT regexp_replace( 'aaa', 'a', 'X', 1 );

--error ER_REGEXP_INDEX_OUTOFBOUNDS_ERROR
SELECT regexp_replace( 'a', 'a+', 'b', 3 );

--echo # REGEXP_REPLACE(), five arguments.
--error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
SELECT regexp_replace( 'aaabbccbbddaa', 'b+', 'X', 0, 1 );
SELECT regexp_replace( 'aaabbccbbddaa', 'b+', 'X', 1, 1 );
SELECT regexp_replace( 'aaabbccbbddaa', 'b+', 'X', 1, 2 );
SELECT regexp_replace( 'aaabbccbbddaa', '(b+)', '<$1>', 1, 2 );
SELECT regexp_replace( 'aaabbccbbddaa', 'x+', 'x', 1, 0 );
SELECT regexp_replace( 'aaabbccbbddaa', 'b+', 'x', 1, 0 );
SELECT regexp_replace( 'aaab', 'b', 'x', 1, 2 );
SELECT regexp_replace( 'aaabccc', 'b', 'x', 1, 2 );

SELECT regexp_replace( 'abc', 'b', 'X' );
SELECT regexp_replace( 'abcbdb', 'b', 'X' );

SELECT regexp_replace( 'abcbdb', 'b', 'X', 3 );
SELECT regexp_replace( 'aaabcbdb', 'b', 'X', 1 );
SELECT regexp_replace( 'aaabcbdb', 'b', 'X', 2 );
SELECT regexp_replace( 'aaabcbdb', 'b', 'X', 3 );

--error ER_REGEXP_ILLEGAL_ARGUMENT
SELECT regexp_replace( 'a', '[[:invalid_bracket_expression:]]', '$1' );

--echo #
--echo # Test of the dynamic buffer in REGEXP_REPLACE.
--echo #
SELECT regexp_replace( 'aaa', 'a', 'X', 2 );
SELECT regexp_replace( 'aaa', 'a', 'XX', 2 );


--echo #
--echo # REGEXP_SUBSTR
--echo #

--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT regexp_substr( 'a' );

--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT regexp_substr( 'a', 'b', 'c', 'd', 'e', 'f' );

--echo # REGEXP_SUBSTR(), two arguments.
SELECT regexp_substr( 'ab ac ad', '.d' );
SELECT regexp_substr( 'ab ac ad', '.D' );
--echo # This tests that the correct character set is declared. If we don't
--echo # declare it correctly, the UTF-16 sequence will be interpreted as a
--echo # zero-terminated string consisting of 'a', and the
--echo # result will thus be 'a'.
SELECT concat( regexp_substr( 'aaa', 'a+' ), 'x' );

--echo # REGEXP_SUBSTR(), three arguments.
--error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
SELECT regexp_substr( 'ab ac ad', 'a.', 0 );
SELECT regexp_substr( 'ab ac ad', 'A.', 3 );
SELECT regexp_substr( 'ab ac ad', 'A.', 3, 1 );
SELECT regexp_substr( 'ab ac ad', 'A.', 3, 2 );
SELECT regexp_substr( 'ab ac ad', 'A.', 3, 3 );
SELECT regexp_substr( 'ab ac ad', 'A.', 3, 3 ) IS NULL;

--echo # REGEXP_SUBSTR(), four arguments.
SELECT regexp_substr( 'ab ac ad', 'A.', 1, 1, 'c' );
SELECT regexp_substr( 'ab\nac\nad', 'A.', 1, 1, 'i' );
SELECT regexp_substr( 'ab\nac\nad', 'A.', 1, 1, 'im' );

--error ER_REGEXP_ILLEGAL_ARGUMENT
SELECT regexp_substr('a', '[[:invalid_bracket_expression:]]');

SET sql_mode = ''; # Un-strict
CREATE TABLE t2 ( g GEOMETRY NOT NULL );
INSERT INTO t2 VALUES ( POINT(1,2) );
SELECT concat( regexp_like(g, g), 'x' ) FROM t2;
SET sql_mode = DEFAULT;
DROP TABLE t2;

--echo #

DROP TABLE t1;

--echo #
--echo # Error handling.
--echo #

--echo # Error handling: Stack limit.
--echo # The following queries are from bug#24449090

--error ER_REGEXP_BAD_INTERVAL
SELECT regexp_instr( '', '((((((((){80}){}){11}){11}){11}){80}){11}){4}' );

--echo # Query and result logs turned off from here ...
--disable_query_log
--disable_result_log

if (`SELECT icu_major_version() > 50`)
{

SET GLOBAL regexp_stack_limit = 239;

--error ER_REGEXP_STACK_OVERFLOW
SELECT regexp_instr( '', '(((((((){120}){11}){11}){11}){80}){11}){4}' );

SET GLOBAL regexp_stack_limit = 32;

--error ER_REGEXP_STACK_OVERFLOW
SELECT regexp_instr( '', '((((((){11}){11}){11}){160}){11}){160}' );

--error ER_REGEXP_STACK_OVERFLOW
SELECT regexp_instr( '', '(((((){150}){11}){160}){11}){160}' );

--error ER_REGEXP_STACK_OVERFLOW
SELECT regexp_instr( '', '((((){255}){255}){255}){255}' );

SET GLOBAL regexp_stack_limit = DEFAULT;

SET GLOBAL regexp_time_limit = 1000;

--error ER_REGEXP_TIME_OUT
SELECT regexp_instr( 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAC', '(A+)+B' );

SET GLOBAL regexp_time_limit = DEFAULT;

}

--enable_result_log
--enable_query_log

--echo # ... to here. The reason is that we're testing the stack limit feature,
--echo # which is not present (or working) in ICU 5.0 or earlier. We don't need
--echo # the query or result/error log to test the feature, though. The above
--echo # tests still fail unless the proper error is thrown.


--echo #
--echo # Character set conversion.
--echo #

SET NAMES latin1;
SELECT regexp_instr( _latin1 x'61F662', _latin1 x'F6' );
SELECT regexp_instr( _latin1 x'61F662', _utf8mb4'ö' );
# SUSHI, codepoint U+1F363, in SMP
SELECT regexp_instr( concat('a', _utf8mb4 x'F09F8DA3'), _utf8mb4 x'F09F8DA3' );

SELECT regexp_instr( _utf8mb4'aöb', _utf8mb4'ö' );
SET NAMES utf8mb3;
SELECT regexp_instr( 'aöb', 'ö' );
SET NAMES DEFAULT;

#--echo #
#--echo # Case sensitivity.
#--echo #
#SELECT 'a' REGEXP 'A' COLLATE latin1_general_ci;

#CREATE TABLE t1 ( a char(6), b char(6) );
#INSERT INTO t1 VALUES ('',''), ('','First'), ('Random','Random');
#SELECT * FROM t1 WHERE CONCAT ( a, ' ', b ) REGEXP 'First.*';
#DROP TABLE t1;

#set collation_connection=utf8mb4_general_ci;
#--source include/ctype_regex.inc

--echo #
--echo # Bug#30241: Regular expression problems
--echo #
SET NAMES utf8mb3;
SELECT regexp_instr( 'אב רק', /*k*/'^[^ב]' );

PREPARE stmt1 FROM "select 'a' rlike ?";
DEALLOCATE PREPARE stmt1;

CREATE TABLE t1( a INT, subject CHAR(10) );
CREATE TABLE t2( pattern CHAR(10) );

insert into t1 values (0, 'apa');
insert into t2 values ('apa');

DELIMITER ||;

CREATE DEFINER=root@localhost PROCEDURE p1()
BEGIN
  UPDATE t1, t2
  SET a = 1
  WHERE regexp_like(t1.subject, t2.pattern);
END||

DELIMITER ;||

CALL p1();

DROP PROCEDURE p1;
DROP TABLE t1, t2;

CREATE TABLE t1 ( a INT );
EXPLAIN SELECT 1 FROM t1 WHERE 1 REGEXP (1 IN (SELECT 1 FROM t1));
DROP TABLE t1;

PREPARE stmt1 FROM "SELECT regexp_like( 'a', ? )";
PREPARE stmt2 FROM "SELECT regexp_like( ?, 'a' )";
PREPARE stmt3 FROM "SELECT regexp_like( ?, ? )";

SET @subject = 'a';
SET @pattern = 'a+';

EXECUTE stmt1 USING @pattern;
EXECUTE stmt2 USING @subject;
EXECUTE stmt3 USING @subject, @pattern;

--echo #
--echo # Yes, you can circumvent the type checking using prepared statements.
--echo #
SET @subject = 1;
SET @pattern = 1;

EXECUTE stmt1 USING @pattern;
EXECUTE stmt2 USING @subject;
EXECUTE stmt3 USING @subject, @pattern;

DEALLOCATE PREPARE stmt1;
DEALLOCATE PREPARE stmt2;
DEALLOCATE PREPARE stmt3;

--echo # Let's make sure we handle arguments that raise errors when evaluated.
--error ER_WRONG_ARGUMENTS
DO 1 rlike multilinestring(point(1, 1));

--echo #
--echo # Test of repeated use of one matcher.
--echo #

CREATE TABLE t1 ( a CHAR(10) );
INSERT INTO t1 VALUES ( 'abc' ), ( 'bcd' ), ( 'cde' );

SELECT regexp_like( a, 'a' ) FROM t1;

DROP TABLE t1;

CREATE TABLE t1 ( a CHAR ( 10 ), b CHAR ( 10 ) );

INSERT INTO t1 VALUES( NULL, 'abc' );
INSERT INTO t1 VALUES( 'def', NULL );

SELECT a, b, regexp_like( a, b ) FROM t1;

DROP TABLE t1;

--echo #
--echo # Generated columns.
--echo #
CREATE TABLE t1 (
  c CHAR(10) CHARSET latin1 COLLATE latin1_bin,
  c_ci CHAR(10) CHARSET latin1 COLLATE latin1_general_ci,
  c_cs CHAR(10) CHARSET latin1 COLLATE latin1_general_cs
);

INSERT INTO t1
VALUES ( 'a', 'a', 'a' ), ( 'A', 'A', 'A' ), ( 'b', 'b', 'b' );

--sorted_result
SELECT c, c_ci REGEXP 'A', c_cs REGEXP 'A' FROM t1;

DROP TABLE t1;

SELECT regexp_like( _utf8mb4 'ss' COLLATE utf8mb4_german2_ci,
                    _utf8mb4 'ß'  COLLATE utf8mb4_german2_ci );

SELECT regexp_like( _utf8mb4 'ß'  COLLATE utf8mb4_german2_ci,
                    _utf8mb4 'ss' );

SELECT regexp_like( _utf8mb4 'ß'  COLLATE utf8mb4_de_pb_0900_as_cs,
                    _utf8mb4 'ss' );

--echo #
--echo # Regression testing.
--echo #
SET NAMES latin1;
SELECT regexp_like( 'a', 'A' COLLATE latin1_general_ci );
SELECT 'a' REGEXP 'A' COLLATE latin1_general_ci;
SELECT regexp_like( 'a', 'A' COLLATE latin1_general_cs );
SELECT 'a' REGEXP 'A' COLLATE latin1_general_cs;

--echo # The default behaviour, multiline match is off:
SELECT regexp_like( 'a\nb\nc', '^b$' );
--echo # Enabling the multiline option using the PCRE option syntax:
--echo # (Previously not accepted)
SELECT regexp_like( 'a\nb\nc', '(?m)^b$' );

--echo # Dot matches newline:
SELECT regexp_like( 'a\nb\nc', '.*' );

SELECT regexp_like( _utf16 'a' , 'a' );
SELECT regexp_like( _utf16le 'a' , 'a' );

--echo # Make sure we exit on error.
--error ER_WRONG_ARGUMENTS
SELECT regexp_like( 'aaa', 'a+', 1 );
--error ER_WRONG_ARGUMENTS
SELECT regexp_substr( 'aaa', 'a+', 1, 1, 1 );
--error ER_WRONG_ARGUMENTS
SELECT regexp_substr( 'aaa', 'a+', 1, 1, 1 );
--error ER_REGEXP_RULE_SYNTAX
SELECT regexp_substr( 'aaa', '+' );

--echo #
--echo # Test of a table where the columns are already in the lib's charset and
--echo # contain NULL.
--echo #
CREATE TABLE t1 (
  a CHAR(3) CHARACTER SET utf16le,
  b CHAR(3) CHARACTER SET utf16le
);

INSERT INTO t1 VALUES ( NULL, 'abc' );
INSERT INTO t1 VALUES ( 'def', NULL );
INSERT INTO t1 VALUES ( NULL, NULL );

SELECT a regexp b FROM t1;

DROP TABLE t1;

--echo #
--echo # Typecasts.
--echo #
CREATE TABLE t1
(
  a REAL,
  b INT,
  c CHAR(100),
  d DECIMAL
);

INSERT INTO t1 VALUES ( regexp_instr('a', 'a'),
                        regexp_instr('a', 'a'),
                        regexp_instr('a', 'a'),
                        regexp_instr('a', 'a') );
SELECT * FROM t1;
DELETE FROM t1;

INSERT INTO t1 VALUES ( regexp_like('a', 'a'),
                        regexp_like('a', 'a'),
                        regexp_like('a', 'a'),
                        regexp_like('a', 'a') );
SELECT * FROM t1;
DELETE FROM t1;

--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO t1 ( a ) VALUES ( regexp_replace('a', 'a', 'a') );
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO t1 ( b ) VALUES ( regexp_replace('a', 'a', 'a') );
INSERT INTO t1 ( c ) VALUES ( regexp_replace('a', 'a', 'a') );
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO t1 ( d ) VALUES ( regexp_replace('a', 'a', 'a') );

SELECT * FROM t1;
DELETE FROM t1;

--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO t1 ( a ) VALUES ( regexp_substr('a', 'a') );
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO t1 ( b ) VALUES ( regexp_substr('a', 'a') );
INSERT INTO t1 ( c ) VALUES ( regexp_substr('a', 'a') );
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO t1 ( d ) VALUES ( regexp_substr('a', 'a') );
SELECT * FROM t1;

DROP TABLE t1;

--echo # At the time of writing, val_int() is not called when inserting into an
--echo # INT column.
SELECT cast( regexp_replace('a', 'a', 'a') AS SIGNED INTEGER );
SELECT cast( regexp_substr ('a', 'a')      AS SIGNED INTEGER );

--echo # Casting to DATETIME/TIME
--echo # Due to the class hierarchy of function objects, these have to be
--echo # copy-pasted.
SELECT cast( regexp_instr  ('a', 'a'     ) AS DATETIME );
SELECT cast( regexp_like   ('a', 'a'     ) AS DATETIME );
SELECT cast( regexp_replace('a', 'a', 'a') AS DATETIME );
SELECT cast( regexp_substr ('a', 'a'     ) AS DATETIME );

SELECT cast( regexp_instr  ('a', 'a'     ) AS TIME );
SELECT cast( regexp_like   ('a', 'a'     ) AS TIME );
SELECT cast( regexp_replace('a', 'a', 'a') AS TIME );
SELECT cast( regexp_substr ('a', 'a'     ) AS TIME );

--echo #
--echo # Error messages.
--echo # Test of the error messages themselves.
--echo #


SET GLOBAL net_buffer_length = 1024;
SET GLOBAL max_allowed_packet = @@global.net_buffer_length;
SELECT @@global.max_allowed_packet;

--echo # We need to start a new session in order for the changes to the session
--echo # version of max_allowed_packet to take effect.

--connect(conn1,localhost,root,,)
--echo # This is now the replacement buffer size in UTF-16 characters.
SET @buf_sz_utf16 = @@global.max_allowed_packet / length( _utf16'x' );
SELECT @buf_sz_utf16;
SELECT length(regexp_replace( repeat('a', @buf_sz_utf16), 'a', 'b' ));
--error ER_REGEXP_BUFFER_OVERFLOW
SELECT length(regexp_replace( repeat('a', @buf_sz_utf16 + 1), 'a', 'b' ));
--error ER_REGEXP_BUFFER_OVERFLOW
SELECT length(regexp_replace( repeat('a', @buf_sz_utf16), 'a', 'bb' ));
--disconnect conn1
--connection default

SET GLOBAL net_buffer_length = DEFAULT;
SET GLOBAL max_allowed_packet = DEFAULT;


--error ER_REGEXP_ILLEGAL_ARGUMENT
SELECT regexp_like( 'a', '[[:<:]]a' );

--error ER_REGEXP_RULE_SYNTAX
SELECT regexp_like( 'a', '   **' );
--error ER_REGEXP_RULE_SYNTAX
SELECT regexp_like( 'a', ' \n  **' );
--error ER_REGEXP_RULE_SYNTAX
SELECT regexp_like( 'a', '  +++' );

--error ER_REGEXP_BAD_ESCAPE_SEQUENCE
SELECT regexp_like( 'a', '\\' );

--error ER_REGEXP_UNIMPLEMENTED
SELECT regexp_like('a','(?{})');

--error ER_REGEXP_MISMATCHED_PAREN
SELECT regexp_like('a','(');

--error ER_REGEXP_BAD_INTERVAL
SELECT regexp_like('a','a{}');

--error ER_REGEXP_MAX_LT_MIN
SELECT regexp_like('a','a{2,1}');

--error ER_REGEXP_INVALID_BACK_REF
SELECT regexp_like('a','\\1');

--error ER_REGEXP_LOOK_BEHIND_LIMIT
SELECT regexp_substr( 'ab','(?<=a+)b' );

--error ER_REGEXP_MISSING_CLOSE_BRACKET
SELECT regexp_like( 'a', 'a[' );

--error ER_REGEXP_INVALID_RANGE
SELECT regexp_substr( 'ab','[b-a]' );

--echo #
--echo # Test that the replacement buffer can grow beyond the maximum VARCHAR
--echo # column length.
--echo #
CREATE TABLE t1 ( a TEXT );
# The maximum VARCHAR length, divided by max multibyte character length,
# plus one.
INSERT INTO t1 VALUES ( repeat( 'a', 16384 ) );
SELECT char_length ( regexp_replace( a, 'a', 'b' ) ) FROM t1;
SET GLOBAL  regexp_time_limit = 10000;
SELECT regexp_like ( regexp_replace( a, 'a', 'b' ), 'b{16384}' ) FROM t1;
SET GLOBAL  regexp_time_limit = DEFAULT;
DROP TABLE t1;

--echo #
--echo # Bug#27134570: DOS: REGEXP TAKES EXPONENTIALLY LONGER, CAN'T BE KILLED,
--echo # HOGS CPU
--echo #

--error ER_REGEXP_PATTERN_TOO_BIG
DO '1' regexp repeat('$', 50000000);


--echo #
--echo # Bug#27140286: REGEXP, ASSERTION FAILED: !THD->IS_ERROR() IN
--echo # SETUP_FIELDS()
--echo #

--error ER_DATA_OUT_OF_RANGE
DO ( (@b) regexp (cot (unhex ( 1 )) ) );
DO ( (@c) rlike (cot ( (!( @f )) )) );
--error ER_DATA_OUT_OF_RANGE
DO ( ('') rlike (cot ( ' %' )) );
--error ER_WRONG_ARGUMENTS
DO ( (-28277) regexp (period_add ( -10966, 1381205734 )) );
--error ER_INVALID_JSON_TEXT_IN_PARAM
DO ( (( @f )) rlike (json_depth ( 'key4' )) );
--error ER_DATA_OUT_OF_RANGE
DO ( ('-  ') regexp (cot ( right (':#.', 33) )) );
--error ER_DATA_OUT_OF_RANGE
DO ( (1) regexp (exp ( 64826 )) );
DO ( (@g) regexp (cot ( @f )) );
--error ER_DATA_OUT_OF_RANGE
DO ( (@b) regexp (exp ( 0x1fc5574c )) );
--error ER_DATA_OUT_OF_RANGE
DO ( (25091) rlike (exp ( 14373 )) );

--echo #
--echo # Bug#27183583: REGEXP OPERATIONS CANT BE KILLED
--echo #
SET GLOBAL regexp_time_limit = 1000000;

enable_connect_log;

connect(conn1, localhost, root);
let $conn1_id= `SELECT CONNECTION_ID()`;
send SELECT regexp_instr('AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAC', '(A+)+B');

connection default;
# Wait until the query is actually executing before we try to kill it
--let $wait_condition= SELECT count(*)=1 FROM performance_schema.threads WHERE processlist_info LIKE "SELECT regexp_instr%" AND processlist_state = "executing";
--source include/wait_condition.inc
replace_result $conn1_id <conn1_id>;
eval KILL QUERY $conn1_id;

connection conn1;
--error ER_QUERY_INTERRUPTED
reap;
disconnect conn1;
source include/wait_until_disconnected.inc;
connection default;
disable_connect_log;
SET GLOBAL regexp_time_limit = DEFAULT;


--echo #
--echo # Bug#27252630: REGEXP FUNCTIONS IGNORE NULLS
--echo #

SELECT regexp_instr ( 'a', 'a', NULL );
SELECT regexp_instr ( 'a', 'a', 1, NULL );
SELECT regexp_instr ( 'a', 'a', 1, 0, NULL );
SELECT regexp_instr ( 'a', 'a', 1, 0, 0, NULL );

SELECT regexp_like ( 'a', 'a', NULL );

SELECT regexp_replace ( 'a', 'a', 'a', NULL );
SELECT regexp_replace ( 'a', 'a', 'a', 1, NULL );
SELECT regexp_replace ( 'a', 'a', 'a', 1, 0, NULL );

SELECT regexp_substr ( 'a', 'a', NULL );
SELECT regexp_substr ( 'a', 'a', 1, NULL );
SELECT regexp_substr ( 'a', 'a', 1, 0, NULL );

--echo #
--echo # Bug#27232647: REGEX: ASSERTION FAILED: !STR || STR != M_PTR
--echo #

SELECT regexp_like( reverse(''), 123 );
SELECT regexp_like( soundex(@v1), 'abc' );
SELECT regexp_like( left('', ''), 'abc' );
SELECT regexp_like( repeat(@v1, 'abc'), 'abc' );

--echo #
--echo # Bug#27597980: ADD ERROR MESSAGE FOR INVALID CAPTURE GROUP NAME
--echo #

--error ER_REGEXP_INVALID_CAPTURE_GROUP_NAME
SELECT regexp_replace( 'abc' , 'abc', '$abc' );

--echo #
--echo # Bug#27595368: ASSERTION FAILED: 0 IN ITEM_PARAM::VAL_REAL WITH
--echo # PREPARED STATEMENT
--echo #

SET @s := "SELECT regexp_like( '', '', ? / '' )";
PREPARE stmt FROM @s;
--error ER_WRONG_ARGUMENTS
EXECUTE stmt;
--error ER_WRONG_ARGUMENTS
EXECUTE stmt;

CREATE TABLE t1 ( match_parameter CHAR(1) );
INSERT INTO t1 VALUES ( 'i' ), ( 'c' ), ( 'i' ), ( 'c' );
SELECT match_parameter, regexp_like ( 'a', 'A', match_parameter ) FROM t1;
DROP TABLE t1;

--echo #
--echo # Bug#27612255: VALGRIND WARNING ON INVALID CAPTURE GROUP
--echo #

--error ER_REGEXP_INVALID_CAPTURE_GROUP_NAME
SELECT regexp_replace( ' F' , '^ ', '[,$' );

--echo #
--echo # Bug#27751277: ASSERTION FALSE IN REGEXP::CHECK_ICU_STATUS
--echo #

--error ER_REGEXP_INVALID_FLAG
SELECT regexp_instr( 'abc', '(?-' );

--echo #
--echo # Bug#27743722 REGEXP::EVALEXPRTOCHARSET:
--echo #              REFERENCE BINDING TO MISALIGNED ADDRESS
--echo #

select regexp_instr(char('313:50:35.199734'using utf16le),
                    cast(uuid() as char character set utf16le));

--echo #
--echo # Bug#27992118: REGEXP_REPLACE ACCUMULATING RESULT
--echo #

CREATE TABLE t1 ( a VARCHAR(10) );
INSERT INTO t1 VALUES ('a a a'), ('b b b'), ('c c c');
SELECT regexp_replace(a, '^([[:alpha:]]+)[[:space:]].*$', '$1') FROM t1;
DROP TABLE t1;

--echo #
--echo # Bug#28027093: REGEXP_REPLACE TRUNCATE UPDATE
--echo #

CREATE TABLE t1 ( a CHAR(3) );
INSERT INTO t1 VALUES ( regexp_replace ('a', 'a', 'x') );
SELECT * FROM t1;
UPDATE t1 SET a = regexp_replace ( 'b', 'b', 'y' );
SELECT * FROM t1;
DROP TABLE t1;

CREATE TABLE t1 ( a CHAR(3) );
INSERT INTO t1 VALUES ( regexp_substr ('a', 'a', 1) );
SELECT * FROM t1;
UPDATE t1 SET a = regexp_substr ('b', 'b', 1);
SELECT * FROM t1;
DROP TABLE t1;

--echo #
--echo # Bug#27682225: WRONG METADATA FOR REGEXP FUNCTIONS
--echo #

CREATE TABLE t1 AS SELECT
  regexp_instr( 'a', 'a' ) AS a,
  regexp_like( 'a', 'a' ) AS b,
  regexp_replace( 'abc', 'b', 'x' ) AS c,
  regexp_substr( 'a', 'a' ) AS d,
  regexp_substr( repeat('a', 512), 'a' ) AS e,
  regexp_substr( repeat('a', 513), 'a' ) AS f;

SHOW CREATE TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;


--echo #
--echo # Bug#29231490: REGEXP FUNCTIONS FAIL TO USE CODEPOINT POSITIONS
--echo #

--echo # Sushi emoji, may not render in your editor.
SET NAMES DEFAULT;

SELECT regexp_instr( '🍣🍣a', '🍣', 2 );
SELECT regexp_instr( '🍣🍣a', 'a', 3 );
--error ER_REGEXP_INDEX_OUTOFBOUNDS_ERROR
SELECT regexp_instr( '🍣🍣a', 'a', 4 );

SELECT regexp_substr( 'a🍣b', '.', 1 );
SELECT regexp_substr( 'a🍣b', '.', 2 );
SELECT regexp_substr( 'a🍣b', '.', 3 );
SELECT regexp_substr( 'a🍣b', '.', 4 );

SELECT regexp_substr( 'a🍣🍣b', '.', 1 );
SELECT regexp_substr( 'a🍣🍣b', '.', 2 );
SELECT regexp_substr( 'a🍣🍣b', '.', 3 );
SELECT regexp_substr( 'a🍣🍣b', '.', 4 );
SELECT regexp_substr( 'a🍣🍣b', '.', 5 );

SELECT regexp_replace( '🍣🍣🍣', '.', 'a', 2 );
SELECT regexp_replace( '🍣🍣🍣', '.', 'a', 2, 2 );

--source ../mysql-test/include/cleanup_icu_utils.inc

--echo #
--echo # REGEXP_REPLACE DOES NOT CONVERT RESULT CHARACTER SET
--echo #

SELECT hex(regexp_replace( convert( 'abcd' using utf8mb4 ), 'c', ''));
SELECT hex(regexp_replace( convert( 'abcd' using utf16 ), 'c', ''));
SELECT hex(regexp_substr( convert( 'abcd' using utf8mb4 ), 'abc'));
SELECT hex(regexp_substr( convert( 'abcd' using utf16 ), 'abc'));

--echo #
--echo # Test of the code path that elides character set conversion when the
--echo # target column has the same character set as ICU produces. This depends
--echo # on the architecture, and so we try both big and little endian.
--echo #
CREATE TABLE t1 (
  a CHAR(10) CHARACTER SET utf16le,
  b CHAR(10) CHARACTER SET utf16
);

INSERT INTO t1 VALUES (
  regexp_substr( convert('abcd' using utf16le), 'abc' ),
  regexp_substr( convert('abcd' using utf16), 'abc' ));

INSERT INTO t1 VALUES (
  regexp_substr( 'abcd', 'abc' ),
  regexp_substr( 'abcd', 'abc' ));

SELECT * FROM t1;

DROP TABLE t1;

--echo #
--echo # Bug#31031886: EVALUATING REGEXP CHARACTER CLASSES IN BINARY
--echo #               STRING CONTEXTS
--echo #

--echo #
--echo # Small part of the test. Making sure you can't mix binary strings with
--echo # any other character set. See regular_expressions_utf-8_big.test for the
--echo # big part.
--echo #

--error ER_CHARACTER_SET_MISMATCH
SELECT regexp_like('1', x'01');
--error ER_CHARACTER_SET_MISMATCH
SELECT regexp_like(x'01', '1');

--error ER_CHARACTER_SET_MISMATCH
SELECT regexp_replace('01', '01', x'02');
--error ER_CHARACTER_SET_MISMATCH
SELECT regexp_replace('01', x'01', '02');
--error ER_CHARACTER_SET_MISMATCH
SELECT regexp_replace('01', x'01', x'02');
--error ER_CHARACTER_SET_MISMATCH
SELECT regexp_replace(x'01', '01', '02');
--error ER_CHARACTER_SET_MISMATCH
SELECT regexp_replace(x'01', '01', x'02');
--error ER_CHARACTER_SET_MISMATCH
SELECT regexp_replace(x'01', x'01', '02');

--echo # We repeat the above test for other char types, but this time we
--echo # make it a little smaller, just to check that the same rule applies for
--echo # all the relevant data types.
CREATE TABLE t1(a CHAR(1));
CREATE TABLE t2(a BLOB);
CREATE TABLE t3(a TEXT);

INSERT INTO t1 VALUES('1');
INSERT INTO t2 VALUES('1');
INSERT INTO t3 VALUES('1');

--error ER_CHARACTER_SET_MISMATCH
SELECT regexp_like(a, x'01') FROM t1;
SELECT regexp_like(a, x'01') FROM t2;
--error ER_CHARACTER_SET_MISMATCH
SELECT regexp_like(a, x'01') FROM t3;
--error ER_CHARACTER_SET_MISMATCH
SELECT regexp_like(x'01', a) FROM t1;
SELECT regexp_like(x'01', a) FROM t2;
--error ER_CHARACTER_SET_MISMATCH
SELECT regexp_like(x'01', a) FROM t3;

DROP TABLE t1, t2, t3;

--echo # When any argument is NULL, the result of a regular expression function
--echo # is also NULL, obviously. These tests are only here to show that we
--echo # don't mistakenly reject NULL values based on character set.
SELECT regexp_instr(1, 'a');
SELECT regexp_instr('a', 1);
SELECT regexp_instr(NULL, 'a');
SELECT regexp_instr('a', NULL);

SELECT regexp_like(1, 'a');
SELECT regexp_like('a', 1);
SELECT regexp_like(NULL, 'a');
SELECT regexp_like('a', NULL);

SELECT regexp_replace(1, 1, 'a');
SELECT regexp_replace(1, 'a', 1);
SELECT regexp_replace(1, 'a', 'a');
SELECT regexp_replace('a', 1, 1);
SELECT regexp_replace('a', 1, 'a');
SELECT regexp_replace('a', 'a', 1);

SELECT regexp_replace(NULL, NULL, 'a');
SELECT regexp_replace(NULL, 'a', NULL);
SELECT regexp_replace(NULL, 'a', 'a');
SELECT regexp_replace('a', NULL, NULL);
SELECT regexp_replace('a', NULL, 'a');
SELECT regexp_replace('a', 'a', NULL);

SELECT regexp_substr(1, 'a');
SELECT regexp_substr('a', 1);

SELECT regexp_substr(NULL, 'a');
SELECT regexp_substr('a', NULL);

--echo #
--echo # Bug#31031888: REGEXP_REPLACE AND REGEXP_SUBSTR STILL ADD NULL
--echo # BYTES
--echo #

SELECT hex(regexp_replace(x'01', x'01', x'02'));

SELECT hex(regexp_substr(x'FFFF', x'FFFF'));

CREATE TABLE t1 AS SELECT regexp_substr(x'01', x'01');
SHOW CREATE TABLE t1;
DROP TABLE t1;

CREATE TABLE t1 AS SELECT regexp_replace(x'01', x'01', x'02');
SHOW CREATE TABLE t1;
DROP TABLE t1;

--echo #
--echo # Bug #31514995 REGEXP::CHECK_ICU_STATUS: ASSERTION `FALSE' FAILED.
--echo #

--error ER_REGEX_NUMBER_TOO_BIG
do regexp_like(0, "^{18446744073709551616");
--error ER_REGEX_NUMBER_TOO_BIG
do regexp_instr(0, "^{18446744073709551616");

--echo #
--echo # Bug #32053093 ASSERTION `MAYBE_NULL' FAILED.
--echo #               WHEN USING REGEXP_LIKE FUNCTION
--echo #

SET SQL_MODE='';

--error ER_REGEXP_MISSING_CLOSE_BRACKET
DO INSERT(regexp_like(1, '['), 0, 1, '');

--error ER_REGEXP_MISSING_CLOSE_BRACKET
DO INSERT(regexp_instr(1, '['), 0, 1, '');

SET NAMES latin1;
--error ER_REGEXP_MISSING_CLOSE_BRACKET
DO INSERT(regexp_replace(1, '[', 42), 0, 1, '');
SET NAMES DEFAULT;

SET SQL_MODE=DEFAULT;

--echo #
--echo # Bug #33290245 Item_func_st_distance_sphere::val_real():
--echo #               Assertion `is_nullable()' failed.

--replace_regex /utf16le/utf16/
--error ER_CANNOT_CONVERT_STRING
select ( _utf32', ,*') regexp 13946;
--replace_regex /utf16le/utf16/
--error ER_CANNOT_CONVERT_STRING
select regexp_instr( ( _utf32', ,*'), 13946);
--replace_regex /utf16le/utf16/
--error ER_CANNOT_CONVERT_STRING
select regexp_replace( ( _utf32', ,*'), 13946, 42);
--replace_regex /utf16le/utf16/
--error ER_CANNOT_CONVERT_STRING
select regexp_substr( ( _utf32', ,*'), 13946);

--echo #
--echo # Bug #33326003 regexp::check_icu_status,
--echo #               handle Unicode Character Names without debug assert
--echo #

# For the metacharacter '\N' queries below:
# System ICU will say 1
# Bundled ICU will say
# ERROR 4078 (HY000): Missing data file in regular expression library.
# This is caused by a missing data file: icudt65l/unames.icu
# This data is builtin to libicudata.so for system ICU.
# To support it for bundled ICU, we need to install the data file,
# and point ICU to its location.

# \N{UNICODE CHARACTER NAME}

SELECT 'a' regexp '\\N{latin small letter a}';
select 'ǅ' regexp '\\N{Latin Capital Letter D with Small Letter Z with Caron}';

# \p{UNICODE PROPERTY NAME}
# \P{UNICODE PROPERTY NAME}

SELECT 'a' regexp '\\p{alphabetic}';
SELECT 'a' regexp '\\P{alphabetic}';

SELECT '👌🏾' regexp '\\p{Emoji}\\p{Emoji_modifier}';

SELECT 'a' regexp '\\p{Lowercase_letter}';
SELECT 'a' regexp '\\p{Uppercase_letter}';

SELECT 'A' regexp '\\p{Lowercase_letter}';
SELECT 'A' regexp '\\p{Uppercase_letter}';

SELECT 'a' collate utf8mb4_0900_as_cs regexp '\\p{Lowercase_letter}';
SELECT 'A' collate utf8mb4_0900_as_cs regexp '\\p{Lowercase_letter}';

SELECT 'a' collate utf8mb4_0900_as_cs regexp '\\p{Uppercase_letter}';
SELECT 'A' collate utf8mb4_0900_as_cs regexp '\\p{Uppercase_letter}';

--echo #
--echo # Bug#35367340 Assertion `!thd->is_error()' failed|sql/sql_base.cc
--echo #

CREATE TABLE t1 (
  col_datetime DATETIME DEFAULT NULL,
  col_date DATE DEFAULT NULL
);

INSERT INTO t1 VALUES ('1981-09-28 23:58:32','2016-04-27');

CREATE TABLE t2 (
  col_varchar_key VARCHAR(1) DEFAULT NULL,
  col_int INT DEFAULT NULL,
  col_varchar VARCHAR(1) DEFAULT NULL
);

INSERT INTO t2 VALUES (NULL,42,'a');

--error ER_TRUNCATED_WRONG_VALUE
UPDATE t1 SET col_datetime = 8
WHERE (col_date) IN
( SELECT
  REGEXP_INSTR( col_varchar_key, '[a-z]+' , 385025119 , 1, 'm' )
  FROM t2
);

--error ER_WRONG_ARGUMENTS
UPDATE t1 SET col_datetime = 8
WHERE (col_date) IN
( SELECT
  REGEXP_INSTR( col_varchar_key, '[a-z]+' , 385025119 , 1, col_int )
  FROM t2
);

--error ER_TRUNCATED_WRONG_VALUE
UPDATE t1 SET col_datetime = 8
WHERE (col_date) IN
( SELECT
  REGEXP_INSTR( col_varchar_key, '[a-z]+' , 385025119 , 1, col_varchar )
  FROM t2
);

DROP TABLE t1, t2;
