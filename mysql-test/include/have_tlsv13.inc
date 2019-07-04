if (`SELECT @@GLOBAL.tls_version NOT LIKE '%TLSv1.3%'`) {
  --skip Requires TLSv1.3
}
