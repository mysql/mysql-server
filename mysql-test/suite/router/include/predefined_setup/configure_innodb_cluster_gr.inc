# ==== Purpose ====
#
# Reusable configuration of InnoDB GR Cluster,
# that uses root account to access both meta and user data.
#
# ==== Usage ====
#
# --source ../include/predefined_setup/configure_innodb_cluster_gr.inc
#

--let $group_replication_group_name= aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa
--let $rpl_skip_group_replication_start= 1
--let $rpl_server_count= 3
--let $rpl_group_replication_single_primary_mode= 1
--source include/group_replication.inc

--let $rpl_connection_name= server1
--source include/connection.inc
--let $s1_uuid=`SELECT @@GLOBAL.server_uuid`
--let $s1_port=$SERVER_MYPORT_1
--source include/start_and_bootstrap_group_replication.inc

--let $rpl_connection_name= server2
--source include/connection.inc
--let $s2_uuid=`SELECT @@GLOBAL.server_uuid`
--let $s2_port=$SERVER_MYPORT_2
--source include/start_group_replication.inc

--let $rpl_connection_name= server3
--source include/connection.inc
--let $s3_uuid=`SELECT @@GLOBAL.server_uuid`
--let $s3_port=$SERVER_MYPORT_3
--source include/start_group_replication.inc

--let $rpl_connection_name= server1
--source include/connection.inc

--source ../innodb_cluster/initialize_schema.inc

--disable_query_log
--disable_result_log
--eval INSERT INTO mysql_innodb_cluster_metadata.clusters() VALUES(1,"my_cluster", null, null, '{"group_replication_group_name": "$group_replication_group_name"}', "gr","pm","{}", 1);
--eval INSERT INTO mysql_innodb_cluster_metadata.instances() VALUES(1,1,"127.0.0.1:$s1_port", "$s1_uuid", "local_i1", '{"mysqlClassic": "127.0.0.1:$s1_port"}', null, null);
--eval INSERT INTO mysql_innodb_cluster_metadata.instances() VALUES(2,1,"127.0.0.1:$s2_port", "$s2_uuid", "local_i2", '{"mysqlClassic": "127.0.0.1:$s2_port"}', null, null);
--eval INSERT INTO mysql_innodb_cluster_metadata.instances() VALUES(3,1,"127.0.0.1:$s3_port", "$s3_uuid", "local_i3", '{"mysqlClassic": "127.0.0.1:$s3_port"}', null, null);
INSERT INTO mysql_innodb_cluster_metadata.async_cluster_views(cluster_id, view_id, topology_type, view_change_reason, view_change_time, view_change_info, attributes)
    VALUES(1,1,'SINGLE-PRIMARY-TREE', "", NOW(), "{}", "{}");
INSERT INTO mysql_innodb_cluster_metadata.async_cluster_members() VALUES(1,1,1,1,true,"{}");


--source ../innodb_cluster/create_mysqlrouter_user.inc

--enable_query_log
--enable_result_log
echo # * insert three R/W nodes into MySQL InnoDB Cluster.;

--source include/rpl/sync.inc
