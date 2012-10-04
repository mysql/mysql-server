let $max_open_files_limit= `SELECT @@open_files_limit < 17000`;
if ($max_open_files_limit)
{
  skip Need open_files_limit >= 16450 (see ulimit -n);
}
