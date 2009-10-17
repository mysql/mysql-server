#
# usage:
#
#   -- let $diff_table=test.t
#   -- source suite/rpl/include/rpl_chained_3_hosts_diff_tables.inc


-- echo Comparing tables mysqld_a:$diff_table and mysqld_b:$diff_table (Chained replication 3 hosts)
connect (master,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (slave,127.0.0.1,root,,test,$SLAVE_MYPORT,);

let $diff_table_1=master:$diff_table;
let $diff_table_2=slave:$diff_table;
source include/diff_tables.inc;

-- disconnect master
-- disconnect slave

-- echo Comparing tables mysqld_a:$diff_table and mysqld_b:$diff_table (Chained replication 3 hosts)
connect (master,127.0.0.1,root,,test,$SLAVE_MYPORT,);
connect (slave,127.0.0.1,root,,test,$SLAVE_MYPORT1,);

let $diff_table_1=master:$diff_table;
let $diff_table_2=slave:$diff_table;
source include/diff_tables.inc;

-- disconnect master
-- disconnect slave

-- connection mysqld_a
