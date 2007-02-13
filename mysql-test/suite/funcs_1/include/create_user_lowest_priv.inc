##### suite/funcs_1/include/create_user_no_priv.inc
#
# $new_user must contain the name (with @<host> if necessary)
#

--error 0,1396
eval DROP USER $new_user;
eval CREATE USER $new_user identified by 'PWD';
# Just to be sure
eval REVOKE ALL PRIVILEGES, GRANT OPTION FROM $new_user;
