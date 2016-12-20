# Several partition-related tests include this file, because Valgrind/Purify
# builds previously disabled symlink support;
# Some partition functionality depends on symlink support.

let $have_symlink= query_get_value(SHOW VARIABLES LIKE 'have_symlink', Value, 1);
if ($have_symlink != YES)
{
  --skip Test requires 'have_symlink'
}
