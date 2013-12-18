# Tests for the performance schema
# This helper can be used to enforce that no threads from previous
# tests are still running, which can impact some test scripts.

# Wait until there is only one session left, this one.

let $wait_condition=
  select count(*) = 1 from information_schema.processlist;
--source include/wait_condition.inc

# Threads are removed from information_schema.processlist
# very soon, but continue to execute in the server,
# before finally be removed from performance_schema.threads.
# Because instrumentation is optional, we use "<=" here.

let $wait_condition=
  select count(*) <= 2 from performance_schema.threads
  where `TYPE`='FOREGROUND';
--source include/wait_condition.inc

