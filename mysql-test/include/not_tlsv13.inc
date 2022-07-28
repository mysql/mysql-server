if (`SELECT @@GLOBAL.tls_version LIKE '%TLSv1.3%'`) {
  --skip Requires no TLSv1.3
}
