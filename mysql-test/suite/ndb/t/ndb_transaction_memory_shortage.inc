let $errs = 0;
let $mysql_errno=0;

error 0,1296,1297;
insert into t1 select x1.n+x2.n*10+x3.n*100, 1, 1+(x1.n+x2.n+x3.n) MOD 3, 1+x1.n+x2.n*10+x3.n*100 from num as x1, num as x2, num as x3;
echo mysql_errno $mysql_errno;
let $errs = $errs + $mysql_errno;
show warnings;

# Out of transaction memory in DBACC will wait for resource and retry, can have lock timeout
error 0, 1205, 1297;
update t1 set b = d, d = d + c * 1000;
echo mysql_errno $mysql_errno;
let $errs = $errs + $mysql_errno;
show warnings;
echo mysql_errno $mysql_errno;

error 0, 1205, 1297;
delete from t1 where a = 2222 and b = 1;
echo mysql_errno $mysql_errno;
let $errs = $errs + $mysql_errno;
show warnings;
echo mysql_errno $mysql_errno;

error 0, 1205, 1297;
delete from t1 where d = 3333;
echo mysql_errno $mysql_errno;
let $errs = $errs + $mysql_errno;
show warnings;
echo mysql_errno $mysql_errno;

error 0, 1205, 1296, 1297;
delete from t1 where c = 2;
echo mysql_errno $mysql_errno;
let $errs = $errs + $mysql_errno;
show warnings;
echo mysql_errno $mysql_errno;

error 0, 1205, 1296, 1297;
delete from t0 where c = 3;
echo mysql_errno $mysql_errno;
let $errs = $errs + $mysql_errno;
show warnings;
echo mysql_errno $mysql_errno;

error 0, 1205, 1296, 1297;
delete from t0;
echo mysql_errno $mysql_errno;
let $errs = $errs + $mysql_errno;
show warnings;
echo mysql_errno $mysql_errno;

if (!`select $errs`)
{
  die Some operation should have failed!;
}
