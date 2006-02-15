#
# To check if the udf_example.so is available,
# try to load one function from it.
#
#
--require r/have_udf.require
--disable_abort_on_error
CREATE FUNCTION metaphon RETURNS STRING SONAME 'udf_example.so';
--disable_query_log
DROP FUNCTION metaphon;
--enable_query_log
--enable_abort_on_error
