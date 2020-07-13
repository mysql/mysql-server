#
# WL#13168: Test MySQL command line client(mysql) --load-data-local-dir option
#

let $test_dir=$MYSQLTEST_VARDIR/tmp/wl13168;
let $wrong_test_dir=$MYSQLTEST_VARDIR/tmp/wl13168_2;
let $nonexistent_test_dir=$MYSQLTEST_VARDIR/tmp/wl13168_3;
let $test_file=$MYSQLTEST_VARDIR/tmp/wl13168/t1;
let $test_file_wrong_case=$MYSQLTEST_VARDIR/tmp/wl13168/T1;
let $test_file_wrong_file=$MYSQLTEST_VARDIR/tmp/wl13168/t2;
let $test_file_wrong_dir=$wrong_test_dir/t1;
let $test_file_wrong_dir_case=$MYSQLTEST_VARDIR/Tmp/wl13168/t1;

--echo # create the test directory
mkdir $test_dir;
--echo # create the wrong test directory
mkdir $wrong_test_dir;
--echo # create the test file
--write_file $test_file
1,a
2,b
EOF
--echo # create the wrong test file
--write_file $test_file_wrong_file
1,a
2,b
EOF
--echo # create a file in wrong dir
--write_file $test_file_wrong_dir
1,a
2,b
EOF

--echo # setup
CREATE TABLE test.wl13168(id INT PRIMARY KEY, data VARCHAR(50));

--echo # FR2/FR1.2: specify test_dir: should work
--exec $MYSQL --load_data_local_dir="$test_dir" -e "LOAD DATA LOCAL INFILE '$test_file' INTO TABLE test.wl13168 FIELDS TERMINATED BY ','" 2>&1
--echo # verify that the data are loaded
SELECT * FROM test.wl13168 ORDER BY id;
DELETE FROM test.wl13168;

--echo # FR1.1: specify empty (default): should fail
--error 1
--exec $MYSQL -e "LOAD DATA LOCAL INFILE '$test_file' INTO TABLE test.wl13168 FIELDS TERMINATED BY ','" 2>&1
SELECT * FROM test.wl13168 ORDER BY id;

--echo # FR1.1: specify explicit empty: should fail
--error 1
--exec $MYSQL --load_data_local_dir= -e "LOAD DATA LOCAL INFILE '$test_file' INTO TABLE test.wl13168 FIELDS TERMINATED BY ','" 2>&1
SELECT * FROM test.wl13168 ORDER BY id;

--echo # FR1.1: specify explicit empty and local-infile: should work
--exec $MYSQL --load_data_local_dir= --local-infile -e "LOAD DATA LOCAL INFILE '$test_file' INTO TABLE test.wl13168 FIELDS TERMINATED BY ','" 2>&1
SELECT * FROM test.wl13168 ORDER BY id;
DELETE FROM test.wl13168;

--echo # FR1.1: specify wrong file: should fail
--error 1
--exec $MYSQL --load_data_local_dir=$test_file_wrong_file -e "LOAD DATA LOCAL INFILE '$test_file' INTO TABLE test.wl13168 FIELDS TERMINATED BY ','" 2>&1
SELECT * FROM test.wl13168 ORDER BY id;

--echo # FR1.1: specify wrong dir: should fail
--error 1
--exec $MYSQL --load_data_local_dir=$test_file_wrong_dir -e "LOAD DATA LOCAL INFILE '$test_file' INTO TABLE test.wl13168 FIELDS TERMINATED BY ','" 2>&1
SELECT * FROM test.wl13168 ORDER BY id;

--echo # FR1.1: specify wrong dir and local-infile: should work
--exec $MYSQL --load_data_local_dir=$test_file_wrong_dir --local-infile -e "LOAD DATA LOCAL INFILE '$test_file' INTO TABLE test.wl13168 FIELDS TERMINATED BY ','" 2>&1
SELECT * FROM test.wl13168 ORDER BY id;
DELETE FROM test.wl13168;

--echo # Expect no error on MacOS and error on others
let expected_error=`SELECT if (CONVERT(@@version_compile_os USING LATIN1) RLIKE '^(osx|macos)', 0, 1)`;

--echo # FR1.5: specify wrong case dir: should fail except on MacOS
--error $expected_error
--exec $MYSQL --load_data_local_dir=$test_file -e "LOAD DATA LOCAL INFILE '$test_file_wrong_dir_case' INTO TABLE test.wl13168 FIELDS TERMINATED BY ','" > $MYSQLTEST_VARDIR/tmp/wl13168_log.txt
remove_file $MYSQLTEST_VARDIR/tmp/wl13168_log.txt;
DELETE FROM test.wl13168;

--echo # FR1.5: specify wrong case file: should fail except on MacOS
--error $expected_error
--exec $MYSQL --load_data_local_dir=$test_file -e "LOAD DATA LOCAL INFILE '$test_file_wrong_case' INTO TABLE test.wl13168 FIELDS TERMINATED BY ','" > $MYSQLTEST_VARDIR/tmp/wl13168_log.txt
remove_file $MYSQLTEST_VARDIR/tmp/wl13168_log.txt;
DELETE FROM test.wl13168;

--echo # Expect no error on windows and error on others
let expected_error=`select if (convert(@@version_compile_os using latin1) IN ("Win32","Win64","Windows") = 1, 0, 1)`;

--echo # FR2.1: specify non-existent dir: expect success on windows and failure on unix
--error $expected_error
--exec $MYSQL --load_data_local_dir=$nonexistent_test_dir -e "SELECT 1" > $MYSQLTEST_VARDIR/tmp/wl13168_log.txt
remove_file $MYSQLTEST_VARDIR/tmp/wl13168_log.txt;

SET @@global.local_infile = 0;

--echo # FR2/FR1.2: specify test_dir: should fail
--error 1
--exec $MYSQL --load_data_local_dir="$test_dir" -e "LOAD DATA LOCAL INFILE '$test_file' INTO TABLE test.wl13168 FIELDS TERMINATED BY ','" 2>&1
SELECT * FROM test.wl13168 ORDER BY id;

--echo # cleanup
DROP TABLE test.wl13168;
SET @@global.local_infile = 1;
force-rmdir $test_dir;
force-rmdir $wrong_test_dir;
