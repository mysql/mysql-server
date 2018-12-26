SET SESSION binlog_format = 'STATEMENT';
include/assert.inc [assert that @@SESSION.binlog_format is STATEMENT]
CREATE TEMPORARY TABLE t1 (a VARCHAR(100));
#
# Test switching binlog_format from STATEMENT to STATEMENT
# is disallowed when the session has open temporary table(s).
#
SET SESSION binlog_format = STATEMENT;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from STATEMENT to 'ROW'
# is disallowed when the session has open temporary table(s).
#
SET SESSION binlog_format = 'ROW';
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from STATEMENT to MIXED
# is disallowed when the session has open temporary table(s).
#
SET SESSION binlog_format = MIXED;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
# Set binlog_format to ROW
DROP TEMPORARY TABLE t1;
SET SESSION binlog_format = 'ROW';
include/assert.inc [assert that @@SESSION.binlog_format is set to ROW]
CREATE TEMPORARY TABLE t1 (a VARCHAR(100));
#
# Test switching binlog_format from ROW to MIXED
# is disallowed when the session has open temporary table(s).
#
SET SESSION binlog_format = MIXED;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from ROW to STATEMENT
# is disallowed when the session has open temporary table(s).
#
SET SESSION binlog_format = STATEMENT;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from ROW to ROW
# is disallowed when the session has open temporary table(s).
#
SET SESSION binlog_format = ROW;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
# Set binlog_format to MIXED
DROP TEMPORARY TABLE t1;
SET SESSION binlog_format = 'MIXED';
include/assert.inc [assert that @@SESSION.binlog_format is set to MIXED]
CREATE TEMPORARY TABLE t1 (a VARCHAR(100));
#
# Test switching binlog_format from MIXED to MIXED
# is disallowed when the session has open temporary table(s).
#
SET SESSION binlog_format = MIXED;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from MIXED to STATEMENT
# is disallowed when the session has open temporary table(s).
#
SET SESSION binlog_format = STATEMENT;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from MIXED to ROW
# is disallowed when the session has open temporary table(s).
#
SET SESSION binlog_format = ROW;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
# Reset binlog_format to STATEMENT
DROP TEMPORARY TABLE t1;
SET SESSION binlog_format = STATEMENT;
include/assert.inc [assert that @@SESSION.binlog_format is set to STATEMENT]
SET @@SESSION.binlog_format = 'STATEMENT';
include/assert.inc [assert that @@SESSION.binlog_format is STATEMENT]
CREATE TEMPORARY TABLE t1 (a VARCHAR(100));
#
# Test switching binlog_format from STATEMENT to STATEMENT
# is disallowed when the session has open temporary table(s).
#
SET @@SESSION.binlog_format = STATEMENT;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from STATEMENT to 'ROW'
# is disallowed when the session has open temporary table(s).
#
SET @@SESSION.binlog_format = 'ROW';
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from STATEMENT to MIXED
# is disallowed when the session has open temporary table(s).
#
SET @@SESSION.binlog_format = MIXED;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
# Set binlog_format to ROW
DROP TEMPORARY TABLE t1;
SET @@SESSION.binlog_format = 'ROW';
include/assert.inc [assert that @@SESSION.binlog_format is set to ROW]
CREATE TEMPORARY TABLE t1 (a VARCHAR(100));
#
# Test switching binlog_format from ROW to MIXED
# is disallowed when the session has open temporary table(s).
#
SET @@SESSION.binlog_format = MIXED;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from ROW to STATEMENT
# is disallowed when the session has open temporary table(s).
#
SET @@SESSION.binlog_format = STATEMENT;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from ROW to ROW
# is disallowed when the session has open temporary table(s).
#
SET @@SESSION.binlog_format = ROW;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
# Set binlog_format to MIXED
DROP TEMPORARY TABLE t1;
SET @@SESSION.binlog_format = 'MIXED';
include/assert.inc [assert that @@SESSION.binlog_format is set to MIXED]
CREATE TEMPORARY TABLE t1 (a VARCHAR(100));
#
# Test switching binlog_format from MIXED to MIXED
# is disallowed when the session has open temporary table(s).
#
SET @@SESSION.binlog_format = MIXED;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from MIXED to STATEMENT
# is disallowed when the session has open temporary table(s).
#
SET @@SESSION.binlog_format = STATEMENT;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from MIXED to ROW
# is disallowed when the session has open temporary table(s).
#
SET @@SESSION.binlog_format = ROW;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
# Reset binlog_format to STATEMENT
DROP TEMPORARY TABLE t1;
SET @@SESSION.binlog_format = STATEMENT;
include/assert.inc [assert that @@SESSION.binlog_format is set to STATEMENT]
SET @@binlog_format = 'STATEMENT';
include/assert.inc [assert that @@SESSION.binlog_format is STATEMENT]
CREATE TEMPORARY TABLE t1 (a VARCHAR(100));
#
# Test switching binlog_format from STATEMENT to STATEMENT
# is disallowed when the session has open temporary table(s).
#
SET @@binlog_format = STATEMENT;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from STATEMENT to 'ROW'
# is disallowed when the session has open temporary table(s).
#
SET @@binlog_format = 'ROW';
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from STATEMENT to MIXED
# is disallowed when the session has open temporary table(s).
#
SET @@binlog_format = MIXED;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
# Set binlog_format to ROW
DROP TEMPORARY TABLE t1;
SET @@binlog_format = 'ROW';
include/assert.inc [assert that @@SESSION.binlog_format is set to ROW]
CREATE TEMPORARY TABLE t1 (a VARCHAR(100));
#
# Test switching binlog_format from ROW to MIXED
# is disallowed when the session has open temporary table(s).
#
SET @@binlog_format = MIXED;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from ROW to STATEMENT
# is disallowed when the session has open temporary table(s).
#
SET @@binlog_format = STATEMENT;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from ROW to ROW
# is disallowed when the session has open temporary table(s).
#
SET @@binlog_format = ROW;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
# Set binlog_format to MIXED
DROP TEMPORARY TABLE t1;
SET @@binlog_format = 'MIXED';
include/assert.inc [assert that @@SESSION.binlog_format is set to MIXED]
CREATE TEMPORARY TABLE t1 (a VARCHAR(100));
#
# Test switching binlog_format from MIXED to MIXED
# is disallowed when the session has open temporary table(s).
#
SET @@binlog_format = MIXED;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from MIXED to STATEMENT
# is disallowed when the session has open temporary table(s).
#
SET @@binlog_format = STATEMENT;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
#
# Test switching binlog_format from MIXED to ROW
# is disallowed when the session has open temporary table(s).
#
SET @@binlog_format = ROW;
ERROR HY000: Changing @@session.binlog_format is disallowed when the session has open temporary table(s). You could wait until these temporary table(s) are dropped and try again.
include/assert.inc [assert that @@SESSION.binlog_format is not changed]
# Reset binlog_format to STATEMENT
DROP TEMPORARY TABLE t1;
SET @@binlog_format = STATEMENT;
include/assert.inc [assert that @@SESSION.binlog_format is set to STATEMENT]
