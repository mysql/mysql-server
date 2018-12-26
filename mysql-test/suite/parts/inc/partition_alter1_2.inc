################################################################################
# inc/partition_alter1_2.inc                                                     #
#                                                                              #
# Purpose:                                                                     #
#   ADD/DROP PRIMARY KEYs and/or UNIQUE INDEXes tests on partitioned tables    #
#   This routine is only useful for the partition_<feature>_<engine> tests.    #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
# Change Author: debanerj                                                      #
# Change Date: 2015-02-18                                                      #
# Change: Added tests for alter table drop + add primary/unique key            #
################################################################################
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

if ($do_pk_tests)
{
--echo
--echo #========================================================================
--echo #  3.    ALTER TABLE "ALTER"(DROP + ADD) PRIMARY KEY
--echo #        mleich: I think that an ALTER TABLE statement where a PRIMARY
--echo #            KEY is dropped and recreated (with different layout) might
--echo #            be of interest, if the tree containing the table data has
--echo #            to be reorganized during this operation.
--echo #========================================================================
#         Rule: The table must have a PRIMARY KEY or UNIQUE KEY
#               ---> $unique must not be empty
#               ---> The KEY to be dropped must contain the columns
#                    used for partitioning.

# Override column list to add NOT NULL constraint for partition by key()
# tests. May be it is better to add more columns in partition.pre but would
# impact a lot more tests.

let $column_list= f_int1 INTEGER NOT NULL,
f_int2 INTEGER NOT NULL,
f_char1 CHAR(20),
f_char2 CHAR(20),
f_charbig VARCHAR(1000);

let $expect_inplace_error= 0;

--echo #------------------------------------------------------------------------
--echo #  3.1   Partitioning function uses the PRIMARY/UNIQUE KEY
--echo #------------------------------------------------------------------------
#         Rule: PRIMARY/UNIQUE KEY is used in partitioning function
#         ---> inc/partition_alter_12.inc

   --echo #  3.1.1 DROP and CREATE PRIMARY KEY on different column
   let $alter= ALTER TABLE t1 DROP PRIMARY KEY, ADD PRIMARY KEY(f_int2, f_int1);
   let $unique= , PRIMARY KEY(f_int1);
   --source suite/parts/inc/partition_alter_12.inc

   --echo #  3.1.2 Negative test: Try DROP and CREATE PRIMARY KEY INPLACE
   --echo # Expect error for INPLACE ALTER
   let $expect_inplace_error= 1;
   let $alter= ALTER TABLE t1 DROP PRIMARY KEY, ADD PRIMARY KEY(f_int2, f_int1),
               ALGORITHM = INPLACE;
   let $unique= , PRIMARY KEY(f_int1);
   --source suite/parts/inc/partition_alter_12.inc
   let $expect_inplace_error= 0;

   --echo #  3.1.3 DROP and CREATE UNIQUE KEY on different column
   let $alter= ALTER TABLE t1 DROP KEY ukey1, ADD UNIQUE KEY
               ukey2(f_int2, f_int1);
   let $unique= , UNIQUE KEY ukey1(f_int1);
   --source suite/parts/inc/partition_alter_12.inc

   --echo #  3.1.4 Negative test: Try DROP and CREATE UNIQUE KEY INPLACE
   --echo # Expect error for INPLACE ALTER
   let $expect_inplace_error= 1;
   let $alter= ALTER TABLE t1 DROP KEY ukey1, ADD UNIQUE KEY
               ukey2(f_int2, f_int1), ALGORITHM = INPLACE;
   let $unique= , UNIQUE KEY ukey1(f_int1);
   --source suite/parts/inc/partition_alter_12.inc
   let $expect_inplace_error= 0;
}

