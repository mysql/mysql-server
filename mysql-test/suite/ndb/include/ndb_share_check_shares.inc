connection server1;
--error 0,1193
set global ndbcluster_check_shares=1;
connection server2;
--error 0,1193
set global ndbcluster_check_shares=1;
connection server3;
--error 0,1193
set global ndbcluster_check_shares=1;
connection server4;
--error 0,1193
set global ndbcluster_check_shares=1;
