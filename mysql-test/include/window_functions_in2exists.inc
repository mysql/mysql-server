# Some cases where there's a window function in the SELECT list
# and we cannot materialize (e.g. due to correlation), so we are
# forced to use in2exists. The old optimizer cannot handle these
# and must give ER_NOT_SUPPORTED_YET. The hypergraph optimizer,
# on the other hand, can split these out of HAVING and execute them
# correctly. Thus, we include this file from separate .test files
# that run only under the old and hypergraph optimizer, respectively.

CREATE TABLE t(a INT);
INSERT INTO t VALUES (1),(2),(3);
ANALYZE TABLE t;

--echo =ALL => NOT EXISTS (select non-equal rows)
--error 0, ER_NOT_SUPPORTED_YET
SELECT * FROM t WHERE a = ALL(SELECT ROW_NUMBER() OVER () + 1 FROM t);
--echo <>SOME => EXISTS (select non-equal rows)
--error 0, ER_NOT_SUPPORTED_YET
SELECT * FROM t WHERE a <> SOME(SELECT ROW_NUMBER() OVER () + 1 FROM t);
--error 0, ER_NOT_SUPPORTED_YET
SELECT * FROM t WHERE a <> ANY(SELECT ROW_NUMBER() OVER () + 1 FROM t);

--echo # Correlated => no subquery materialization.
--echo # IN => EXISTS, error
--error 0, ER_NOT_SUPPORTED_YET
SELECT * FROM t upper
    WHERE upper.a IN (SELECT ROW_NUMBER() OVER () FROM t
                      WHERE t.a > upper.a);

--echo # Correlated => no subquery materialization.
--echo # >ANY => EXISTS(select smaller rows), error
--error 0, ER_NOT_SUPPORTED_YET
SELECT * FROM t upper
    WHERE upper.a > ANY (SELECT ROW_NUMBER() OVER () FROM t
                         WHERE t.a > upper.a);
--error 0, ER_NOT_SUPPORTED_YET
SELECT * FROM t upper
    WHERE upper.a > ALL (SELECT ROW_NUMBER() OVER () FROM t
                         WHERE t.a > upper.a);

DROP TABLE t;

--echo #
--echo # Bug#25601138 WINDOW FUNCTION IN CORRELATED SUBQUERY GIVES WRONG RESULT
--echo #

CREATE TABLE t1(i INT, j INT, k INT);
INSERT INTO t1 VALUES (1,1,1),(2,2,2);
--error 0, ER_NOT_SUPPORTED_YET
select * from t1 AS upper where i+1
 IN (select row_number() over () + upper.i from t1 );
DROP TABLE t1;
