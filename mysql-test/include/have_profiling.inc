let $have_profiling= `SELECT @@have_profiling`;
if ($have_profiling != YES)
{
  --skip Test requires 'have_profiling'
}
