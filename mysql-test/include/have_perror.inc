let $have_perror= `SELECT LENGTH("$MY_PERROR") > 0`;
if (!$have_perror)
{
  --skip Test requires 'have_perror'
}
