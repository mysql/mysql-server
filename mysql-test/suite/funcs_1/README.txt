Matthias 17.06.2005
-------------------
1. I changed the database test1 (dropped + created in SP test)
   to test4.
   Please adjust the SP test cases.
2. There is a difference between my definition of
       innodb_tb4 + memory_tb4
   to the latest table definition used by disha.
   Please adjust the table definition if needed.
3. The data load files are product of the Disha data generation script
   (downloaded ~20 May ?) + modified by Omer
   These load data fit fairly to the table definitions.

4. How to execute the "small" test with 10 rows per table.
   Do NOT set the environment variable NO_REFRESH to a
   value <> ''.
   Start the test for example by
   ./mysql-test-run.pl --vardir=/dev/shm/var \
              --force --suite=funcs_1 --do-test=myisam
   The "result" files fit mostly to this variant.

   Any database not in ('mysql','test') and any tables
   needed within a testcase ( t/<storage engine>_<test filed>.test )
   will be (re)created at the beginning of the test.

5. How to execute the "big" test with many rows per table.
   Replace the directories
     suite/funcs_1/data   and
     suite/funcs_1/r
   with the appropriate ones for the "big" test.
   Set the environment variable NO_REFRESH to a value <> ''.
   Start the test for example by
   ./mysql-test-run.pl --vardir=/dev/shm/var \
              --force --suite=funcs_1 --do-test=myisam

   All databases and tables will be (re)created by the script
   <storage engine>__load.test  .

6. I am not sure of the files
   ./funcs_1/include/create_<whatever>.inc
   are in the moment needed. I included them, because I
   guess my VIEW testcase example needs them.

I guess the pushed files are far away from being perfect.
It is a 8 hours hack.
Please try them, create missing files and come up with improvements.

Good luck !

Matthias 17.06.2005
===================================================================
Omer 19.06.2005
---------------
1. Changed the structure of the memory_tb3 table to include two
   additional column f121, f122. These columns exist for the table in
   the other storage engines as TEXT. Since memory does not support
   TEXT, Disha did not include them. How ever I am using them in the
   Trigger tests so added them to the memory definition as CHAR(50);.
   Also modifyed the DataGen_modiy.pl file to account for these two
   column when generating the data.
   - checked in a new DataGen_modify.pl (create a 'lib' directory
     under 'funcs_1').
   - checked in a new memory_tb3.txt
2. Added three <storage>_triggers.test files based on Matthias's
   structure above.
3. Added three <storage>__triggers.result files
4. Added the Trigger_master.test file in the trigger dierctory
   Note: This is not complete and is still under work
5. Created a 'lib' directory and added the DataGen*.pl scripts to it
   (exists under the disha suite) but should be here as well).
Omer 19.06.2005
===================================================================
Matthias 12.09.2005
-------------------
   Replace the geometry data types by VARBINARY
   The removal of the geometry data types was necessary, because the
   execution of the funcs_1 testsuite should not depend on the
   availability of the geometry feature.
   Note: There are servers compiled without the geometry feature.

   The columns are not removed, but their data type was changed
   VARBINARY. This allows us to omit any changes within the loader
   input files or data generation scripts.
   The replacement of geometry by VARCHAR allows us to use our

Matthias 12.09.2005
===================================================================
Matthias 14.09.2005
-------------------
   The results of the <storage_engine>_views testcases suffer when
   executed in "--ps-protocol" mode from the open
   Bug#11589: mysqltest, --ps-protocol, strange output,
              float/double/real with zerofill  .
   Implementation of a workaround:
     At the beginning of views_master.inc is a variable $have_bug_11589 .
     If this varable is set to 1, the ps-protocol will be switched
     of for the critical statements.
Matthias 14.09.2005
===================================================================
Carsten 16.09.2005
------------------
1. The results of the datadict_<engine> testcases have been changed in nearly 
   all occurrencies of --error <n> because now for the INFORMATION_SCHEMA only 
   the --error 1044 (ERROR 42000: Access denied for user '..' to database 
   'information_schema') seems to be used.
2. To get identical results when using "--ps-protocol" some SELECTs FROM 
   information_schema has been wrapped to suppress using ps-protocol because 
   there are differences.
3. The test using SELECT .. OUTFILE has been disabled due to bug #13202.
4. Fixed datadict_<engine>.result files after the change that added 2 columns to 
   the VIEWS table (DEFINER varchar(77), SECURITY_TYPE varchar(7)).
===================================================================
Matthias 25.08.2007
-------------------
Data dictionary tests:
Fixes for Bugs 30418,30420,30438,30440 
1. Replace error numbers with error names
2. Replace static "InnoDB" (not all time available) used within an
   "alter table" by $OTHER_ENGINE_TYPE (set to MEMORY or MyISAM).
   Minor adjustment of column data type.
3. Use mysqltest result set sorting in several cases.
4. Avoid any statistics about help tables, because their content
   depends on configuration:
   developer release - help tables are empty
   build release     - help tables have content + growing with version
5. Add two help table related tests (one for build, one for developer)
   to ensure that informations about help tables within
   INFORMATION_SCHEMA.TABLES/STATISTICS are checked.
6. Note about new Bug#30689 at the beginning of the test.
   The files with expected results contain incomplete result sets.
7. Fix the NDB variant of the data dictionary test (ndb__datadict) as far as
   it was necessary for the bug fixes mentioned above.


General note:
   Most INFORMATION_SCHEMA properties (table layout, permissions etc.)
   are not affected by our variation of the storage engines except
   that some properties of our tables using a specific storage
   engine become visible. So it makes sense to decompose
   the data dictionary test into a storage engine specific part and
   a non storage engine specific part in future.
