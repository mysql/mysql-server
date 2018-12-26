# Create temporary tables in various storage engines.
create temporary table ta (i int not null) engine=archive;
create temporary table tb (i int not null) engine=blackhole;
create temporary table tc (i int not null) engine=csv;
create temporary table th (i int not null) engine=heap;
create temporary table ti (i int not null) engine=innodb;
create temporary table tm (i int not null) engine=myisam;
create temporary table tg (i int not null) engine=merge union=();
# Also manually create files mimicing temporary table files which
# are symlinks to files in data-directory.
create database mysqltest;
create table mysqltest.t1 (i int not null) engine=myisam;
# Check files created for them.
#sqlXXXX.ARZ
#sqlXXXX.CSM
#sqlXXXX.CSV
#sqlXXXX.MYD
#sqlXXXX.MYI
#sqlXXXX.MRG
#sqlXXXX.MYD
#sqlXXXX.MYI
# Crash server:
begin;
select * from ti;
i
# Ask server to crash on next commit.
set session debug="+d,crash_commit_before";
# Write file to make mysql-test-run.pl start up the server again.
# Crash server.
commit;
ERROR HY000: Lost connection to MySQL server during query
# Wait until disconnected.
# Check that temporary table files are still around.
#sqlXXXX.ARZ
#sqlXXXX.CSM
#sqlXXXX.CSV
#sqlXXXX.MYD
#sqlXXXX.MYI
#sqlXXXX.MRG
#sqlXXXX.MYD
#sqlXXXX.MYI
# Reconnect.
# Check that temporary table files are gone after restart,
# including symlinks.
# But files in data-directory to which these symlinks were pointing
# should still be around.
t1.MYD
t1.MYI
t1_XXX.sdi
drop database mysqltest;
