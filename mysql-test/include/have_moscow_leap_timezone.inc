let $timezone= `SELECT FROM_UNIXTIME(1072904422) LIKE '2004-01-01 00:00:00'`;
if (!$timezone)
{
  --skip Test requires 'have_moscow_leap_timezone'
}
