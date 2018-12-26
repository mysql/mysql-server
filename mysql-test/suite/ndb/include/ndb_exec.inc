#
# Usage:
#
# let $ndb_exec_command= $NDB_DESC -d test t1;
# let $ndb_exec_select= output_line like 'Table options:%';
# let $ndb_exec_project= output_line;
# let $ndb_exec_replace= /([ _])[0-9][0-9]*/\1#/;
# source suite/ndb/include/ndb_exec.inc;
#

if ($ndb_exec_file) {
  echo The variable ndb_exec_file must be empty when calling ndb_exec.inc;
  die Bad argument ndb_exec_file must be empty;
}

if (!$ndb_exec_command) {
  echo The variable ndb_exec_command must be set when calling ndb_exec.inc;
  die Missing argument ndb_exec_command;
}

if (!$ndb_exec_select) {
  let $ndb_exec_select= TRUE;
}

if (!$ndb_exec_project) {
  let $ndb_exec_project= output_line;
}

if (!$ndb_exec_replace) {
  let $ndb_exec_replace=;
}

let $ndb_exec_file = $MYSQLTEST_VARDIR/tmp/ndb_exec_file.txt;
exec $ndb_exec_command > $ndb_exec_file;
disable_query_log;
create temporary table ndb_exec_output (
  idx int auto_increment primary key,
  output_line text
);
set @save_local_infile = @@local_infile;
set GLOBAL local_infile = ON;
eval
  load data local infile '$ndb_exec_file'
  into table ndb_exec_output
  fields terminated by '\r'
  (output_line,@dummy);
replace_regex $ndb_exec_replace;
eval
  select $ndb_exec_project
  from ndb_exec_output
  where $ndb_exec_select
  order by idx;
drop table ndb_exec_output;
set GLOBAL local_infile = @save_local_infile;
enable_query_log;
remove_file $ndb_exec_file;

let $ndb_exec_file=;
let $ndb_exec_command=;
let $ndb_exec_select=;
let $ndb_exec_project=;
let $ndb_exec_replace=;
