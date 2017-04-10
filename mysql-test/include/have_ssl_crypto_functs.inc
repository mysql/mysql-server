# "yes" means that the ssl library is found and the server can communicate using SSL.
# "no" means that there are no SSL functions in the server.
# "disabled" means that ssl functions exist, but communication is disabled for some reason.
let $have_ssl= `SELECT @@have_ssl LIKE 'YES' OR @@have_ssl LIKE 'DISABLED'`;
if (!$have_ssl)
{
  --skip Test requires 'have_ssl_is_yes_or_disabled_only'
}
