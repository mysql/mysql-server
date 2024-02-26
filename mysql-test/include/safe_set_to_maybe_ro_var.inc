# to mask out the error - never abort neither log in result file - in setting 
# to read-only variable.
# It is assumed that the new value is equal to one the var was set to.
# Such situation happens particularily with binlog_format that becomes read-only
# with ndb default storage.
#
# when generate results always watch the file to find what is expected,
# the SET query may fail

# script accepts $maybe_ro_var the var name and $val4var the value

### USAGE:
### let $maybe_ro_var= ...
### let $val4var= ...
### include/safe_set_to_maybe_ro_var.inc

--disable_result_log
--disable_abort_on_error
eval SET $maybe_ro_var = $val4var;
--enable_abort_on_error
--enable_result_log

eval SELECT $maybe_ro_var;
