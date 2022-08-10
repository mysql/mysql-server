--disable_query_log
call mtr.add_suppression('Error in Log_event::read_log_event()');
call mtr.add_suppression("Ignoring ib_.*_0.dblwr - page size doesn't match");
--enable_query_log

# setup db and table
--disable_warnings
DROP DATABASE IF EXISTS dbredoopt;
CREATE DATABASE dbredoopt;
USE dbredoopt;
--enable_warnings
CREATE TABLE  dbredoopt.t1
        (col1 INT, col2 INT, c1 LONGBLOB, c2 LONGBLOB,
        PRIMARY KEY(col1), INDEX `idx2` (col2),
        INDEX `idx3` (c1(300),c2(200))
        );
delimiter |;
CREATE PROCEDURE dbredoopt.populate_t1()
BEGIN
        DECLARE i INT DEFAULT 1;
        while (i <= 500) DO
                INSERT INTO dbredoopt.t1 values (i, i,REPEAT('a',5000), REPEAT('b',5000));
                SET i = i + 1;
        END WHILE;
END|
delimiter ;|

# new user which execute load from different client sessions
CREATE USER 'redo_wl_user'@'localhost';
GRANT ALL PRIVILEGES ON *.* TO 'redo_wl_user'@'localhost' WITH GRANT OPTION;

# Poulate initial data in table
START TRANSACTION;
SELECT count(*) FROM dbredoopt.t1;
call dbredoopt.populate_t1();
SELECT count(*) FROM dbredoopt.t1;
SELECT col1 FROM dbredoopt.t1 LIMIT 10;
COMMIT;


# Following perl snippet generates SQL file with INSERT/UPDATE/DELETE
# with transactions
# Files are created per client.
# Same SQL used in all files but sequence of SQL is shuffled in each file
perl;
   $files_cnt=$ENV{'client_cnt'};
   $debug_var="crash_".$ENV{'debug_point'};
   @load_type_array = ('mixed','high');
   @log_buffer_size_array = ('1048576','4194304','8388608','16777216','33554432');
   @log_innodb_log_write_ahead_size_array = ('512','1024','2048','4096','8192','16384');

   $use_transactions = 1;
   $use_transactions = $ENV{'use_transactions'} if (defined($ENV{'use_transactions'}));

   while ($files_cnt >= 1) {
      die unless open(FILE, ">$ENV{MYSQLTEST_VARDIR}/tmp/t1"."_input".$files_cnt);
      @lines = ();
      @initial_line = ("START TRANSACTION;\n");
      @trn_lines = ("COMMIT;START TRANSACTION;\n");
      @rollback_lines = ("ROLLBACK;START TRANSACTION;\n");
      for ( $i = 580 ; $i < 1501 ; $i++ ) {
         push(@lines,"INSERT INTO dbredoopt.t1 VALUES (".$i.",".($i*-1).","."REPEAT('c',5000),REPEAT('d',5000));\n" );
         if ($i%20 == 0) {
            $rand_picked_int = rand_int(1);
            push(@lines,"UPDATE dbredoopt.t1 SET col1=(col1 * -1) , col2= (col2 * -1)". " WHERE ABS(col1)=".$rand_picked_int.";\n");
            push(@lines,"DELETE FROM dbredoopt.t1 WHERE ABS(col1)=".rand_int(2).";\n");
         }
      }
      push(@lines,@trn_lines,@trn_lines,@trn_lines,@trn_lines) if ($use_transactions);
      push(@lines,@rollback_lines,@rollback_lines) if ($use_transactions);
      shuffle(\@lines);
      splice @lines, (int rand(400)+100), 0, "set session debug=\"+d,".$debug_var."\";\n";
      splice @lines, (int rand(400)+100), 0, "set global debug=\"+d,".$debug_var."\";\n";

      # vary innodb_log_write_ahead_size
      print FILE "select \@\@innodb_log_write_ahead_size;\n";
      print FILE "set global innodb_log_write_ahead_size=".$log_innodb_log_write_ahead_size_array[rand @log_innodb_log_write_ahead_size_array].";\n";
      print FILE "select \@\@innodb_log_write_ahead_size;\n";

      # innodb-log-buffer-size is global variable
      print FILE "select \@\@innodb_log_buffer_size;\n";
      print FILE "set global innodb_log_buffer_size=".$log_buffer_size_array[rand @log_buffer_size_array].";\n";
      print FILE "select \@\@innodb_log_buffer_size;\n";
      print FILE "SET DEBUG_SYNC= 'now WAIT_FOR go_ahead';\n";
      print FILE @initial_line if ($use_transactions);
      print FILE @lines;
      rand() < 0.5 ? print FILE "COMMIT;\n" : print FILE "ROLLBACK;\n" if ($use_transactions);

      $files_cnt--;
      close(FILE);
   }

sub rand_int {
   return (int rand(1000)+501) if(@_[0]==1) ;
   return (int rand(500)+501) if(@_[0]==2) ;
}
sub shuffle {
    my $array = shift;
    my $i;
    for ($i = @$array; --$i; ) {
        my $j = int rand ($i+1);
        next if $i == $j;
        @$array[$i,$j] = @$array[$j,$i];
    }
}
EOF


# start mysql clients in parallel
let connection_cnt = $client_cnt;
while ($connection_cnt > 0 ) {
    --exec_in_background $MYSQL --force -vvv -hlocalhost --port=$MASTER_MYPORT -uredo_wl_user < $MYSQLTEST_VARDIR/tmp/t1_input$connection_cnt >$MYSQLTEST_VARDIR/tmp/t1_output$connection_cnt 2>&1
    dec $connection_cnt;
}

--echo # Check rows which are not modified by parallel load
SELECT col1 FROM dbredoopt.t1 WHERE col1%50=0 AND (col1 < 501 AND col1 > 0 );


--echo # Wait till all mysql clients are connected
let $wait_condition=
  SELECT count(*) = $client_cnt FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE User LIKE "%redo_wl_user%";
--source include/wait_condition.inc
--echo # Singal all mysql clients since each one is waiting for signal go_ahead
--echo # with 'go_ahead' signal , all client suppose to resume in parallel
--disable_query_log
--disable_result_log
let connection_cnt = $client_cnt;
while ($connection_cnt > 0 ) {
   let $mysql_errno = 0;
   --error 0,1040,1053,2002,2003,2006,2013,2016,2017
   SET DEBUG_SYNC= 'now SIGNAL go_ahead';
   if ($mysql_errno != 0) {
       let connection_cnt = 1;
   }
   dec $connection_cnt;
}
# See if server is alive
# If server is not alive the $mysql_errno will not 0 when 'select 1' is run
let $mysql_errno = 0;
--error 0,1040,1053,2002,2003,2006,2013,2016,2017
SELECT 1;
--echo # Wait till all clients are over or server is gone
#if ($mysql_errno == 0) {
#let $wait_timeout= 180;
#let $wait_condition=
#  SELECT count(*) = 0 FROM INFORMATION_SCHEMA.PROCESSLIST
#  WHERE user LIKE "%redo_wl_user%";
#--source include/wait_condition.inc
#}

let $mysql_errno = 0;
while ($mysql_errno == 0) {
--error 0,1040,1053,2002,2003,2006,2013,2016,2017
SELECT SLEEP(0.1);
}
--enable_query_log
--enable_result_log
# works
--source include/start_mysqld.inc
--echo # Check rows which are not modified by parallel load
SELECT col1 FROM dbredoopt.t1 WHERE col1%50=0 AND (col1 < 501 AND col1 > 0 );

--echo # Check rows which are updated by parallel load
# Insert in parallel load makes sure col2 = col1 * -1 , so that col1+col2=0
# and update in parallel is do not violate col1+col2=0 (by doing , col1*-1 & col2*-1)
SELECT count(*)=0 FROM (SELECT col1,col2 FROM dbredoopt.t1 WHERE col1 NOT BETWEEN 1 AND 501 ) AS A WHERE (A.col1+A.col2) != 0 ;

DROP DATABASE dbredoopt;
DROP USER 'redo_wl_user'@'localhost';
--remove_files_wildcard $MYSQLTEST_VARDIR/tmp/ t1_*put*
