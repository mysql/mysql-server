let $have_macos = `select convert(@@version_compile_os using latin1) RLIKE '^(osx|macos)' = 1`;
if ($have_macos)
{
   let $have_debug = `select convert (@@version using latin1) like '%debug%'`;
   if ( $have_debug )
   {
      skip Test requires: 'not_mac_os_debug';
   }
}
