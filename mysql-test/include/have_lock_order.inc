let $have_lock_order= `SELECT COUNT(*) != 0 FROM performance_schema.global_variables WHERE variable_name = 'lock_order'`;
if (!$have_lock_order)
{
  skip Test requires 'have_lock_order';
}
