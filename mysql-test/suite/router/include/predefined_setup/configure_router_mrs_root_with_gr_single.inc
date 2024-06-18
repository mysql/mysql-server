# ==== Purpose ====
#
# Reusable configuration of Router with MRS plugin,
# that uses root account to access both meta and user data.
#
# ==== Usage ====
#
# --source ../include/predefined_setup/configure_router_mrs_root_with_gr.inc
#

--let $group_replication_group_name= aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa

if (!$dont_init_gr)
{
--let $rpl_skip_group_replication_start= 1
--let $rpl_group_replication_single_primary_mode=1
--let $rpl_server_count= 3
--source include/group_replication.inc

--source include/start_and_bootstrap_group_replication.inc
}

if (!$s1_port)
{
  --let $rpl_connection_name= server1
  --source include/connection.inc
  --let $s1_uuid=`SELECT @@GLOBAL.server_uuid`
  --let $s1_port=$SERVER_MYPORT_1
}

if (!$s2_port)
{
  --let $rpl_connection_name= server2
  --source include/connection.inc
  --source include/start_group_replication.inc
  --let $s2_uuid=`SELECT @@GLOBAL.server_uuid`
  --let $s2_port=$SERVER_MYPORT_2
}

if (!$s3_port)
{
  --let $rpl_connection_name= server3
  --source include/connection.inc
  --source include/start_group_replication.inc
  --let $s3_uuid=`SELECT @@GLOBAL.server_uuid`
  --let $s3_port=$SERVER_MYPORT_3
}

--let $rpl_connection_name= server1
--source include/connection.inc

--source ../mrs/initialize_schema.inc
--source ../innodb_cluster/initialize_schema.inc

--disable_query_log
--disable_result_log
--eval INSERT INTO mysql_innodb_cluster_metadata.clusters() VALUES(1,"my_cluster",null, null, '{"group_replication_group_name": "$group_replication_group_name"}', "gr","mm","{}", 1);
--eval INSERT INTO mysql_innodb_cluster_metadata.instances() VALUES(1,1,"127.0.0.1:$s1_port", "$s1_uuid", "local_i1", '{"mysqlClassic": "127.0.0.1:$s1_port"}', null, null);
--eval INSERT INTO mysql_innodb_cluster_metadata.instances() VALUES(2,1,"127.0.0.1:$s2_port", "$s2_uuid", "local_i2", '{"mysqlClassic": "127.0.0.1:$s2_port"}', null, null);
--eval INSERT INTO mysql_innodb_cluster_metadata.instances() VALUES(3,1,"127.0.0.1:$s3_port", "$s3_uuid", "local_i3", '{"mysqlClassic": "127.0.0.1:$s3_port"}', null, null);
INSERT INTO mysql_innodb_cluster_metadata.async_cluster_views(cluster_id, view_id, topology_type, view_change_reason, view_change_time, view_change_info, attributes)
    VALUES(1,1,'SINGLE-PRIMARY-TREE', "", NOW(), "{}", "{}");
INSERT INTO mysql_innodb_cluster_metadata.async_cluster_members() VALUES(1,1,1,1,true,"{}");
--enable_query_log
--enable_result_log
echo # * insert three RW and RO nodes into MySQL InnoDB Cluster.;

if (!$dont_init_gr)
{
  --source include/rpl/sync.inc
}

--let $extra_route_rw_port=`SELECT $ROUTER_PORT + 10`
--let $extra_route_ro_port=`SELECT $ROUTER_PORT + 11`
--let $extra_rw_route=inno_rw
--let $extra_ro_route=inno_ro
--let $extra_metadata_user=root
--source ../mysqlrouter_create_innodb_cluster_router.inc

--let $extra_mrs_read_write_route=inno_rw
--let $extra_mrs_read_only_route=inno_ro
--source ./configure_router_mrs_root_inner.inc

if (!$dont_init_gr)
{
  --source include/rpl/sync.inc
}

--source ../mysqlrouter_mysql_connection.inc

--replace_regex /restart: (.*\/)*/restart:/
--let $restart_parameters= restart: $extra_append_router_config
--source ../mysqlrouter_restart.inc

--let $mrs_host_and_port=127.0.0.1:$HTTP_SERVER_PORT
--let MRS_CLIENT_ARGS=$MRS_CLIENT --url https://$mrs_host_and_port
