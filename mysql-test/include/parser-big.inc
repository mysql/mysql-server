#
# Input parameters:
#
#  $min20087571: minimal number of "(a=1 and b=2)" disjuncts
#  $max20087571: maximal number of "(a=1 and b=2)" disjuncts
#

--echo #
--echo # Bug #20087571: 5.7 EASILY HITS ERROR 1436 (HY000):
--echo #                THREAD STACK OVERRUN ERRORS ON MANY EXPR'S
--echo #

# we need a larger max_allowed_packet for the REPEAT() function
SELECT @tmp_max:= @@global.max_allowed_packet;
SET @@global.max_allowed_packet=16999424;
# switching connection to allow the new max_allowed_packet take effect
--connect (newconn, localhost, root,,)

CREATE TABLE t1 (a INT UNSIGNED, b INT UNSIGNED, PRIMARY KEY (a, b))
ENGINE=INNODB;

DELIMITER |;

CREATE PROCEDURE p1(p_min_terms INT UNSIGNED, p_max_terms INT UNSIGNED, p_incr
INT UNSIGNED)
begin
  DECLARE v_sql LONGTEXT CHARSET LATIN1;
  DECLARE i INT UNSIGNED DEFAULT 1;
  SET i:=greatest(1, p_min_terms);
  REPEAT
    SET @terms:=f1(i);
    SET v_sql:=concat("delete from t1 where ",@terms);
    SET @t:=v_sql;
    SELECT i AS 'numterms', length(@t) AS 'query lengt', p_incr AS 'increment';
    PREPARE s FROM @t;
    EXECUTE s;
    DEALLOCATE PREPARE s;
    SET i:=i + p_incr;
  UNTIL i > p_max_terms end repeat;
END|

CREATE FUNCTION f1(p_numterms INT UNSIGNED) RETURNS LONGTEXT
BEGIN
  DECLARE ret LONGTEXT DEFAULT '';
  SET ret:=CONCAT(REPEAT("(a=1 and b=2) or ",GREATEST(p_numterms-1,1)),"(a=2 and b=1)");
  RETURN RET;
END|

DELIMITER ;|

eval CALL p1($min20087571, $max20087571, 100000);
SELECT LENGTH(@terms);
SELECT @@global.max_allowed_packet;

DROP FUNCTION f1;
DROP PROCEDURE p1;
DROP TABLE t1;

--connection default
SET @@global.max_allowed_packet:= @tmp_max;
--disconnect newconn

--echo #
