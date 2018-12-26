# turn ndb_index_stat_enable ON or OFF
# caller sets @is_enable 0/1
# based on global variable, local follows global
# do nothing if value is already correct
# setting OFF drops stats tables to avoid MTR diff

let is_enable_on = `select @is_enable and not @@global.ndb_index_stat_enable`;
let is_enable_off = `select not @is_enable and @@global.ndb_index_stat_enable`;
set @is_enable = NULL;

--echo # is_enable_on=$is_enable_on is_enable_off=$is_enable_off

--echo # ndb_index_stat_enable - before
show global variables like 'ndb_index_stat_enable';
show local variables like 'ndb_index_stat_enable';

if ($is_enable_on)
{
  # first global
  eval set @@global.ndb_index_stat_enable = 1;
  eval set @@local.ndb_index_stat_enable = 1;

  # stats thread creates stats tables
}

if ($is_enable_off)
{
  # first local
  eval set @@local.ndb_index_stat_enable = 0;
  eval set @@global.ndb_index_stat_enable = 0;

  # stats thread does not (and must not) drop stats tables
  drop table mysql.ndb_index_stat_sample;
  drop table mysql.ndb_index_stat_head;
}

--echo # ndb_index_stat_enable - after
show global variables like 'ndb_index_stat_enable';
show local variables like 'ndb_index_stat_enable';

# wait for stats thread to notice any change
source include/ndb_index_stat_wait.inc;
