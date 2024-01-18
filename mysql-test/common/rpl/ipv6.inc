--echo connect (master,$IPv6,root,,test,MASTER_MYPORT);
connect (master,$IPv6,root,,test,$MASTER_MYPORT);
--echo connect (slave,$IPv6,root,,test,SLAVE_MYPORT);
connect (slave,$IPv6,root,,test,$SLAVE_MYPORT);
--echo connection master;
connection master;
reset binary logs and gtids;
source include/rpl/deprecated/show_binary_log_status.inc;
save_master_pos;
--echo connection slave;
connection slave;
RESET REPLICA;
eval CHANGE REPLICATION SOURCE TO SOURCE_HOST='$IPv6';
let $source_host= query_get_value(SHOW REPLICA STATUS, Source_Host, 1);
--echo Source-Host: $source_host
--echo disconnect slave;
disconnect slave;
--echo disconnect master;
disconnect master;
--echo connection default;
connection default;

