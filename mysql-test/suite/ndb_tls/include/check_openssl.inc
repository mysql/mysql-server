# Detect whether compile-time OpenSSL library provides sufficient support
# for NDB to use TLS

let $ver = query_get_value(show status like "Tls_library_version", Value, 1);
let $match = `select "$ver" like "OpenSSL 1.0%"`;
if($match)
{
  skip OpenSSL too old;
}

