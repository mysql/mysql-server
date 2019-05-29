--echo #
--echo # Bug #29813582 INSUFFICIENT BUFFER SIZE TO HOLD MAX LENGTH NAME OF
--echo # PARTITIONED TABLE
--echo #

--echo # A 65 character table or (sub) partition name should be rejected.
#####################################################################################
#                            1         2         3         4         5         6
#                   12345678901234567890123456789012345678901234567890123456789012345
let too_long_name = �����������������������������������������������������������������;
--echo # Too long table name.
--error ER_WRONG_TABLE_NAME
eval CREATE TABLE $too_long_name (i INTEGER)
 PARTITION BY LIST(MOD(i, 2))
 SUBPARTITION BY KEY(i)
 (PARTITION p1 VALUES IN (-1) (SUBPARTITION sp1, SUBPARTITION sp2),
  PARTITION p2 VALUES IN (0)  (SUBPARTITION sp3, SUBPARTITION sp5),
  PARTITION p3 VALUES IN (1)  (SUBPARTITION sp4, SUBPARTITION sp6));

--echo # Too long partition name.
--error ER_TOO_LONG_IDENT
eval CREATE TABLE t1 (i INTEGER)
 PARTITION BY LIST(MOD(i, 2))
 SUBPARTITION BY KEY(i)
 (PARTITION $too_long_name VALUES IN (-1) (SUBPARTITION sp1, SUBPARTITION sp2),
  PARTITION p2 VALUES IN (0)  (SUBPARTITION sp3, SUBPARTITION sp5),
  PARTITION p3 VALUES IN (1)  (SUBPARTITION sp4, SUBPARTITION sp6));

--echo # Too long subpartition name.
--error ER_TOO_LONG_IDENT
eval CREATE TABLE t1 (i INTEGER)
 PARTITION BY LIST(MOD(i, 2))
 SUBPARTITION BY KEY(i)
 (PARTITION p1 VALUES IN (-1) (SUBPARTITION sp1, SUBPARTITION $too_long_name),
  PARTITION p2 VALUES IN (0)  (SUBPARTITION sp3, SUBPARTITION sp5),
  PARTITION p3 VALUES IN (1)  (SUBPARTITION sp4, SUBPARTITION sp6));

--echo # A 64 character table or (sub) partition name should be accepted.
##################################################################################
#                          1         2         3         4         5         6
#                 1234567890123456789012345678901234567890123456789012345678901234
let long_name_1 = ����������������������������������������������������������������;
let long_name_2 = ����������������������������������������������������������������;
let long_name_3 = ���������������������������������������������������������������å;
--echo # Table, partition and subpartition names at 64 characters.
--error ER_PATH_LENGTH
eval CREATE TABLE $long_name_1 (i INTEGER)
 PARTITION BY LIST(MOD(i, 2))
 SUBPARTITION BY KEY(i)
 (PARTITION $long_name_2 VALUES IN (-1) (SUBPARTITION $long_name_3, SUBPARTITION sp2),
  PARTITION p2 VALUES IN (0) (SUBPARTITION sp3, SUBPARTITION sp5),
  PARTITION p3 VALUES IN (1) (SUBPARTITION sp4, SUBPARTITION sp6));
