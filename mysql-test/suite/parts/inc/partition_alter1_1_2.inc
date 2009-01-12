################################################################################
# inc/partition_alter1_1_2.inc                                                     #
#                                                                              #
# Purpose:                                                                     #
#   ADD/DROP PRIMARY KEYs and/or UNIQUE INDEXes tests on partitioned tables    #
#   This routine is only useful for the partition_<feature>_<engine> tests.    #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--echo
--echo #========================================================================
--echo #  1.    ALTER TABLE ADD PRIMARY KEY and/or UNIQUE INDEX
--echo #========================================================================
#         Rule: The table does not have a PRIMARY KEY or UNIQUE INDEX.
#               ---> $unique must be empty
#               ---> The PRIMARY KEY or UNIQUE INDEX to be created must contain
#                     the columns used for partitioning.
#
--echo #------------------------------------------------------------------------
--echo #  1.2   ADD PRIMARY KEY or UNIQUE INDEX to table with two columns
--echo #        (f_int1 and f_int2) within the partitioning function
--echo #------------------------------------------------------------------------
#         Rule: f_int1 and f_int2 is used within the partitioning function
#         ---> inc/partition_alter_13.inc
if ($do_pk_tests)
{
   --echo #  1.2.1 PRIMARY KEY consisting of two columns
   let $alter= ALTER TABLE t1 ADD PRIMARY KEY(f_int1,f_int2);
   --source suite/parts/inc/partition_alter_13.inc
   let $alter= ALTER TABLE t1 ADD PRIMARY KEY(f_int2,f_int1);
   --source suite/parts/inc/partition_alter_13.inc
}
--echo #  1.2.2 UNIQUE INDEX consisting of two columns
let $alter= ALTER TABLE t1 ADD UNIQUE INDEX uidx1 (f_int1,f_int2);
--source suite/parts/inc/partition_alter_13.inc
let $alter= ALTER TABLE t1 ADD UNIQUE INDEX uidx1 (f_int2,f_int1);
--source suite/parts/inc/partition_alter_13.inc
if ($do_pk_tests)
{
   --echo #  1.2.3 PRIMARY KEY and UNIQUE INDEX consisting of two columns
   let $alter= ALTER TABLE t1 ADD UNIQUE INDEX uidx1 (f_int1,f_int2), ADD PRIMARY KEY(f_int2,f_int1);
   --source suite/parts/inc/partition_alter_13.inc
   let $alter= ALTER TABLE t1 ADD UNIQUE INDEX uidx1 (f_int2,f_int1), ADD PRIMARY KEY(f_int1,f_int2);
   --source suite/parts/inc/partition_alter_13.inc
   let $unique= ;
   --source suite/parts/inc/partition_alter_13.inc
}

