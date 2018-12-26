# Only run this test if fips enabled openssl library is not installed.
if ($OPENSSL_FIPS_INSTALLED) {
  --skip system is installed with fips openssl library.
}
