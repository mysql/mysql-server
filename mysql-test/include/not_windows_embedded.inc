let $is_win = `select convert(@@version_compile_os using latin1) IN ("Win32","Win64","Windows")`;
let $is_embedded = `select version() like '%embedded%'`;
#echo is_win: $is_win;
#echo is_embedded: $is_embedded;
if ($is_win)
{
  if ($is_embedded)
  {
    skip Not supported with embedded on windows;
  }
}
