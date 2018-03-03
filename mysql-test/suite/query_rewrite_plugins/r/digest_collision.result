#
# Test of the case when two rewrite rule pattern have the same digest.
#
SET sql_mode = concat ( @@sql_mode, ',PIPES_AS_CONCAT' );
CREATE TABLE t1 ( a VARCHAR(10) );
# Query rewrite plugin was installed.
# This will supposedly fill up the token buffer.
# We have to keep entries in the error log unique, we can't clear it.
SET @query_term = 'SELECT * FROM test.t1 WHERE a = "digest_collision.test"';
SET @a_union_branch = ' UNION ' || @query_term;
SET @tail1 = 'UNION SELECT 1';
SET @tail2 = 'UNION SELECT 2';
SELECT character_maximum_length INTO @max_pattern_length
FROM information_schema.columns
WHERE table_name = 'rewrite_rules'
AND column_name = 'pattern';
SET @no_unions_that_fit =
floor(
( @max_pattern_length - char_length(@query_term) - char_length(@tail1) ) /
char_length(@a_union_branch));
SELECT @query_term, @a_union_branch, @max_pattern_length, @no_unions_that_fit;
@query_term	@a_union_branch	@max_pattern_length	@no_unions_that_fit
SELECT * FROM test.t1 WHERE a = "digest_collision.test"	 UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test"	5000	79
SET @pattern1 = @query_term || repeat( @a_union_branch, @no_unions_that_fit);
SET @pattern2 = concat ( @pattern1, @tail1 );
SET @nonmatching_query = concat ( @pattern1, @tail2 );
INSERT INTO query_rewrite.rewrite_rules ( pattern, replacement  )
VALUES ( @pattern1, 'SELECT "Rewrite rule #1"' ),
( @pattern2, 'SELECT "Rewrite rule #2"' );
CALL query_rewrite.flush_rewrite_rules();
SELECT group_concat(replacement) =
'SELECT "Rewrite rule #1",SELECT "Rewrite rule #2"'
AS digests_indeed_collide
FROM query_rewrite.rewrite_rules
GROUP BY pattern_digest;
digests_indeed_collide
1
SELECT char_length(pattern) FROM query_rewrite.rewrite_rules;
char_length(pattern)
4953
4967
PREPARE pattern1_stmt FROM @pattern1;
Warnings:
Note	1105	Query 'SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT *
EXECUTE pattern1_stmt;
Rewrite rule #1
Rewrite rule #1
PREPARE pattern2_stmt FROM @pattern2;
Warnings:
Note	1105	Query 'SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT *
EXECUTE pattern2_stmt;
Rewrite rule #2
Rewrite rule #2
SET @@global.rewriter_verbose = 2;
PREPARE nonmatching_stmt FROM @nonmatching_query;
EXECUTE nonmatching_stmt;
a
2
SET @@global.rewriter_verbose = DEFAULT;
CREATE TABLE IF NOT EXISTS error_log ( line TEXT );
DELETE FROM error_log;
LOAD DATA LOCAL INFILE 'ERROR_LOG'
INTO TABLE error_log FIELDS TERMINATED BY '\t';
UPDATE error_log SET line = replace ( line, '\r', '' );
SELECT search_error_log( '[Note]', '%Plugin Rewriter reported: ''Statement "SELECT * FROM test.t1 ' ||
'WHERE a = "digest_collision.test"%' );
search_error_log( '[Note]', '%Plugin Rewriter reported: ''Statement "SELECT * FROM test.t1 ' ||
'WHERE a = "digest_collision.test"%' )
[Note] [MY-011235] [Server] Plugin Rewriter reported: 'Statement "SELECT * FROM test.t1 WHERE a = "digest_collision.test" UNION SELECT * FROM test.t1 WHERE a = "diges..." with digest "3c579229047547d5b35dbb0f1f00f0d1ff521755e231f83952e6d51b2019818b" matched some rule but had different parse tree and/or literals.'
DROP TABLE t1;
Warnings:
Warning	1620	Plugin is busy and will be uninstalled on shutdown
# Query rewrite plugin was queued for uninstalling.
DROP TABLE error_log;
DROP FUNCTION search_error_log;
SET sql_mode = DEFAULT;
