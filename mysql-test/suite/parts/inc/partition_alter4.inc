################################################################################
# inc/partition_alter1.inc                                                     #
#                                                                              #
# Purpose:                                                                     #
#   Execute ALTER ... OPTIMIZE/CHECK/REBUID/ANALYZE statements (maintenance)   #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: HH                                                          #
# Original Date: 2006-07-27                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--echo
--echo #========================================================================
--echo #  1.1.1.12    ALTER TABLE <maintenance> PARTITION
--echo #========================================================================
--echo #------------------------------------------------------------------------
--echo #  1   ALTER ... ANALYZE PARTITION
--echo #------------------------------------------------------------------------
--echo #  1.1 ALTER ... ANALYZE PARTITION part_1;
let $alter= ALTER TABLE t1 ANALYZE PARTITION part_1;
--source suite/parts/inc/partition_alter_41.inc
--echo #  1.2 ALTER ... ANALYZE PARTITION part_1,part_2;
let $alter= ALTER TABLE t1 ANALYZE PARTITION part_1,part_2;
--source suite/parts/inc/partition_alter_41.inc
--echo #  1.3 ALTER ... ANALYZE PARTITION part_1,part_2,part_5,part_6,part_10;
let $alter= ALTER TABLE t1 ANALYZE PARTITION part_1,part_2,part_5,part_6,part_10;
--source suite/parts/inc/partition_alter_41.inc
--echo #  1.4 ALTER ... ANALYZE PARTITION part_1,part_1,part_1;
let $alter= ALTER TABLE t1 ANALYZE PARTITION part_1,part_1,part_1;
--source suite/parts/inc/partition_alter_41.inc
--echo #  1.5 ALTER ... ANALYZE PARTITION ALL;
let $alter= ALTER TABLE t1 ANALYZE PARTITION ALL;
--source suite/parts/inc/partition_alter_41.inc

--echo #------------------------------------------------------------------------
--echo #  2   ALTER ... CHECK PARTITION
--echo #------------------------------------------------------------------------
--echo #  2.1 ALTER ... CHECK PARTITION part_1;
let $alter= ALTER TABLE t1 CHECK PARTITION part_1;
--source suite/parts/inc/partition_alter_41.inc
--echo #  2.2 ALTER ... CHECK PARTITION part_1,part_2;
let $alter= ALTER TABLE t1 CHECK PARTITION part_1,part_2;
--source suite/parts/inc/partition_alter_41.inc
--echo #  2.3 ALTER ... CHECK PARTITION part_1,part_2,part_5,part_6,part_10;
let $alter= ALTER TABLE t1 CHECK PARTITION part_1,part_2,part_5,part_6,part_10;
--source suite/parts/inc/partition_alter_41.inc
--echo #  2.4 ALTER ... CHECK PARTITION part_1,part_1,part_1;
let $alter= ALTER TABLE t1 CHECK PARTITION part_1,part_1,part_1;
--source suite/parts/inc/partition_alter_41.inc
--echo #  2.5 ALTER ... CHECK PARTITION ALL;
let $alter= ALTER TABLE t1 CHECK PARTITION ALL;
--source suite/parts/inc/partition_alter_41.inc

--echo #------------------------------------------------------------------------
--echo #  3   ALTER ... OPTIMIZE PARTITION
--echo #------------------------------------------------------------------------
--echo #  3.1 ALTER ... OPTIMIZE PARTITION part_1;
let $alter= ALTER TABLE t1 OPTIMIZE PARTITION part_1;
--source suite/parts/inc/partition_alter_41.inc
--echo #  3.2 ALTER ... OPTIMIZE PARTITION part_1,part_2;
let $alter= ALTER TABLE t1 OPTIMIZE PARTITION part_1,part_2;
--source suite/parts/inc/partition_alter_41.inc
--echo #  3.3 ALTER ... OPTIMIZE PARTITION part_1,part_2,part_5,part_6,part_10;
let $alter= ALTER TABLE t1 OPTIMIZE PARTITION part_1,part_2,part_5,part_6,part_10;
--source suite/parts/inc/partition_alter_41.inc
--echo #  3.4 ALTER ... OPTIMIZE PARTITION part_1,part_1,part_1;
let $alter= ALTER TABLE t1 OPTIMIZE PARTITION part_1,part_1,part_1;
--source suite/parts/inc/partition_alter_41.inc
--echo #  3.5 ALTER ... OPTIMIZE PARTITION ALL;
let $alter= ALTER TABLE t1 OPTIMIZE PARTITION ALL;
--source suite/parts/inc/partition_alter_41.inc

--echo #------------------------------------------------------------------------
--echo #  4   ALTER ... REBUILD PARTITION
--echo #------------------------------------------------------------------------
--echo #  4.1 ALTER ... REBUILD PARTITION part_1;
let $alter= ALTER TABLE t1 REBUILD PARTITION part_1;
--source suite/parts/inc/partition_alter_41.inc
--echo #  4.2 ALTER ... REBUILD PARTITION part_1,part_2;
let $alter= ALTER TABLE t1 REBUILD PARTITION part_1,part_2;
--source suite/parts/inc/partition_alter_41.inc
--echo #  4.3 ALTER ... REBUILD PARTITION part_1,part_2,part_5,part_6,part_10;
let $alter= ALTER TABLE t1 REBUILD PARTITION part_1,part_2,part_5,part_6,part_10;
--source suite/parts/inc/partition_alter_41.inc
--echo #  4.4 ALTER ... REBUILD PARTITION part_1,part_1,part_1;
let $alter= ALTER TABLE t1 REBUILD PARTITION part_1,part_1,part_1;
--source suite/parts/inc/partition_alter_41.inc
--echo #  4.5 ALTER ... REBUILD PARTITION ALL;
let $alter= ALTER TABLE t1 REBUILD PARTITION ALL;
--source suite/parts/inc/partition_alter_41.inc

--echo #------------------------------------------------------------------------
--echo #  5   ALTER ... REPAIR PARTITION
--echo #------------------------------------------------------------------------
--echo #  5.1 ALTER ... REPAIR PARTITION part_1;
let $alter= ALTER TABLE t1 REPAIR PARTITION part_1;
--source suite/parts/inc/partition_alter_41.inc
--echo #  5.2 ALTER ... REPAIR PARTITION part_1,part_2;
let $alter= ALTER TABLE t1 REPAIR PARTITION part_1,part_2;
--source suite/parts/inc/partition_alter_41.inc
--echo #  5.3 ALTER ... REPAIR PARTITION part_1,part_2,part_5,part_6,part_10;
let $alter= ALTER TABLE t1 REPAIR PARTITION part_1,part_2,part_5,part_6,part_10;
--source suite/parts/inc/partition_alter_41.inc
--echo #  5.4 ALTER ... REPAIR PARTITION part_1,part_1,part_1;
let $alter= ALTER TABLE t1 REPAIR PARTITION part_1,part_1,part_1;
--source suite/parts/inc/partition_alter_41.inc
--echo #  5.5 ALTER ... REPAIR PARTITION ALL;
let $alter= ALTER TABLE t1 REPAIR PARTITION ALL;
--source suite/parts/inc/partition_alter_41.inc

--echo #------------------------------------------------------------------------
--echo #  6   ALTER ... REMOVE PARTITIONING
--echo #------------------------------------------------------------------------
--echo #  6.1 ALTER ... REMOVE PARTITIONING;
let $alter= ALTER TABLE t1 REMOVE PARTITIONING;
--source suite/parts/inc/partition_alter_41.inc

