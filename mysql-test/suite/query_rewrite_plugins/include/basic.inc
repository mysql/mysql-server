CREATE TABLE t1 ( a VARCHAR(10), b VARCHAR(10) );
INSERT INTO t1 VALUES ( 'abc', 'def' ), ( 'ghi', 'klm' ), ( 'nop', 'qrs' );

CREATE TABLE t2 ( a VARCHAR(10) );
INSERT INTO t2 VALUES ( 'abc' ), ( 'klm' );

--echo # Test of literals matching.

INSERT INTO query_rewrite.rewrite_rules ( pattern, replacement )
VALUES ( 'SELECT ?', 'SELECT "literal"' );

SELECT * FROM query_rewrite.rewrite_rules;
CALL query_rewrite.flush_rewrite_rules();
SELECT * FROM query_rewrite.rewrite_rules;

SELECT NULL;
SELECT 'abc';
SELECT 1;
SELECT 1.1;
SELECT 123456789123456789123456789123456789123456789123456789;

--echo # Check our status variables.
SHOW STATUS LIKE 'Rewriter%';

DELETE FROM query_rewrite.rewrite_rules;

INSERT INTO query_rewrite.rewrite_rules ( pattern, replacement )
VALUES ( 'SELECT * FROM test.t1 WHERE ( a = ? AND TRUE ) OR b = ?',
         'SELECT a FROM test.t1 WHERE a = ?' ),

       ( 'SELECT a FROM test.t1',
         'SELECT * FROM test.t1 WHERE a = \'abc\'' ),

       ( 'SELECT a FROM test.t1 WHERE b = ?',
         'SELECT * FROM test.t1 WHERE b = ?' ),

       ( 'SELECT * FROM test.t2',
         'SELECT * FROM test.t1 JOIN test.t2 ON t1.a = t2.a' ),

       ( 'SELECT * FROM test.t1 WHERE a = ? OR b = ?',
         'SELECT * FROM test.t1 WHERE b = ? OR a = ?' );

--sorted_result
SELECT * FROM query_rewrite.rewrite_rules;
CALL query_rewrite.flush_rewrite_rules();
--sorted_result
SELECT * FROM query_rewrite.rewrite_rules;

--echo # Check a query that doesn't have a rewrite rule doesn't get rewritten.
SELECT b FROM test.t1;

--echo # Check a query with just one parameter.
SELECT a FROM test.t1 WHERE b = 'qrs';

--echo # Check a rule with parameter truncation (fewer parameters in the
--echo # output query than in the input query);
SELECT * FROM test.t1 WHERE ( a = 'abc' AND TRUE ) OR b = 'klm';
SELECT * FROM test.t1 WHERE ( a = 'abc' AND FALSE ) OR b = 'klm';

--echo # Check a non parameterized query.
SELECT * from t1 WHERE a = 'abc';

--echo # Check that a non-rewritten query does not yield a warning.
SELECT b FROM test.t1;

--echo # Check that a query is not rewritten if the query corresponds to a
--echo # replacement.
SELECT a FROM test.t1;

--echo # Check that we can execute a rewrite more than once.
SELECT * FROM test.t2;

SELECT * FROM test.t2;

--echo # Remove the warnings.
SELECT b FROM test.t1;

--echo # Check parameter switching in a query rewrite
SELECT * FROM test.t1 WHERE a = 'def' OR b = 'ghi';

--echo # Verify with whitespace.
SELECT * FROM test.t1   WHERE   a  = 'def' OR b  =  'ghi';

--echo # Verify with comments.
SELECT * FROM test.t1 WHERE a =/* 'def' ? */'def' OR b = 'ghi';

--echo # Check our status variables.
SHOW STATUS LIKE 'Rewriter%';

DROP TABLE t1;
DROP TABLE t2;

DELETE FROM query_rewrite.rewrite_rules;

--echo # Test of literals matching.
INSERT INTO query_rewrite.rewrite_rules ( pattern, replacement )
VALUES ( 'SELECT 1, ?', 'SELECT "rewritten w/rule 1"' ),
       ( 'SELECT 2, ?', 'SELECT "rewritten w/rule 2"' ),
       ( 'SELECT "The_original_query"', 'SELECT "The_rewritten_query"');

--sorted_result
SELECT * FROM query_rewrite.rewrite_rules;
CALL query_rewrite.flush_rewrite_rules();
--sorted_result
SELECT * FROM query_rewrite.rewrite_rules;

SELECT 1, 1;
SELECT 1, 2;
SELECT 2, 1;
SELECT 2, 2;
SELECT 3, 1;
SELECT 3, 2;
SELECT 'The_original_query';

DELETE FROM query_rewrite.rewrite_rules;
