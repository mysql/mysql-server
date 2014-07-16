/*
  This Dtrace script is used by the test "dynamic_tracing.test" for
  dynamic tracing. This script is executed in background by
  "dynamic_tracing.test" and SQL queries are executed concurrently. When
  any probe used in this script are hit then counter is incremented. After
  hitting all the 8(exp_probe_hits) probes, summary containing probes enabled
  and their hit state is printed.
*/

#pragma D option quiet

short query_parse_start;
short query_parse_done;
short select_start;
short select_done;
short net_read_start;
short net_read_done;
short handler_rdlock_start;
short handler_rdlock_done;

short tot_probe_hits;
short exp_probe_hits;

BEGIN
{
  query_parse_start= 0;
  query_parse_done= 0;
  select_start= 0;
  select_done= 0;
  net_read_start= 0;
  net_read_done= 0;
  handler_rdlock_start= 0;
  handler_rdlock_done= 0;
  
  tot_probe_hits= 0;
  exp_probe_hits= 8;
  printf("\n Dynamic tracing ...... started.\n");
}

mysql*:::query-parse-start
/query_parse_start == 0/
{
  query_parse_start++;
  tot_probe_hits++;
}

mysql*:::query-parse-done
/query_parse_done == 0/
{
  query_parse_done++;
  tot_probe_hits++;
}

mysql*:::select-start
/select_start == 0/
{
  select_start++;
  tot_probe_hits++;
}

mysql*:::select-done
/select_done == 0/
{
  select_done++;
  tot_probe_hits++;
}

mysql*:::net-read-start
/net_read_start == 0/
{
  net_read_start++;
  tot_probe_hits++;
}

mysql*:::net-read-done
/net_read_done == 0/
{
  net_read_done++;
  tot_probe_hits++;
}

mysql*:::handler-rdlock-start
/handler_rdlock_start == 0/
{
  handler_rdlock_start++;
  tot_probe_hits++;
}

mysql*:::handler-rdlock-done
/handler_rdlock_done == 0/
{
  handler_rdlock_done++;
  tot_probe_hits++;
}

mysql*:::query-parse-start,
mysql*:::query-parse-done,
mysql*:::select-start,
mysql*:::select-done,
mysql*:::net-read-start,
mysql*:::net-read-done,
mysql*:::handler-rdlock-start,
mysql*:::handler-rdlock-done
/tot_probe_hits >= exp_probe_hits/
{
  printf("\n query-parse-start    : %d", query_parse_start);
  printf("\n query-parse-done     : %d", query_parse_done);
  printf("\n select-start         : %d", select_start);
  printf("\n select-done          : %d", select_done);
  printf("\n net-read-start       : %d", net_read_start);
  printf("\n net-read-done        : %d", net_read_done);
  printf("\n handler_rdlock_start : %d", handler_rdlock_start);
  printf("\n handler_rdlock_done  : %d", handler_rdlock_done);
  printf("\n");
  printf("\n Expected probe hits  : %d", exp_probe_hits);
  printf("\n Actual probe hits    : %d\n", tot_probe_hits);
  printf("\n Dynamic tracing ...... completed.\n");
  exit(0);
}
