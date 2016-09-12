#
# Bug #23645320 INNODB: ASSERTION FAILURE: UT0FITBLK.CC:319:RET == Z_STREAM_END
#
CREATE TABLE t1 ( f1 longblob ) engine=innodb row_format=compressed;
#
# Inserting std_data/lob/earth215kb.jpg.
#
drop table t1;
