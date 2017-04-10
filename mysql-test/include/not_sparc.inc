let $have_sparc = `select convert(@@version_compile_machine using latin1) IN ("sparc")`;
if ($have_sparc)
{
   skip Test requires: 'not_sparc';
}
