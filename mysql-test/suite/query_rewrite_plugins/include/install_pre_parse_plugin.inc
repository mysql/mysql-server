--replace_regex /\.(dll|so)/.xxx/
eval INSTALL PLUGIN rewrite_example SONAME '$REWRITE_EXAMPLE';
