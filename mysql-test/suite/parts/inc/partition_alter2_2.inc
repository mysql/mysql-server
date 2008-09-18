################################################################################
# inc/partition_alter2_2.inc                                                   #
#                                                                              #
# Purpose:                                                                     #
#   Tests where the columns used within the partitioning function are altered. #
#   This routine is only useful for the partition_<feature>_<engine> tests.   .#
#   Part 2: decreasing size of column
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
# Change Author: MattiasJ                                                      #
# Change Date: 2008-09-08                                                      #
# Change: Splitted the test in two different parts (increasing/decreasing col) #
################################################################################

--echo
--echo #========================================================================
--echo #  2     Decrease the size of the column used in the partitioning
--echo #        function and/or PRIMARY KEY and/or UNIQUE INDEX
--echo #========================================================================
--echo #------------------------------------------------------------------------
--echo #  2.1   ALTER column f_int2 not used in partitioning function
--echo #------------------------------------------------------------------------
#         Rule: Only f_int1 is used within the partitioning function
#         ---> inc/partition_alter_11.inc
let $alter= ALTER TABLE t1 MODIFY f_int2 MEDIUMINT;
--echo #  2.1.1 no PRIMARY KEY or UNIQUE INDEX exists
let $unique= ;
--source suite/parts/inc/partition_alter_11.inc
#
if ($do_pk_tests)
{
   # The value of the direct following test is maybe covered by the test with
   # the PRIMARY KEY containing two columns.
   if ($more_pk_ui_tests)
   {
      --echo #  2.1.2 PRIMARY KEY exists
      let $unique= , PRIMARY KEY (f_int1);
      --source suite/parts/inc/partition_alter_11.inc
   }
   let $unique= , PRIMARY KEY (f_int1,f_int2);
   --source suite/parts/inc/partition_alter_11.inc
   let $unique= , PRIMARY KEY (f_int2,f_int1);
   --source suite/parts/inc/partition_alter_11.inc
}
#
--echo #  2.1.3 UNIQUE INDEX exists
# The value of the direct following test is maybe covered by the test with
# the UNIQUE INDEX containing two columns.
if ($more_pk_ui_tests)
{
   let $unique= , UNIQUE INDEX uidx1 (f_int1);
   --source suite/parts/inc/partition_alter_11.inc
}
let $unique= , UNIQUE INDEX uidx1 (f_int1,f_int2);
--source suite/parts/inc/partition_alter_11.inc
let $unique= , UNIQUE INDEX uidx1 (f_int2,f_int1);
--source suite/parts/inc/partition_alter_11.inc
#
if ($more_pk_ui_tests)
{
   # The value of the tests 2.2 is maybe covered by the tests 2.3
   --echo #------------------------------------------------------------------------
   --echo #  2.2   ALTER column f_int1 used in partitioning function
   --echo #------------------------------------------------------------------------
   #         Rule: Only f_int1 is used within the partitioning function
   #         ---> inc/partition_alter_11.inc
   let $alter= ALTER TABLE t1 MODIFY f_int1 MEDIUMINT;
   --echo #  2.2.1 no PRIMARY KEY or UNIQUE INDEX exists
   let $unique= ;
   --source suite/parts/inc/partition_alter_11.inc
   --source suite/parts/inc/partition_alter_13.inc
   #
   if ($do_pk_tests)
   {
      --echo #  2.2.2 PRIMARY KEY exists
      let $unique= , PRIMARY KEY (f_int1);
      --source suite/parts/inc/partition_alter_11.inc
      let $unique= , PRIMARY KEY (f_int1,f_int2);
      --source suite/parts/inc/partition_alter_11.inc
      --source suite/parts/inc/partition_alter_13.inc
      let $unique= , PRIMARY KEY (f_int2,f_int1);
      --source suite/parts/inc/partition_alter_11.inc
      --source suite/parts/inc/partition_alter_13.inc
   }
   #
   --echo #  2.2.3 UNIQUE INDEX exists
   let $unique= , UNIQUE INDEX uidx (f_int1);
   --source suite/parts/inc/partition_alter_11.inc
   let $unique= , UNIQUE INDEX uidx (f_int1,f_int2);
   --source suite/parts/inc/partition_alter_11.inc
   --source suite/parts/inc/partition_alter_13.inc
   let $unique= , UNIQUE INDEX uidx (f_int2,f_int1);
   --source suite/parts/inc/partition_alter_11.inc
   --source suite/parts/inc/partition_alter_13.inc
}
#
--echo #------------------------------------------------------------------------
--echo #  2.3   ALTER column f_int1 and f_int2 used in partitioning function
--echo #------------------------------------------------------------------------
#         Rule: f_int1 and f_int2 is used within the partitioning function
#         ---> inc/partition_alter_13.inc
let $alter= ALTER TABLE t1 MODIFY f_int1 MEDIUMINT, MODIFY f_int2 MEDIUMINT;
--echo #  2.3.1 no PRIMARY KEY or UNIQUE INDEX exists
let $unique= ;
--source suite/parts/inc/partition_alter_11.inc
--source suite/parts/inc/partition_alter_13.inc
#
if ($do_pk_tests)
{
   --echo #  2.3.2 PRIMARY KEY exists
   # The value of the direct following test is maybe covered by the test with
   # the PRIMARY KEY containing two columns.
   if ($more_pk_ui_tests)
   {
      let $unique= , PRIMARY KEY (f_int1);
      --source suite/parts/inc/partition_alter_11.inc
   }
   let $unique= , PRIMARY KEY (f_int1,f_int2);
   --source suite/parts/inc/partition_alter_11.inc
   --source suite/parts/inc/partition_alter_13.inc
   let $unique= , PRIMARY KEY (f_int2,f_int1);
   --source suite/parts/inc/partition_alter_11.inc
   --source suite/parts/inc/partition_alter_13.inc
}
#
--echo #  2.3.3 UNIQUE INDEX exists
# The value of the direct following test is maybe covered by the test with
# the UNIQUE INDEX containing two columns.
if ($more_pk_ui_tests)
{
   let $unique= , UNIQUE INDEX uidx (f_int1);
   --source suite/parts/inc/partition_alter_11.inc
}
let $unique= , UNIQUE INDEX uidx (f_int1,f_int2);
--source suite/parts/inc/partition_alter_11.inc
--source suite/parts/inc/partition_alter_13.inc
let $unique= , UNIQUE INDEX uidx (f_int2,f_int1);
--source suite/parts/inc/partition_alter_11.inc
--source suite/parts/inc/partition_alter_13.inc
#

if (0)
{
--echo
--echo #========================================================================
--echo #  3     ALTER the type of the column used in the partitioning
--echo #        function and/or PRIMARY KEY and/or UNIQUE INDEX
--echo #                   INTEGER --> FLOAT
--echo #                   INTEGER --> DECIMAL
--echo #                   INTEGER --> VARCHAR
--echo #        mleich: I assume that at least the first two variants are of
--echo #            some interest. But I am unsure if the server allows such
--echo #            conversions. I also think that such operations have a
--echo #            conversions very small likelihood.
--echo #            To be implemented.
--echo #========================================================================
}
