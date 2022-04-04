let $have_build_id= `SELECT COUNT(*) != 0 FROM
    performance_schema.global_variables WHERE variable_name = 'build_id'`;
if (!$have_build_id)
{
  skip Test requires 'have_build_id';
}
