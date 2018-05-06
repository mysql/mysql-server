# Only run this test if YaSSL/wolfSSL is supported
let $shavars= query_get_value("SHOW STATUS LIKE 'Rsa_public_key'", Variable_name, 1);
if ($shavars == 'Rsa_public_key'){
  skip Need YaSSL/wolfSSL support;
}
