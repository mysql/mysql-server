# ==== Purpose ====
#
# This file restarts a server after replacing its connection info repository file
#
# ==== Usage ====
#
# --let $sub_testcase= <sub testcase name>
#
# Parameters:
#  $replica_server_num
#    The number of the replica server for which operations in this script
#    are to be performed.
#  $repo_info_file_path
#    The repository file with path included
#  $rpl_server_restart_parameters
#    The parameters passed to the server on start
#  $sub_testcase
#    The test case name for which file repo is to be prepared.

--write_file $MYSQL_TMP_DIR/create_file_repo_with_no_gitd_only.inc END_OF_PROCEDURE
  --write_file $repo_info_file_path
32
master-bin.000001
156
127.0.0.1
root

13000
1
0





0
30.000

0
6b3ce8fe-81b7-11eb-b24e-001cbfe2e6a4
10


0



0

uncompressed
3

0
EOF
END_OF_PROCEDURE

--let $rpl_server_number = $slave_server_num
--let $rpl_force_stop=1
--source include/rpl_stop_server.inc
--remove_file $repo_info_file_path

if ($sub_testcase == no_gtid_only) {
  --source $MYSQL_TMP_DIR/create_file_repo_with_no_gitd_only.inc
}

--let $rpl_server_parameters= $rpl_server_restart_parameters

--let $explicit_default_wait_counter = 15000
--source include/rpl_start_server.inc

--remove_file $MYSQL_TMP_DIR/create_file_repo_with_no_gitd_only.inc
