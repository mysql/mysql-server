# Only run this test if YaSSL/wolfSSL is supported
let $shavars= query_get_value("SELECT COUNT(*) as shavars FROM performance_schema.session_variables WHERE variable_name = 'sha256_password_private_key_path'", shavars, 1);
if ($shavars != 0){
  skip Need YaSSL/wolfSSL support;
}
