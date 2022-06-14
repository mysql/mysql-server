#
# Bug #29813582 INSUFFICIENT BUFFER SIZE TO HOLD MAX LENGTH NAME OF
# PARTITIONED TABLE
#
# A 65 character table or (sub) partition name should be rejected.
# Too long table name.
CREATE TABLE ����������������������������������������������������������������� (i INTEGER)
PARTITION BY LIST(MOD(i, 2))
SUBPARTITION BY KEY(i)
(PARTITION p1 VALUES IN (-1) (SUBPARTITION sp1, SUBPARTITION sp2),
PARTITION p2 VALUES IN (0)  (SUBPARTITION sp3, SUBPARTITION sp5),
PARTITION p3 VALUES IN (1)  (SUBPARTITION sp4, SUBPARTITION sp6));
ERROR 42000: Incorrect table name '���������������������������������?'
# Too long partition name.
CREATE TABLE t1 (i INTEGER)
PARTITION BY LIST(MOD(i, 2))
SUBPARTITION BY KEY(i)
(PARTITION ����������������������������������������������������������������� VALUES IN (-1) (SUBPARTITION sp1, SUBPARTITION sp2),
PARTITION p2 VALUES IN (0)  (SUBPARTITION sp3, SUBPARTITION sp5),
PARTITION p3 VALUES IN (1)  (SUBPARTITION sp4, SUBPARTITION sp6));
ERROR 42000: Identifier name '���������������������������������?' is too long
# Too long subpartition name.
CREATE TABLE t1 (i INTEGER)
PARTITION BY LIST(MOD(i, 2))
SUBPARTITION BY KEY(i)
(PARTITION p1 VALUES IN (-1) (SUBPARTITION sp1, SUBPARTITION �����������������������������������������������������������������),
PARTITION p2 VALUES IN (0)  (SUBPARTITION sp3, SUBPARTITION sp5),
PARTITION p3 VALUES IN (1)  (SUBPARTITION sp4, SUBPARTITION sp6));
ERROR 42000: Identifier name '���������������������������������?' is too long
# A 64 character table or (sub) partition name should be accepted.
# Table, partition and subpartition names at 64 characters.
CREATE TABLE ���������������������������������������������������������������� (i INTEGER)
PARTITION BY LIST(MOD(i, 2))
SUBPARTITION BY KEY(i)
(PARTITION ���������������������������������������������������������������� VALUES IN (-1) (SUBPARTITION ���������������������������������������������������������������å, SUBPARTITION sp2),
PARTITION p2 VALUES IN (0) (SUBPARTITION sp3, SUBPARTITION sp5),
PARTITION p3 VALUES IN (1) (SUBPARTITION sp4, SUBPARTITION sp6));
ERROR HY000: The path specified for test/@fffd@fffd@fffd@fffd@fffd@fffd@fffd@fffd@fffd@fffd@fffd@fff is too long.
