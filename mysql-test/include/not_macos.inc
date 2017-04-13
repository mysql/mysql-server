let $have_macos = `SELECT LOCATE("osx", CONVERT(@@version_compile_os using latin1))`;
if ($have_macos)
{
  --skip Test requires 'not_macos'
}
