let $have_windows = `SELECT CONVERT(@@version_compile_os using latin1) IN ("Win32", "Win64", "Windows")`;
if ($have_windows)
{
  --skip Test requires 'not_windows'
}
