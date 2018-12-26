################################################################################
# inc/partition_alter1_1.inc                                                     #
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
--echo #------------------------------------------------------------------------
--echo #  1.1   ADD PRIMARY KEY or UNIQUE INDEX to table with one column (f_int1)
--echo #        within the partitioning function
--echo #------------------------------------------------------------------------
#         Rule: Only f_int1 is used within the partitioning function
#         ---> inc/partition_alter_11.inc
if ($do_pk_tests)
{
   # The value of the following test is maybe covered by 1.1.3.
   if ($more_pk_ui_tests)
   {
      --echo #  1.1.1 PRIMARY KEY consisting of one column
      let $alter= ALTER TABLE t1 ADD PRIMARY KEY(f_int1);
      --source suite/parts/inc/partition_alter_11.inc
   }
   # This must fail, because PRIMARY KEY does not contain f_int1
   let $alter= ALTER TABLE t1 ADD PRIMARY KEY(f_int2);
   --source suite/parts/inc/partition_alter_11.inc
}
# The value of the following test is maybe covered by 1.1.4.
if ($more_pk_ui_tests)
{
   --echo #  1.1.2 UNIQUE INDEX consisting of one column
   let $alter= ALTER TABLE t1 ADD UNIQUE INDEX uidx1 (f_int1);
   --source suite/parts/inc/partition_alter_11.inc
}
# This must fail, because UNIQUE INDEX does not contain f_int1
let $alter= ALTER TABLE t1 ADD UNIQUE INDEX uidx1 (f_int2);
--source suite/parts/inc/partition_alter_11.inc
if ($do_pk_tests)
{
   --echo #  1.1.3 PRIMARY KEY consisting of two columns
   let $alter= ALTER TABLE t1 ADD PRIMARY KEY(f_int1,f_int2);
   --source suite/parts/inc/partition_alter_11.inc
   let $alter= ALTER TABLE t1 ADD PRIMARY KEY(f_int2,f_int1);
   --source suite/parts/inc/partition_alter_11.inc
}
--echo #  1.1.4 UNIQUE INDEX consisting of two columns
let $alter= ALTER TABLE t1 ADD UNIQUE INDEX uidx1 (f_int1,f_int2);
--source suite/parts/inc/partition_alter_11.inc
let $alter= ALTER TABLE t1 ADD UNIQUE INDEX uidx1 (f_int2,f_int1);
--source suite/parts/inc/partition_alter_11.inc


