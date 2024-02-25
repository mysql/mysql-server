if (`SELECT @@GLOBAL.tls_version LIKE '%TLSv1.3%'`) {
  --skip Doesn't support TLSv1.3
}
