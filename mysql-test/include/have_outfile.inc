-- require r/have_outfile.require
disable_query_log;
select load_file(concat(@tmpdir,'/outfile.test'));
--remove_file $MYSQLTEST_VARDIR/tmp/outfile.test
enable_query_log;
