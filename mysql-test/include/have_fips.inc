# Only run this test if OpenSSL FIPS is enabled.
if (!$OPENSSL_FIPS_INSTALLED) {
  --skip The OpenSSL library in use is not built with FIPS module support.
}
