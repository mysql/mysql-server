let $timezone= `SELECT FROM_UNIXTIME(24*3600) LIKE '1970-01-02 01:00:00'`;
if (!$timezone)
{
  --skip Test requires 'have_met_timezone'
}
