let $have_sparc = `select convert(@@version_compile_machine using latin1) IN ("sparc")`;
if ($have_sparc)
{
   let $have_debug = `select convert (@@version using latin1) like '%debug%'`;
   if ( $have_debug )
   {
      skip Test requires: 'not_sparc_debug';
   }
}
