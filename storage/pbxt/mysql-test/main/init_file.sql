#
# Normally, PBXT creates pbxt database automatically, on the first use.
# but if we let it to do so in mysql-test, then the after-test check
# will complain that the test did not restore the enviroment properly
# (did not delete 'pbxt' database, that was created during the test).
# One solution would be to patch all test files and add "drop database"
# at the end,
# Another one - use --init-file to create the database *before* the
# before-test check.
#
create database pbxt;
