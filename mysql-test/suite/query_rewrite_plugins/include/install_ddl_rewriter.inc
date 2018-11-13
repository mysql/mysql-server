--replace_regex /\.(dll|so)/.xxx/
eval INSTALL PLUGIN ddl_rewriter SONAME '$DDL_REWRITER';
