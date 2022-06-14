# Input file $check_file
# Pattern $check_pattern

--disable_query_log
--disable_result_log
create table test.check_file_contains(r varchar(1000)) engine=innodb;
eval load data infile '$check_file' into table test.check_file_contains;

eval select @match_count:=count(1) 
         from test.check_file_contains
  where r like '%$check_pattern%';

let $match_count=query_get_value(select @match_count as m, m, 1);

if ($match_count)
{
  --echo File contains row(s) matching pattern $check_pattern 
}
if (!$match_count)
{
  --echo File $check_file does not contain pattern $check_pattern
  --echo Content
  --enable_result_log
  select * from test.check_file_contains;
  --disable_result_log
  --echo /Content
}
 
drop table test.check_file_contains;
--enable_result_log
--enable_query_log
