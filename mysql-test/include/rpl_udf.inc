#####################################################################
# Author: Chuck Bell                                                #
# Date: 2006-12-21                                                  #
# Purpose: To test that UDFs are replicated in both row based and   #
# statement based format. This tests work completed in WL#3629.     #
#                                                                   #
# This test is designed to exercise two of the three types of UDFs: #
# 1) UDFs via loadable libraries, and 2) UDFs with a SQL body.      #
#####################################################################

--source include/have_udf.inc

#
# To run this tests the "sql/udf_example.c" need to be compiled into
# udf_example.so and LD_LIBRARY_PATH should be setup to point out where
# the library are.
#

connection master;
--disable_warnings
drop table if exists t1;
--enable_warnings

#
# Test 1) Test UDFs via loadable libraries
#
--echo "*** Test 1) Test UDFs via loadable libraries ***
--echo "Running on the master"
--enable_info
--replace_result $UDF_EXAMPLE_LIB UDF_EXAMPLE_LIB
eval CREATE FUNCTION myfunc_double RETURNS REAL SONAME "$UDF_EXAMPLE_LIB";
--replace_result $UDF_EXAMPLE_LIB UDF_EXAMPLE_LIB
eval CREATE FUNCTION myfunc_int RETURNS INTEGER SONAME "$UDF_EXAMPLE_LIB";
--replace_result $UDF_EXAMPLE_LIB UDF_EXAMPLE_LIB
--error ER_CANT_FIND_DL_ENTRY
eval CREATE FUNCTION myfunc_nonexist RETURNS INTEGER SONAME "$UDF_EXAMPLE_LIB";
--replace_column 3 UDF_LIB
SELECT * FROM mysql.func ORDER BY name;
--disable_info

save_master_pos;
connection slave;
sync_with_master;

# Check to see that UDF CREATE statements were replicated
--echo "Running on the slave"
--enable_info
--replace_column 3 UDF_LIB
SELECT * FROM mysql.func ORDER BY name;
--disable_info

connection master;

# Use the UDFs to do something
--echo "Running on the master"
--enable_info
eval CREATE TABLE t1(sum INT, price FLOAT(24)) ENGINE=$engine_type;
INSERT INTO t1 VALUES(myfunc_int(100), myfunc_double(50.00));
INSERT INTO t1 VALUES(myfunc_int(10), myfunc_double(5.00));
INSERT INTO t1 VALUES(myfunc_int(200), myfunc_double(25.00));
INSERT INTO t1 VALUES(myfunc_int(1), myfunc_double(500.00));
SELECT * FROM t1 ORDER BY sum;
--disable_info

sync_slave_with_master;

# Check to see if data was replicated
--echo "Running on the slave"
--enable_info
SELECT * FROM t1 ORDER BY sum;

# Check to see that the functions are available for execution on the slave
SELECT myfunc_int(25);
SELECT myfunc_double(75.00);
--disable_info

connection master;

# Drop the functions
--echo "Running on the master"
--enable_info
DROP FUNCTION myfunc_double;
DROP FUNCTION myfunc_int;
SELECT * FROM mysql.func ORDER BY name;
--disable_info

sync_slave_with_master;

# Check to see if the UDFs were dropped on the slave
--echo "Running on the slave"
--enable_info
SELECT * FROM mysql.func ORDER BY name;
--disable_info

connection master;

# Cleanup
--echo "Running on the master"
--enable_info
DROP TABLE t1;
--disable_info

#
# Test 2) Test UDFs with SQL body
#
--echo "*** Test 2) Test UDFs with SQL body ***
--echo "Running on the master"
--enable_info
CREATE FUNCTION myfuncsql_int(i INT) RETURNS INTEGER DETERMINISTIC RETURN i; 
CREATE FUNCTION myfuncsql_double(d DOUBLE) RETURNS INTEGER DETERMINISTIC RETURN d * 2.00; 
SELECT db, name, type,  param_list, body, comment FROM mysql.proc WHERE db = 'test' AND name LIKE 'myfuncsql%' ORDER BY name;
--disable_info

sync_slave_with_master;

# Check to see that UDF CREATE statements were replicated
--echo "Running on the slave"
--enable_info
SELECT db, name, type,  param_list, body, comment FROM mysql.proc WHERE db = 'test' AND name LIKE 'myfuncsql%' ORDER BY name;
--disable_info

connection master;

# Use the UDFs to do something
--echo "Running on the master"
--enable_info
eval CREATE TABLE t1(sum INT, price FLOAT(24)) ENGINE=$engine_type;
INSERT INTO t1 VALUES(myfuncsql_int(100), myfuncsql_double(50.00));
INSERT INTO t1 VALUES(myfuncsql_int(10), myfuncsql_double(5.00));
INSERT INTO t1 VALUES(myfuncsql_int(200), myfuncsql_double(25.00));
INSERT INTO t1 VALUES(myfuncsql_int(1), myfuncsql_double(500.00));
SELECT * FROM t1 ORDER BY sum;
--disable_info

sync_slave_with_master;

# Check to see if data was replicated
--echo "Running on the slave"
--enable_info
SELECT * FROM t1 ORDER BY sum;
--disable_info

connection master;

# Modify the UDFs to add a comment
--echo "Running on the master"
--enable_info
ALTER FUNCTION myfuncsql_int COMMENT "This was altered.";
ALTER FUNCTION myfuncsql_double COMMENT "This was altered.";
SELECT db, name, type,  param_list, body, comment FROM mysql.proc WHERE db = 'test' AND name LIKE 'myfuncsql%' ORDER BY name;
--disable_info

sync_slave_with_master;

# Check to see if data was replicated
--echo "Running on the slave"
--enable_info
SELECT db, name, type,  param_list, body, comment FROM mysql.proc WHERE db = 'test' AND name LIKE 'myfuncsql%' ORDER BY name;

# Check to see that the functions are available for execution on the slave
SELECT myfuncsql_int(25);
SELECT myfuncsql_double(75.00);
--disable_info

connection master;

# Drop the functions
--echo "Running on the master"
--enable_info
DROP FUNCTION myfuncsql_double;
DROP FUNCTION myfuncsql_int;
SELECT db, name, type,  param_list, body, comment FROM mysql.proc WHERE db = 'test' AND name LIKE 'myfuncsql%' ORDER BY name;
--disable_info

sync_slave_with_master;

# Check to see if the UDFs were dropped on the slave
--echo "Running on the slave"
--enable_info
SELECT db, name, type,  param_list, body, comment FROM mysql.proc WHERE db = 'test' AND name LIKE 'myfuncsql%' ORDER BY name;
--disable_info

connection master;

# Cleanup
--echo "Running on the master"
--enable_info
DROP TABLE t1;
--disable_info
