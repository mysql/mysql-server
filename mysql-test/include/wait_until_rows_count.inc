# include/wait_until_rows_count.inc
# inspired by wait_for_slave_status by Matthias Leich
#
# SUMMARY
#
#    Waits until SELECT count(*)-$count from $table returns zero
#
# USAGE
#
#    Set vars like
#      let $count=11;
#      let $table=t1;
#      # invoke the macro
#      --include wait_until_rows_count.inc
#
# EXAMPLE
# extra/binlog/binlog_insert_delayed.test
#
#
# TODO: generalize up to wait_[until|while] with arbitrary select or even query and
#       a condition to wait or get awakened
#       It's impossible to implement such a "most" general macro without
#       extending mysqltest. Just no way to pass a query as an argument and
#       evaluate it here, like eval "$quuery". One is bound
#       to specify it inside of the macro

--disable_query_log

let $wait_counter= 300;     # max wait in 0.1 seconds
while ($wait_counter)
{
    eval select count(*)-$count from $table into @rez;
    let $rez=`select @rez`;
    let $success=`SELECT @rez = 0`;
    let $no_success=1;
    if ($success)
    {
        let $wait_counter= 1; # droppping counter to leave loop
        let $no_success=0;
    }
    if ($no_success)
    {
       --sleep 0.1
    }
    dec $wait_counter;
}

--enable_query_log
if ($no_success)
{
	--die Timeout in wait_until_rows_count.inc, required table never had a prescribed number of rows.
}
