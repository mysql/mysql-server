source ../mysql-test/include/icu_utils.inc;
let $icu_major_version=`SELECT icu_major_version()`;
source ../mysql-test/include/cleanup_icu_utils.inc;

if ($icu_major_version < $required_icu_version)
{
  skip Not run for ICU versions below $required_icu_version;
}
