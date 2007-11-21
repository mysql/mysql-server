################################################################################
# inc/partition_alter1.inc                                                     #
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
#
#
--echo
--echo #========================================================================
--echo #  2     DROP PRIMARY KEY or UNIQUE INDEX
--echo #========================================================================
#         Rule: The table must have a PRIMARY KEY or UNIQUE INDEX.
#               ---> $unique must not be empty
#               ---> The PRIMARY KEY or UNIQUE INDEX to be dropped must contain
#                    the columns used for partitioning.
--echo #------------------------------------------------------------------------
--echo #  2.1   Partitioning function contains one column(f_int1)
--echo #------------------------------------------------------------------------
#         Rule: Only f_int1 is used within the partitioning function
#         ---> inc/partition_alter_11.inc
# The value of the following test is maybe covered by 2.1.5.
if ($more_pk_ui_tests)
{
   if ($do_pk_tests)
   {
      --echo #  2.1.1 DROP PRIMARY KEY consisting of one column
      let $unique= , PRIMARY KEY(f_int1);
      let $alter= ALTER TABLE t1 DROP PRIMARY KEY;
      --source suite/parts/inc/partition_alter_11.inc
   }
   #
   --echo #  2.1.2 DROP UNIQUE INDEX consisting of one column
   let $unique= , UNIQUE INDEX uidx1 (f_int1);
   let $alter= ALTER TABLE t1 DROP INDEX uidx1;
   --source suite/parts/inc/partition_alter_11.inc
   #
   if ($do_pk_tests)
   {
      --echo #  2.1.3 DROP PRIMARY KEY consisting of two columns
      let $alter= ALTER TABLE t1 DROP PRIMARY KEY;
      let $unique= , PRIMARY KEY(f_int1,f_int2);
      --source suite/parts/inc/partition_alter_11.inc
      let $unique= , PRIMARY KEY(f_int2,f_int1);
      --source suite/parts/inc/partition_alter_11.inc
   }
   #
   --echo #  2.1.4 DROP UNIQUE INDEX consisting of two columns
   let $alter= ALTER TABLE t1 DROP INDEX uidx1;
   let $unique= , UNIQUE INDEX uidx1 (f_int1,f_int2);
   --source suite/parts/inc/partition_alter_11.inc
   let $unique= , UNIQUE INDEX uidx1 (f_int2,f_int1);
   --source suite/parts/inc/partition_alter_11.inc
   }
#
if ($do_pk_tests)
{
   --echo #  2.1.5 DROP PRIMARY KEY + UNIQUE INDEX consisting of two columns
   let $unique= , UNIQUE INDEX uidx1 (f_int1,f_int2), PRIMARY KEY(f_int2,f_int1);
   let $alter= ALTER TABLE t1 DROP PRIMARY KEY, DROP INDEX uidx1;
   --source suite/parts/inc/partition_alter_11.inc
   let $unique= , UNIQUE INDEX uidx1 (f_int2,f_int1), PRIMARY KEY(f_int1,f_int2);
   let $alter= ALTER TABLE t1 DROP PRIMARY KEY, DROP INDEX uidx1;
   --source suite/parts/inc/partition_alter_11.inc
}
let $unique= , UNIQUE INDEX uidx1 (f_int1,f_int2), UNIQUE INDEX uidx2 (f_int2,f_int1);
let $alter= ALTER TABLE t1 DROP INDEX uidx1, DROP INDEX uidx2;
--source suite/parts/inc/partition_alter_11.inc
#
--echo #------------------------------------------------------------------------
--echo #  2.2   Partitioning function contains two columns (f_int1,f_int2)
--echo #------------------------------------------------------------------------
#         Rule: f_int1 and f_int2 is used within the partitioning function
#         ---> inc/partition_alter_13.inc
if ($do_pk_tests)
{
   --echo #  2.2.1 DROP PRIMARY KEY consisting of two columns
   let $alter= ALTER TABLE t1 DROP PRIMARY KEY;
   let $unique= , PRIMARY KEY(f_int1,f_int2);
   --source suite/parts/inc/partition_alter_13.inc
   let $unique= , PRIMARY KEY(f_int2,f_int1);
   --source suite/parts/inc/partition_alter_13.inc
}
#
--echo #  2.2.2 DROP UNIQUE INDEX consisting of two columns
let $alter= ALTER TABLE t1 DROP INDEX uidx1;
let $unique= , UNIQUE INDEX uidx1 (f_int1,f_int2);
--source suite/parts/inc/partition_alter_13.inc
let $unique= , UNIQUE INDEX uidx1 (f_int2,f_int1);
--source suite/parts/inc/partition_alter_13.inc
#
if ($do_pk_tests)
{
   --echo #  2.2.3 DROP PRIMARY KEY + UNIQUE INDEX consisting of two columns
   let $unique= , UNIQUE INDEX uidx1 (f_int1,f_int2), PRIMARY KEY(f_int2,f_int1);
   let $alter= ALTER TABLE t1 DROP PRIMARY KEY, DROP INDEX uidx1;
   --source suite/parts/inc/partition_alter_13.inc
   let $unique= , UNIQUE INDEX uidx1 (f_int2,f_int1), PRIMARY KEY(f_int1,f_int2);
   let $alter= ALTER TABLE t1 DROP PRIMARY KEY, DROP INDEX uidx1;
   --source suite/parts/inc/partition_alter_13.inc
}
let $unique= , UNIQUE INDEX uidx1 (f_int1,f_int2), UNIQUE INDEX uidx2 (f_int2,f_int1);
let $alter= ALTER TABLE t1 DROP INDEX uidx1, DROP INDEX uidx2;
--source suite/parts/inc/partition_alter_13.inc

if (0)
{
--echo
--echo #========================================================================
--echo #  3.    ALTER TABLE "ALTER" PRIMARY KEY
--echo #        mleich: I think that an ALTER TABLE statement where a PRIMARY
--echo #            KEY is dropped and recreated (with different layout) might
--echo #            be of interest, if the tree containing the table data has
--echo #            to be reorganized during this operation.
--echo #        To be implemented
--echo #========================================================================
--echo
}
