let $have_solaris =  `SELECT CONVERT(@@version_compile_os using latin1) LIKE "solaris%"`;
if ($have_solaris)
{
  --skip Test requires 'not_solaris'
}
