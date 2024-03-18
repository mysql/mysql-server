let $have_asan_windows = `SELECT VERSION() LIKE '%asan%' AND CONVERT(@@version_compile_os using latin1) IN ('Win32', 'Win64', 'Windows')`;
if ($have_asan_windows)
{
  --skip Test requires 'not_asan_windows'
}
