if (`SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.PLUGINS WHERE plugin_name = 'ngram' AND plugin_type = 'ftparser'`)
{
  --skip Test requires ngram plugin.
}
