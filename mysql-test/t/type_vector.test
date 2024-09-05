--echo #
--echo # Bug#37009633 - Allow string format vectors with whitespaces
--echo #

SELECT TO_VECTOR('[1,2]');
SELECT TO_VECTOR('[1,  2]');
SELECT TO_VECTOR('[1   ,2]');
SELECT TO_VECTOR('  [1  ,2]');
SELECT TO_VECTOR('[1  ,2]   ');
SELECT TO_VECTOR('  [1  ,2]   ');
SELECT TO_VECTOR('  [1  ,    2]   ');
SELECT TO_VECTOR('  [1.0  ,    2.0]   ');
SELECT TO_VECTOR('  [1.,0  ,    2.0]   ');
--error ER_TO_VECTOR_CONVERSION
SELECT TO_VECTOR('  1.,0  ,    2.0]   ');
--error ER_TO_VECTOR_CONVERSION
SELECT TO_VECTOR('  [1.,0  ,    2.0   ');
--error ER_TO_VECTOR_CONVERSION
SELECT TO_VECTOR('  [1  x,0  ,    2.0]   ');
--error ER_TO_VECTOR_CONVERSION
SELECT TO_VECTOR('  [1.,0  ,    2.0]   a');
--error ER_TO_VECTOR_CONVERSION
SELECT TO_VECTOR('  [1.,0  ,    2.0]]');

CREATE TABLE A(a1 INT, a2 VARCHAR(30) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci);
INSERT INTO A VALUES (1, '[1,2,3]'),(2, ' [1,2,3] ');
SELECT TO_VECTOR(a2) FROM A;
DROP TABLE A;

CREATE TABLE B(b1 INT, b2 VARCHAR(30) CHARACTER SET utf32);
INSERT INTO B VALUES (1, '[1,2,3]'),(2, ' [1,2,3] ');
SELECT TO_VECTOR(b2) FROM B;
DROP TABLE B;
