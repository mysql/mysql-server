# Only run this test if fips enabled openssl library is installed.
if (!$OPENSSL_FIPS_INSTALLED) {
  --skip openssl is not build with fips modue, fips functionality is not supported. or env OPENSSL_FIPS_INSTALLED not set.
}
