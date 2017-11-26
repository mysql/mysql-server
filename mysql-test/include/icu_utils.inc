#
# Utility functions for working with ICU.
#

--disable_query_log
CREATE FUNCTION icu_major_version() RETURNS INT
  RETURN regexp_replace(icu_version(), '([[:digit:]]+)\..*', '$1');
--enable_query_log
