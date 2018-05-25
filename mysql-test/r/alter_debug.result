#
# BUG#27899274 - [MYSQL 8.0 GA DEBUG BUILD] ASSERTION `!IS_SET()' FAILED.
#
#
CREATE TABLE test.t1(col INT);
SET SESSION debug= "+d, enable_stack_overrun_post_alter_commit";
ALTER TABLE test.t1 ADD COLUMN col1 CHAR;
Got one of the listed errors
SET SESSION debug= "-d, enable_stack_overrun_post_alter_commit";
DROP TABLE test.t1;
