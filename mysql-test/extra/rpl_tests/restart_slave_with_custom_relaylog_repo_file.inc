# ==== Purpose ====
#
# This file restarts a server after replacing it relay log info repository file by a custom one
#
# ==== Usage ====
#
# --let $sub_testcase= <sub testcase name>
#
# Parameters:
#  $slave_server_num
#    The number of the slave server for which operations in this script
#    are to be performed.
#  $rpl_server_restart_parameters
#    The parameters passed to the server on start
#  $sub_testcase
#    The test case name for which file repo is to be prepared.

--write_file $MYSQL_TMP_DIR/create_file_repo_with_PCU_has_@.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
10
./slave-relay-bin.000001
4

0
0
0
1

replication_applier
l@calhost
EOF
END_OF_PROCEDURE

--write_file $MYSQL_TMP_DIR/create_file_repo_with_anon_PCU.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
10
./slave-relay-bin.000001
4

0
0
0
1


localhost
EOF
END_OF_PROCEDURE

--write_file $MYSQL_TMP_DIR/create_file_repo_with_nonexist_PCU.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
10
./slave-relay-bin.000001
4

0
0
0
1

replication_applier_nonexist
localhost
EOF
END_OF_PROCEDURE

--write_file $MYSQL_TMP_DIR/create_file_repo_with_PCU.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
10
./slave-relay-bin.000001
4

0
0
0
1

replication_applier
localhost
EOF
END_OF_PROCEDURE

--write_file $MYSQL_TMP_DIR/create_file_repo_with_no_require_flag_no_priv_user.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
10
./slave-relay-bin.000001
4

0
0
0
1



EOF
END_OF_PROCEDURE

--write_file $MYSQL_TMP_DIR/create_file_repo_with_no_require_flag_with_priv_user.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
10
./slave-relay-bin.000001
4

0
0
0
1

replication_applier
localhost
EOF
END_OF_PROCEDURE

--write_file $MYSQL_TMP_DIR/create_file_repo_with_no_require_table_primary_key_check.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
11
./slave-relay-bin.000001
4

0
0
0
1

root
localhost
0
EOF
END_OF_PROCEDURE

--write_file $MYSQL_TMP_DIR/create_file_repo_with_invalid_require_table_primary_key_check.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
12
./slave-relay-bin.000001
4

0
0
0
1

root
localhost
0
6
EOF
END_OF_PROCEDURE

--write_file $MYSQL_TMP_DIR/create_file_repo_with_no_assign_gtids_to_anonymous_transactions.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
12
./slave-relay-bin.000001
4

0
0
0
1

root
localhost
0
1
EOF
END_OF_PROCEDURE

--write_file $MYSQL_TMP_DIR/create_file_repo_with_invalid_type_assign_gtids_to_anonymous_transactions.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
14
./slave-relay-bin.000001
4

0
0
0
1

root
localhost
0
1
6

EOF
END_OF_PROCEDURE

--write_file $MYSQL_TMP_DIR/create_file_repo_with_invalid_default_value_for_type_off_assign_gtids_to_anonymous_transactions.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
14
./slave-relay-bin.000001
4

0
0
0
1

root
localhost
0
1
1
aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaaa
EOF
END_OF_PROCEDURE

--write_file $MYSQL_TMP_DIR/create_file_repo_with_invalid_value_for_type_not_off_assign_gtids_to_anonymous_transactions.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
14
./slave-relay-bin.000001
4

0
0
0
1

root
localhost
0
1
3

EOF
END_OF_PROCEDURE

--write_file $MYSQL_TMP_DIR/create_file_repo_with_invalid_value_assign_gtids_to_anonymous_transactions.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
14
./slave-relay-bin.000001
4

0
0
0
1

root
localhost
0
1
3
aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa
EOF
END_OF_PROCEDURE

--write_file $MYSQL_TMP_DIR/create_file_repo_with_no_value_assign_gtids_to_anonymous_transactions.inc END_OF_PROCEDURE
  --write_file $MYSQLD_DATADIR/$filename
13
./slave-relay-bin.000001
4

0
0
0
1

root
localhost
0
1
3
EOF
END_OF_PROCEDURE

--let $MYSQLD_DATADIR= `select @@datadir`
--let $filename = `SELECT @@relay_log_info_file`

--let $rpl_server_number = $slave_server_num
--let $rpl_force_stop=1
--source include/rpl_stop_server.inc
--remove_file $MYSQLD_DATADIR/$filename

if ($sub_testcase == default) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_PCU.inc
}
if ($sub_testcase == host_name_has_@) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_PCU_has_@.inc
}
if ($sub_testcase == anonymous_user) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_anon_PCU.inc
}
if ($sub_testcase == nonexist_user) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_nonexist_PCU.inc
}
if ($sub_testcase == no_flag_no_priv_user) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_no_require_flag_no_priv_user.inc
}
if ($sub_testcase == no_flag_with_priv_user) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_no_require_flag_with_priv_user.inc
}
if ($sub_testcase == no_require_table_primary_key_check) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_no_require_table_primary_key_check.inc
}
if ($sub_testcase == invalid_require_table_primary_key_check) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_invalid_require_table_primary_key_check.inc
}
if ($sub_testcase == no_assign_gtids_to_anonymous_transactions) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_no_assign_gtids_to_anonymous_transactions.inc
}
if ($sub_testcase == invalid_type_assign_gtids_to_anonymous_transactions) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_invalid_type_assign_gtids_to_anonymous_transactions.inc
}
if ($sub_testcase == invalid_default_value_for_type_off_assign_gtids_to_anonymous_transactions) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_invalid_default_value_for_type_off_assign_gtids_to_anonymous_transactions.inc
}
if ($sub_testcase == invalid_value_for_type_not_off_assign_gtids_to_anonymous_transactions) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_invalid_value_for_type_not_off_assign_gtids_to_anonymous_transactions.inc
}
if ($sub_testcase == invalid_value_assign_gtids_to_anonymous_transactions) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_invalid_value_assign_gtids_to_anonymous_transactions.inc
}
if ($sub_testcase == no_value_assign_gtids_to_anonymous_transactions) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_no_value_assign_gtids_to_anonymous_transactions.inc
}
--let $rpl_server_parameters= $rpl_server_restart_parameters

--let $explicit_default_wait_counter = 15000
--source include/rpl_start_server.inc

--remove_file $MYSQL_TMP_DIR/create_file_repo_with_no_require_flag_no_priv_user.inc
--remove_file $MYSQL_TMP_DIR/create_file_repo_with_no_require_flag_with_priv_user.inc
--remove_file $MYSQL_TMP_DIR/create_file_repo_with_PCU.inc
--remove_file $MYSQL_TMP_DIR/create_file_repo_with_PCU_has_@.inc
--remove_file $MYSQL_TMP_DIR/create_file_repo_with_anon_PCU.inc
--remove_file $MYSQL_TMP_DIR/create_file_repo_with_nonexist_PCU.inc
--remove_file $MYSQL_TMP_DIR/create_file_repo_with_no_require_table_primary_key_check.inc
--remove_file $MYSQL_TMP_DIR/create_file_repo_with_invalid_require_table_primary_key_check.inc
--remove_file $MYSQL_TMP_DIR/create_file_repo_with_no_assign_gtids_to_anonymous_transactions.inc
--remove_file $MYSQL_TMP_DIR/create_file_repo_with_invalid_type_assign_gtids_to_anonymous_transactions.inc
--remove_file $MYSQL_TMP_DIR/create_file_repo_with_invalid_default_value_for_type_off_assign_gtids_to_anonymous_transactions.inc
--remove_file $MYSQL_TMP_DIR/create_file_repo_with_invalid_value_assign_gtids_to_anonymous_transactions.inc
--remove_file $MYSQL_TMP_DIR/create_file_repo_with_invalid_value_for_type_not_off_assign_gtids_to_anonymous_transactions.inc
--remove_file $MYSQL_TMP_DIR/create_file_repo_with_no_value_assign_gtids_to_anonymous_transactions.inc
