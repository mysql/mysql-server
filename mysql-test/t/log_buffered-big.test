# With the addition of WL#14793, this test is mostly of historical interest.
# It could eventually be removed.

# When loadable logging-components were first introduced, their configuration
# was persisted in an InnoDB-table. This meant that we could only load those
# components once InnoDB was available. All error-events up to that point
# where saved in memory, and flushed to the loadable components once those
# components were available. At the same time, if start-up took a long time
# (e.g. due to a data dictionary upgrade, binlog replay, etc.), we would
# flush status to the built-in logger even before InnoDB became available,
# so the user would not consider the server frozen.
#
# The original version of this test primarily demonstrated some of the
# "long start-up" features (as introduced by WL#11875).
#
# WL#14793 re-organizes the loading of logging components so they are
# available very early during start-up. Because of this, most of the
# "intermediate updates" logic could be removed. Consequently, it also
# removed much of this test.

--source include/big_test.inc
--source include/not_windows.inc
--source include/not_asan.inc
--source include/not_parallel.inc

--let $MYSQLD_DATADIR=`SELECT @@datadir`

# Restart with both trad and JSON logs and verify that both logs show
# the same events.

--let LOG_FILE_TRAD=$MYSQL_TMP_DIR/buffered1.err
--let LOG_FILE_JSON=$MYSQL_TMP_DIR/buffered1.err.00.json
--let LOG_FILE_JSON_TO_TRAD=$MYSQL_TMP_DIR/buffered1.converted.err
# To test filtering on buffered messages
--let SUPPRESSED_ERRCODE1=convert_error(ER_BASEDIR_SET_TO)
# To test filtering on non-buffered messages
--let SUPPRESSED_ERRCODE2=convert_error(ER_DD_INIT_FAILED)

# restart the server so it will write both trad and JSON error logs,
# with some messages suppressed:
SET PERSIST log_error_services="log_filter_internal,log_sink_internal,log_sink_json";
SET PERSIST log_error_suppression_list="ER_BASEDIR_SET_TO,ER_DD_INIT_FAILED";

--let LOG_FILE1= $MYSQLTEST_VARDIR/tmp/test1.err
--let restart_parameters="restart: --log-error=$LOG_FILE_TRAD --log-error-verbosity=3"
--replace_result $LOG_FILE_TRAD LOG_FILE_TRAD
--source include/restart_mysqld.inc

SELECT "[ PASS ] Server has restarted.";

# Now convert the JSON log to trad format, so we can more easily compare
# the contents of the two:
--perl
   use strict;
   use JSON;
   my $file_trad= $ENV{'LOG_FILE_TRAD'};
   my $file_json= $ENV{'LOG_FILE_JSON'};
   my $file_conv= $ENV{'LOG_FILE_JSON_TO_TRAD'};

   # Read entire trad log so far (i.e. start-up log).
   open(FILET,"$file_trad") or die("Unable to open $file_trad $!\n");
   my @log_lines_trad=<FILET>;
   close(FILET);

   print "[ PASS ] Successfully read traditional log file.\n";

#  s/^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]:[0-9][0-9]:[0-9][0-9]\.[0-9][0-9][0-9][0-9][0-9][0-9][-+Z][0-9:]* *[0-9]* *(\[.*)$/$1/ for @log_lines_trad;

   # Read entire JSON log so far (i.e. start-up log).
   open(FILEJ,"$file_json") or die("Unable to open $file_json $!\n");
   my @log_lines_json=<FILEJ>;
   close(FILEJ);

   print "[ PASS ] Successfully read JSON log file.\n";

   # Write trad log output generated from JSON input
   # (so that we may use diff to show that trad log and the JSON log
   # contain the same events).
   open(my $outfh,">",$file_conv) or
     die("Unable to open file '$file_conv' for writing $!\n");

   # Test for validity of the JSON docs in error log.

   # JSON-log is in JSON-lines format. Convert it to plain JSON.
   my $string = "[\n".join("", @log_lines_json)."\n]";
   $string =~ s/\}\n\{/\},\n\{/g ;

   # Attempt to decode the JSON document we built:
   my $success=1;
   my $parsed_json = decode_json $string;

   unless ( $parsed_json )
   {
     print "[ FAIL ] Error while parsing the error log as a JSON document:".
           "\n$@\n";
     $success=0;
   }

   if($success)
   {
     print "[ PASS ] Error log successfully parsed as a JSON document.\n";

     my $line_count=0;
     for my $item( sort { $a->{time} cmp $b->{time} } @$parsed_json ) {
        my $errcode_formatted= sprintf("%06d", $item->{'err_code'});

        # Verify that the --log-error-suppression-list works for
        # buffered messages flushed after timeout as well as for
        # non-buffered messages.
        if ($errcode_formatted =~ $ENV{'SUPPRESSED_ERRCODE1'} or
            $errcode_formatted =~ $ENV{'SUPPRESSED_ERRCODE2'}) {
          print "[ FAIL ] Error message with error code suppressed using ".
              "--log-error-suppression-list is seen in the error log";
        }
        my $thread_id= defined $item->{'thread'} ? $item->{'thread'} : 0;
        my $label= ($item->{'label'} =~ "Error") ? uc $item->{'label'}
                                                 : $item->{'label'};
        my $msg= $item->{'msg'};
        # log_sink_trad does '\n' -> ' ', so we need to emulate that.
        $msg =~ s/\n/ /g;

        print $outfh $item->{'time'}." ".$thread_id." "."[".$label."] [MY-".
              $errcode_formatted."] [".$item->{'subsystem'}."] ".$msg . "\n";

        $line_count++;
     }
     if ($line_count == 0) {
       print "[ FAIL ] No lines were converted from JSON to trad.\n";
     }
     else {
       print "[ PASS ] Lines were converted from JSON to trad.\n";
     }
   }
   else {
     print "[ FAIL ] Error log could not be parsed as a JSON document.\n";
   }
   close($outfh);
 EOF

# Now, compare the two logs ("native" trad log, and JSON-converted-to-trad):
--perl
   use strict;
   use File::Compare;

   my $file_trad= $ENV{'LOG_FILE_TRAD'};
   my $file_conv= $ENV{'LOG_FILE_JSON_TO_TRAD'};

   my $compare = compare($file_conv, $file_trad);

   if ($compare == 0) {
     print "[ PASS ] Traditional log and JSON-converted-to-trad log are the same.\n";
   }
   else {
     print "[ FAIL ] Traditional log and JSON-converted-to-trad log are NOT the same.\n";

     $/ = undef;
     my $contents;

     print "\n--- Contents of converted file, ".$file_conv." ---\n";
     open my $fh, '<', $file_conv or die("Unable to open $file_conv $!\n");
     $contents = <$fh>;
     close $fh;
     print $contents;

     print "\n--- Contents of native trad file, ".$file_trad." ---\n";
     open my $fh, '<', $file_trad or die("Unable to open $file_trad $!\n");
     $contents = <$fh>;
     close $fh;
     print $contents;

     print "--- end of inserted log ---\n\n";
   }
 EOF

# Cleanup
RESET PERSIST log_error_services;
RESET PERSIST log_error_suppression_list;
SET GLOBAL log_error_services=DEFAULT;
SET GLOBAL log_error_suppression_list=DEFAULT;

--remove_files_wildcard $MYSQL_TMP_DIR buffered1*
