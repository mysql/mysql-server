#!/usr/bin/perl
#
# Testing of grants.
# Note that this will delete all table and column grants !
#

use DBI;
use Getopt::Long;
use strict;

use vars qw($dbh $user_dbh $opt_help $opt_Information $opt_force $opt_debug 
	    $opt_verbose $opt_server $opt_root_user $opt_password $opt_user 
	    $opt_database $opt_host $version $user $tables_cols $columns_cols);

$version="1.0";
$opt_help=$opt_Information=$opt_force=$opt_debug=$opt_verbose=0;
$opt_host="localhost",
$opt_server="mysql";
$opt_root_user="root";
$opt_password="";
$opt_user="grant_user";
$opt_database="grant_test";

GetOptions("Information","help","server=s","root-user=s","password=s","user","database=s","force","host=s","debug","verbose") || usage();
usage() if ($opt_help || $opt_Information);

$user="$opt_user\@$opt_host";

if (!$opt_force)
{
  print_info()
}

$|=1;

$tables_cols="Host, Db, User, Table_name, Grantor, Table_priv, Column_priv";
$columns_cols="Host, Db, User, Table_name, Column_name, Column_priv";

#
# clear grant tables
#

$dbh = DBI->connect("DBI:mysql:mysql:$opt_host",
		    $opt_root_user,$opt_password,
		    { PrintError => 0}) || die "Can't connect to mysql server with user '$opt_root_user': $DBI::errstr\n";

safe_query("delete from user where user='$opt_user' or user='${opt_user}2'");
safe_query("delete from db where user='$opt_user'");
safe_query("delete from tables_priv");
safe_query("delete from columns_priv");
safe_query("lock tables mysql.user write"); # Test lock tables
safe_query("flush privileges");
safe_query("unlock tables");	     # should already be unlocked
safe_query("drop database $opt_database",2);
safe_query("create database $opt_database");

# check that the user can't login yet

user_connect(1);
#goto test;

#
# Test grants on user level
#

safe_query("grant select on *.* to $user");
safe_query("set password FOR ${opt_user}2\@$opt_host = password('test')",1);
safe_query("set password FOR $opt_user=password('test')");
user_connect(1);
safe_query("set password FOR $opt_user=''");
user_connect(0);
user_query("select * from mysql.user where user = '$opt_user'");
user_query("select * from mysql.db where user = '$opt_user'");
safe_query("grant select on *.* to $user,$user");

# The following should fail
user_query("insert into mysql.user (host,user) values ('error','$opt_user')",1);
user_query("update mysql.user set host='error' WHERE user='$opt_user'",1);
user_query("create table $opt_database.test (a int,b int)",1);
user_query("grant select on *.* to ${opt_user}2\@$opt_host",1);
safe_query("revoke select on $opt_database.test from $opt_user\@opt_host",1);
safe_query("revoke select on $opt_database.* from $opt_user\@opt_host",1);
safe_query("revoke select on *.* from $opt_user",1);
safe_query("grant select on $opt_database.not_exists to $opt_user",1);
safe_query("grant FILE on $opt_database.test to $opt_user",1);
safe_query("grant select on *.* to wrong___________user_name",1);
safe_query("grant select on $opt_database.* to wrong___________user_name",1);
user_query("grant select on $opt_database.test to $opt_user with grant option",1);
safe_query("set password FOR ''\@''=''",1);
user_query("set password FOR root\@$opt_host = password('test')",1);

# Change privileges for user
safe_query("revoke select on *.* from $user");
safe_query("grant create on *.* to $user");
user_connect(0);
user_query("create table $opt_database.test (a int,b int)");

safe_query("grant select(c) on $opt_database.test to $user",1);
safe_query("revoke select(c) on $opt_database.test from $user",1);
safe_query("grant select on $opt_database.test to wrong___________user_name",1);
user_query("INSERT INTO $opt_database.test values (2,0)",1);

safe_query("grant ALL PRIVILEGES on *.* to $user");
safe_query("REVOKE INSERT on *.* from $user");
user_connect(0);
user_query("INSERT INTO $opt_database.test values (1,0)",1);
safe_query("grant INSERT on *.* to $user");
user_connect(0);
user_query("INSERT INTO $opt_database.test values (2,0)");
user_query("select count(*) from $opt_database.test");
safe_query("revoke SELECT on *.* from $user");
user_connect(0);
user_query("select count(*) from $opt_database.test",1);
user_query("INSERT INTO $opt_database.test values (3,0)");
safe_query("grant SELECT on *.* to $user");
user_connect(0);
user_query("select count(*) from $opt_database.test");
safe_query("revoke ALL PRIVILEGES on *.* from $user");
user_connect(1);
safe_query("delete from user where user='$opt_user'");
safe_query("flush privileges");
if (0)				# Only if no anonymous user on localhost.
{
  safe_query("grant select on *.* to $opt_user");
  user_connect(0);
  safe_query("revoke select on *.* from $opt_user");
  user_connect(1);
}
safe_query("delete from user where user='$opt_user'");
safe_query("flush privileges");

#
# Test grants on database level
#
safe_query("grant select on $opt_database.* to $user");
safe_query("select * from mysql.user where user = '$opt_user'");
safe_query("select * from mysql.db where user = '$opt_user'");
user_connect(0);
user_query("select count(*) from $opt_database.test");
# The following should fail
user_query("select * from mysql.user where user = '$opt_user'",1);
user_query("insert into $opt_database.test values (4,0)",1);
user_query("update $opt_database.test set a=1",1); 
user_query("delete from $opt_database.test",1); 
user_query("create table $opt_database.test2 (a int)",1);
user_query("ALTER TABLE $opt_database.test add c int",1);
user_query("CREATE INDEX dummy ON $opt_database.test (a)",1);
user_query("drop table $opt_database.test",1);
user_query("grant ALL PRIVILEGES on $opt_database.* to ${opt_user}2\@$opt_host",1);

# Change privileges for user
safe_query("grant ALL PRIVILEGES on $opt_database.* to $user WITH GRANT OPTION");
user_connect(0);
user_query("insert into $opt_database.test values (5,0)");
safe_query("REVOKE ALL PRIVILEGES on * from $user",1);
safe_query("REVOKE ALL PRIVILEGES on *.* from $user");
safe_query("REVOKE ALL PRIVILEGES on $opt_database.* from $user");
safe_query("REVOKE ALL PRIVILEGES on $opt_database.* from $user");
user_connect(0);
user_query("insert into $opt_database.test values (6,0)",1);
safe_query("REVOKE GRANT OPTION on $opt_database.* from $user");
user_connect(1);
safe_query("grant ALL PRIVILEGES on $opt_database.* to $user");

user_connect(0);
user_query("select * from mysql.user where user = '$opt_user'",1);
user_query("insert into $opt_database.test values (7,0)");
user_query("update $opt_database.test set a=3 where a=2"); 
user_query("delete from $opt_database.test where a=3"); 
user_query("create table $opt_database.test2 (a int not null)");
user_query("alter table $opt_database.test2 add b int");
user_query("create index dummy on $opt_database.test2 (a)");
user_query("drop table $opt_database.test2");
user_query("show tables from grant_test");
# These should fail
user_query("insert into mysql.user (host,user) values ('error','$opt_user',0)",1);

# Revoke database privileges
safe_query("revoke ALL PRIVILEGES on $opt_database.* from $user");
safe_query("select * from mysql.user where user = '$opt_user'");
safe_query("select * from mysql.db where user = '$opt_user'");
user_connect(1);

#
# Test of grants on table level
#

safe_query("grant create on $opt_database.test2 to $user");
user_connect(0);
user_query("create table $opt_database.test2 (a int not null)");
user_query("show tables");	# Should only show test, not test2
user_query("show columns from test",1);
user_query("show keys from test",1);
user_query("show columns from test2");
user_query("show keys from test2");
user_query("select * from test",1);
safe_query("grant insert on $opt_database.test to $user");
user_query("show tables");
user_query("insert into $opt_database.test values (8,0)");
user_query("update $opt_database.test set b=1",1);
safe_query("grant update on $opt_database.test to $user");
user_query("update $opt_database.test set b=2");
user_query("delete from $opt_database.test",1);
safe_query("grant delete on $opt_database.test to $user");
user_query("delete from $opt_database.test where a=1",1);
user_query("update $opt_database.test set b=3 where b=1",1);
user_query("update $opt_database.test set b=b+1",1);

# Add one privilege at a time until the user has all privileges
user_query("select * from test",1);
safe_query("grant select on $opt_database.test to $user");
user_query("delete from $opt_database.test where a=1");
user_query("update $opt_database.test set b=2 where b=1");
user_query("update $opt_database.test set b=b+1");
user_query("select count(*) from test");

user_query("create table $opt_database.test3 (a int)",1);
user_query("alter table $opt_database.test2 add c int",1);
safe_query("grant alter on $opt_database.test2 to $user");
user_query("alter table $opt_database.test2 add c int");
user_query("create index dummy ON $opt_database.test (a)",1);
safe_query("grant index on $opt_database.test2 to $user");
user_query("create index dummy ON $opt_database.test2 (a)");
user_query("insert into test2 SELECT a,a from test",1);
safe_query("grant insert on test2 to $user",1);	# No table: mysql.test2
safe_query("grant insert(a) on $opt_database.test2 to $user");
user_query("insert into test2 SELECT a,a from test",1);
safe_query("grant insert(c) on $opt_database.test2 to $user");
user_query("insert into test2 SELECT a,a from test");
user_query("select count(*) from test2,test",1);
user_query("select count(*) from test,test2",1);
user_query("replace into test2 SELECT a from test",1);
safe_query("grant update on $opt_database.test2 to $user");
user_query("replace into test2 SELECT a,a from test",1);
safe_query("grant DELETE on $opt_database.test2 to $user");
user_query("replace into test2 SELECT a,a from test");
user_query("insert into test (a) SELECT a from test2",1);

user_query("drop table $opt_database.test2",1);
user_query("grant select on $opt_database.test2 to $user with grant option",1);
safe_query("grant drop on $opt_database.test2 to $user with grant option");
user_query("grant drop on $opt_database.test2 to $user with grant option");
user_query("grant select on $opt_database.test2 to $user with grant option",1);

# check rename privileges
user_query("rename table $opt_database.test2 to $opt_database.test3",1);
safe_query("grant CREATE,DROP on $opt_database.test3 to $user");
user_query("rename table $opt_database.test2 to $opt_database.test3",1);
user_query("create table $opt_database.test3 (a int)");
safe_query("grant INSERT on $opt_database.test3 to $user");
user_query("drop table $opt_database.test3");
user_query("rename table $opt_database.test2 to $opt_database.test3");
user_query("rename table $opt_database.test3 to $opt_database.test2",1);
safe_query("grant ALTER on $opt_database.test3 to $user");
user_query("rename table $opt_database.test3 to $opt_database.test2");
safe_query("revoke DROP on $opt_database.test2 from $user");
user_query("rename table $opt_database.test2 to $opt_database.test3");
user_query("drop table if exists $opt_database.test2,$opt_database.test3",1);
safe_query("drop table if exists $opt_database.test2,$opt_database.test3");

# Check that the user doesn't have some user privileges
user_query("create database $opt_database",1);
user_query("drop database $opt_database",1);
user_query("flush tables",1);
safe_query("flush privileges");

safe_query("select $tables_cols from mysql.tables_priv");
safe_query("revoke ALL PRIVILEGES on $opt_database.test from $user");
safe_query("revoke ALL PRIVILEGES on $opt_database.test2 from $user");
safe_query("revoke ALL PRIVILEGES on $opt_database.test3 from $user");
safe_query("revoke GRANT OPTION on $opt_database.test2 from $user");
safe_query("select $tables_cols from mysql.tables_priv");
user_query("select count(a) from test",1);

#
# Test some grants on column level
#

user_query("delete from $opt_database.test where a=2",1);
user_query("delete from $opt_database.test where A=2",1);
user_query("update test set b=5 where b>0",1);
safe_query("grant update(b),delete on $opt_database.test to $user");
safe_query("revoke update(a) on $opt_database.test from $user",1);
user_query("delete from $opt_database.test where a=2",1);
user_query("update test set b=5 where b>0",1);
safe_query("grant select(a),select(b) on $opt_database.test to $user");
user_query("delete from $opt_database.test where a=2");
user_query("delete from $opt_database.test where A=2");
user_query("update test set b=5 where b>0");
user_query("update test set a=11 where b>5",1);
user_query("select a,A from test");

safe_query("select $tables_cols from mysql.tables_priv");
safe_query("revoke ALL PRIVILEGES on $opt_database.test from $user");
safe_query("select $tables_cols from mysql.tables_priv");
safe_query("revoke GRANT OPTION on $opt_database.test from $user",1);
#
# Test grants on database level
#

safe_query("grant select(a) on $opt_database.test to $user");
user_query("show columns from test");
safe_query("grant insert (b), update (b) on $opt_database.test to $user");

user_query("select count(a) from test");
user_query("select count(skr.a) from test as skr");
user_query("select count(a) from test where a > 5");
user_query("insert into test (b) values (5)");
user_query("insert into test (b) values (a)");
user_query("update test set b=3 where a > 0");

user_query("select * from test",1);
user_query("select b from test",1);
user_query("select a from test where b > 0",1);
user_query("insert into test (a) values (10)",1);
user_query("insert into test (b) values (b)",1);
user_query("insert into test (a,b) values (1,5)",1);
user_query("insert into test (b) values (1),(b)",1);
user_query("update test set b=3 where b > 0",1);

safe_query("select $tables_cols from mysql.tables_priv");
safe_query("select $columns_cols from mysql.columns_priv");
safe_query("revoke select(a), update (b) on $opt_database.test from $user");
safe_query("select $tables_cols from mysql.tables_priv");
safe_query("select $columns_cols from mysql.columns_priv");

user_query("select count(a) from test",1);
user_query("update test set b=4",1);

safe_query("grant select(a,b), update (a,b) on $opt_database.test to $user");
user_query("select count(a),count(b) from test where a+b > 0");
user_query("insert into test (b) values (9)");
user_query("update test set b=6 where b > 0");

safe_query("flush privileges");	# Test restoring privileges from disk
safe_query("select $tables_cols from mysql.tables_priv");
safe_query("select $columns_cols from mysql.columns_priv");

# Try mixing of table and database privileges

user_query("insert into test (a,b) values (12,12)",1);
safe_query("grant insert on $opt_database.* to $user");
user_connect(0);
user_query("insert into test (a,b) values (13,13)");

# This grants and revokes SELECT on different levels.
safe_query("revoke select(b) on $opt_database.test from $user");
user_query("select count(a) from test where a+b > 0",1);
user_query("update test set b=5 where a=2");
safe_query("grant select on $opt_database.test to $user");
user_connect(0);
user_query("select count(a) from test where a+b > 0");
safe_query("revoke select(b) on $opt_database.test from $user");
user_query("select count(a) from test where a+b > 0");
safe_query("revoke select on $opt_database.test from $user");
user_connect(0);
user_query("select count(a) from test where a+b > 0",1);
safe_query("grant select(a) on $opt_database.test to $user");
user_query("select count(a) from test where a+b > 0",1);
safe_query("grant select on *.* to $user");
user_connect(0);
user_query("select count(a) from test where a+b > 0");
safe_query("revoke select on *.* from $user");
safe_query("grant select(b) on $opt_database.test to $user");
user_connect(0);
user_query("select count(a) from test where a+b > 0");


safe_query("select * from mysql.db where user = '$opt_user'");
safe_query("select $tables_cols from mysql.tables_priv where user = '$opt_user'");
safe_query("select $columns_cols from mysql.columns_priv where user = '$opt_user'");

safe_query("revoke ALL PRIVILEGES on $opt_database.test from $user");
user_query("select count(a) from test",1);
user_query("select * from mysql.user",1);
safe_query("select * from mysql.db where user = '$opt_user'");
safe_query("select $tables_cols from mysql.tables_priv where user = '$opt_user'");
safe_query("select $columns_cols from mysql.columns_priv where user = '$opt_user'");

#
# Test IDENTIFIED BY
#

safe_query("delete from user where user='$opt_user'");
safe_query("flush privileges");
safe_query("grant ALL PRIVILEGES on $opt_database.test to $user identified by 'dummy',  ${opt_user}\@127.0.0.1 identified by 'dummy2'");
user_connect(0,"dummy");
safe_query("grant SELECT on $opt_database.* to $user identified by ''");
user_connect(0);

#
# Clean up things
#

safe_query("drop database $opt_database");
safe_query("delete from user where user='$opt_user'");
safe_query("delete from db where user='$opt_user'");
safe_query("delete from tables_priv");
safe_query("delete from columns_priv");
safe_query("flush privileges");

print "end of test\n";
exit 0;

sub usage
{
    print <<EOF;
$0  Ver $version

This program tests that the GRANT commands works by creating a temporary
database ($opt_database) and user ($opt_user).

Options:

--database (Default $opt_database)
  In which database the test tables are created.

--force
  Don''t ask any question before starting this test.

--host='host name' (Default $opt_host)
  Host name where the database server is located.

--Information
--help
  Print this help

--password
  Password for root-user.

--server='server name'  (Default $opt_server)
  Run the test on the given SQL server.

--user  (Default $opt_user)
  A non-existing user on which we will test the GRANT commands.

--verbose
  Write all queries when we are execute them.

--root-user='user name' (Default $opt_root_user)
  User with privileges to modify the 'mysql' database.
EOF
  exit(0);
}


sub print_info
{
  my $tmp;
  print <<EOF;
This test will clear your table and column grant table and recreate the
$opt_database database !
All privileges for $user will be destroyed !

Don\'t run this test if you have done any GRANT commands that you want to keep!
EOF
 for (;;)
  {
    print "Start test (yes/no) ? ";
    $tmp=<STDIN>; chomp($tmp); $tmp=lc($tmp);
    last if ($tmp =~ /^yes$/i);
    exit 1 if ($tmp =~ /^n/i);
    print "\n";
  }
}


sub user_connect
{
  my ($ignore_error,$password)=@_;
  $password="" if (!defined($password));

  print "Connecting $opt_user\n" if ($opt_verbose);
  $user_dbh->disconnect if (defined($user_dbh));

  $user_dbh=DBI->connect("DBI:mysql:$opt_database:$opt_host",$opt_user,
			 $password, { PrintError => 0});
  if (!$user_dbh)
  {
    print "$DBI::errstr\n";
    if (!$ignore_error)
    {
      die "The above should not have failed!";
    }
  }
  elsif ($ignore_error)
  {
    die "Connect succeeded when it shouldn't have !\n";
  }
}

sub safe_query
{
  my ($query,$ignore_error)=@_;
  if (do_query($dbh,$query))
  {
    if (!defined($ignore_error))
    {
      die "The above should not have failed!";
    }
  }
  elsif (defined($ignore_error) && $ignore_error == 1)
  {
    die "Query '$query' succeeded when it shouldn't have !\n";
  }
}


sub user_query
{
  my ($query,$ignore_error)=@_;
  if (do_query($user_dbh,$query))
  {
    if (!defined($ignore_error))
    {
      die "The above should not have failed!";
    }
  }
  elsif (defined($ignore_error) && $ignore_error == 1)
  {
    die "Query '$query' succeeded when it shouldn't have !\n";
  }
}


sub do_query
{
  my ($my_dbh, $query)=@_;
  my ($sth,$row,$tab,$col,$found);

  print "$query\n" if ($opt_debug || $opt_verbose);
  if (!($sth= $my_dbh->prepare($query)))
  {
    print "Error in prepare: $DBI::errstr\n";
    return 1;
  }
  if (!$sth->execute)
  {
    print "Error in execute: $DBI::errstr\n";
    die if ($DBI::errstr =~ /parse error/);
    $sth->finish;
    return 1;
  }
  $found=0;
  while (($row=$sth->fetchrow_arrayref))
  {
    $found=1;
    $tab="";
    foreach $col (@$row)
    {
      print $tab;
      print defined($col) ? $col : "NULL";
      $tab="\t";
    }
    print "\n";
  }
  print "\n" if ($found);
  $sth->finish;
  return 0;
}
