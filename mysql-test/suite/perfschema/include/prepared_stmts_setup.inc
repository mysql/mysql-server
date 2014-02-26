#
# Creating various prepared statements. 
# HELPER include/prepared_stmts_setup.inc 
#

CREATE TABLE t1 (a INT NOT NULL);
INSERT INTO t1 VALUES (4), (8), (11), (32), (80);

# Prepared statments

PREPARE st1 FROM 'SELECT SQRT(POW(?,2) + POW(?,2)) AS hypotenuse';

SET @table = 't1';
SET @s = CONCAT('SELECT * FROM ', @table);
PREPARE st2 FROM @s;

PREPARE st3 FROM 'INSERT INTO t1 SELECT * FROM t1 WHERE a<=?';

PREPARE st4 FROM
  '(SELECT a FROM t1) UNION (SELECT a+10 FROM t1) ORDER BY RAND()*0+a';
