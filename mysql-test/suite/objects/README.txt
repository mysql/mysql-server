Objects test suite
~~~~~~~~~~~~~~~~~~
Create 49166 objects in a database and drop the database afterwards.

1 Quick overview
2 Detailed overview
3 Further testing scenarios
4 Detailed run times

1 Quick overview
~~~~~~~~~~~~~~~~
1.1 What is tested?
A database with 49166 objects (tables and views) is created. The
database is dropped afterwards. Only DDL statements are used. No
data is loaded at all.

1.2 Steps of the test
- CREATE SCHEMA.
- Populate 49166 objects.
- Check the objects with SHOW TABLES.
- Check the output of one SHOW CREATE TABLE.
- DROP SCHEMA.

1.3 How to run the test
Run objects test:
./mysql-test-run.pl --skip-ndb --suite=objects

If one experiences time out problems, please use
  --testcase-timeout=3600 --suite-timeout=3600

Run objects test with Falcon storage engine only:
- Run time. No run time due to crash.
- Space requirements ~1.3 GB.
./mysql-test-run.pl --skip-ndb --suite=objects --do-test=objects_falcon

To avoid time out on Falcon storage engine use
./mysql-test-run.pl --skip-ndb --testcase-timeout=3600 --suite-timeout=3600 \
--suite=objects --do-test=objects_falcon

Run objects test with InnoDB storage engine only.
- Run time ~100 minutes.
- Space requirements ~1.5 GB.
./mysql-test-run.pl --skip-ndb --suite=objects --do-test=objects_innodb


2 Detailed overview
~~~~~~~~~~~~~~~~~~~
2.1 Background of this test
The motivation of this test is rooted in the SAP porting project. To
run SAP specific tests one has to create a small database (~17000 tables
and views) or depending on the tests to be run a big database (~50000
tables and views).
   The SAP porting team tested various storage engines and found out
that prior to running the SAP tests the loading of the required data
often lead to problems and crashes in the database server.
   As the initial data loading for a SAP test is around 5 GB the DDL
part of the data loading was stripped out. 
   A successful run of this test can be followed by a R3load test (data
loading with a SAP tool), to see whether the storage engine can handle
the rather huge amount of data and to measure the performance while
loading the data.

2.2 Usage in a given bk tree
If one wants to test in a give bk tree, lets say 5.1-falcon, one can
either copy or soft link this suite into the mysql-test/suite/
folder. We recommend a soft link:
  ln -s <path_to>/mysql-test-extra-5.1/mysql-test/suite/objects \
  <path_to>/mysql-5.1-falcon/mysql-test/suite

2.3 What kind of objects?
Although, the number of 49166 objects sounds huge only tables and
views are created. Please note, that this test does not create:
- Stored procedures.
- Foreign keys.
- Triggers.
- Events.
   This means, that this test suite has a potential for improvement
by adding further objects like stored procedures, foreign keys,
triggers, or events.


3 Further testing scenarios
~~~~~~~~~~~~~~~~~~~~~~~~~~~
3.1 Testing the functionality and performance of our INFORMATION_SCHEMA
Tests on INFORMATION_SCHEMA can be done by disabling the DROP SCHEMA at
the end of the test by setting the variable $drop_database to 0 (zero):
  let $drop_database= 0;
This can be done in the objects_<engine_type>.test file.
   Performance tests with huge amount of tables and views can be done by
generating the objects several time in different databases.

3.2 DDL Performance
The run time of this test can be used as an indicator for DDL performance
of the engine.

3.3 ALTER TABLE performance
After object creation an ALTER TABLE <table_name> Engine <other_engine>
can be done.


4 Detailed run times
~~~~~~~~~~~~~~~~~~~~
Run times and storage requirements.

Without DROP SCHEMA:             With DROP SCHEMA
- InnoDB on my laptop:
real    36m25.700s               real    100m17.541s
531M    OBJECTS_TEST
du -h ib*
891M    ibdata1
5.1M    ib_logfile0
5.1M    ib_logfile1

- Falcon on my laptop:
real    106m22.480s              real    374m49.860s
531M    OBJECTS_TEST
du -h OBJECTS_TEST.n*
609M    OBJECTS_TEST.ndb
42M     OBJECTS_TEST.nl1
115M    OBJECTS_TEST.nl2

- InnoDB on faster machine:
real    10m58.265s               real    78m26.733s
531M    OBJECTS_TEST/
du -h  ib*
891M    ibdata1
5.1M    ib_logfile0
5.1M    ib_logfile1

- Falcon on faster machine:
real    25m49.409s               real    162m32.852s
531M    OBJECTS_TEST
du -h  OBJECTS_TEST.n*
609M    OBJECTS_TEST.ndb
86M     OBJECTS_TEST.nl1
72M     OBJECTS_TEST.nl2

--
Hakan Kuecuekyilmaz <hakan at mysql dot com>, 2006-08-18.
