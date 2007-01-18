# Check that server has come out ot readonly mode
--disable_query_log
let $counter= 100;
let $mysql_errno= 1;
while ($mysql_errno)
{
  --error 0, 1005
  create table check_read_only(a int) engine=NDB;
  sleep 0.1;
  if (!$counter)
  {
    die("Failed while waiting for mysqld to come out of readonly mode");
  }
  dec $counter;
}

let $counter= 100;
let $mysql_errno= 1;
while ($mysql_errno)
{
  --error 0, 1036
  insert into check_read_only values(1);
  sleep 0.1;
  if (!$counter)
  {
    die("Failed while waiting for mysqld to come out of readonly mode");
  }
  dec $counter;
}
drop table check_read_only;
--enable_query_log
