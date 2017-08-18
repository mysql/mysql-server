#
# Bug#26331795: CRASH WHEN SETTING LOG_ERROR_SERVICES TO NULL
#

# this should throw a warning:
SET GLOBAL log_error_services='';
# this should throw an error:
--error ER_WRONG_VALUE_FOR_VAR
SET GLOBAL log_error_services=NULL;
# clean up
SET GLOBAL log_error_services=DEFAULT;

SELECT @@global.log_error_services;
