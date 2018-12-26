#
# Test of rules where the parser gives warnings about the originial
# query, pattern or replacement.
#
call mtr.add_suppression("Plugin rewriter reported: 'Some rules failed to load.'");
# Query rewrite plugin was installed.
INSERT INTO query_rewrite.rewrite_rules ( pattern, replacement )
VALUES ( 'LOAD DATA INFILE "tab1.data" INTO TABLE test.t1',
'LOAD DATA INFILE "tab2.data" INTO TABLE test.t1' ),
( 'DO CONCAT("a", "b")', 'DO CONCAT("c", "d")' );
CALL query_rewrite.flush_rewrite_rules();
ERROR 45000: Loading of some rule(s) failed.
SELECT * FROM query_rewrite.rewrite_rules;
id	pattern	pattern_database	replacement	enabled	message
1	LOAD DATA INFILE "tab1.data" INTO TABLE test.t1	NULL	LOAD DATA INFILE "tab2.data" INTO TABLE test.t1	YES	Statement type of pattern not supported.
2	DO CONCAT("a", "b")	NULL	DO CONCAT("c", "d")	YES	Statement type of pattern not supported.
INSERT INTO query_rewrite.rewrite_rules ( pattern, replacement )
VALUES ( 'SHOW WARNINGS', 'SHOW /* I rewrote it */ WARNINGS' );
CALL query_rewrite.flush_rewrite_rules();
ERROR 45000: Loading of some rule(s) failed.
# We want the note about the query being rewritten here ...
SHOW WARNINGS;
Level	Code	Message
Error	1644	Loading of some rule(s) failed.
SHOW ERRORS;
Level	Code	Message
Error	1644	Loading of some rule(s) failed.
GET DIAGNOSTICS CONDITION 1 @p1 = RETURNED_SQLSTATE, @p2 = MESSAGE_TEXT;
SELECT @p1;
@p1
45000
SELECT @p2;
@p2
Loading of some rule(s) failed.
# ... but not here, since we failed parsing after seeing SHOW WARNINGS.
SHOW WARNINGS and give me a parse error, please;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'and give me a parse error, please' at line 1
DELETE FROM query_rewrite.rewrite_rules;
# Test of warnings when we rewrite the diagnostics statement.  echo #
# We disable mtr's SHOW WARNINGS here, as it would just be confusing
# when it gets rewritten without us seeing it.
INSERT INTO query_rewrite.rewrite_rules ( pattern, replacement )
VALUES ( 'SHOW WARNINGS', 'SELECT "used to be SHOW WARNINGS"' ),
( 'SELECT 1', 'SHOW WARNINGS' );
CALL query_rewrite.flush_rewrite_rules();
ERROR 45000: Loading of some rule(s) failed.
SELECT 1;
Level	Code	Message
Error	1644	Loading of some rule(s) failed.
Note	1105	Query 'SELECT 1' rewritten to 'SHOW WARNINGS' by a query rewrite plugin
SHOW WARNINGS;
Level	Code	Message
Error	1644	Loading of some rule(s) failed.
Note	1105	Query 'SELECT 1' rewritten to 'SHOW WARNINGS' by a query rewrite plugin
Warnings:
Warning	1620	Plugin is busy and will be uninstalled on shutdown
# Query rewrite plugin was queued for uninstalling.
