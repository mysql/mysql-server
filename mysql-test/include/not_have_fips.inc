# Only run this test if OpenSSL FIPS is *not* enabled.
if ($OPENSSL_FIPS_INSTALLED) {
  --skip The OpenSSL library in use is built with FIPS module support.
}
