# suite/funcs_1/datadict/charset_collation.inc
#
# Tests checking the content of the information_schema tables
#      character_sets
#      collations
#      collation_character_set_applicability
#
#
# The amount and properties of character_sets/collations depend on the
# build type
# 2007-12 MySQL 5.0, 2008-06 MySQL 5.1
# ---------------------------------------------------------------------
#
# Variant 1 fits to
#    version_comment       MySQL Enterprise Server (Commercial)
#    version_comment       MySQL Enterprise Server (GPL)
#    version_comment       MySQL Classic Server (Commercial)
#    version_comment       MySQL Pushbuild Edition, build <number>
#   (version_comment       Source distribution
#    and
#    compile was without "max" - > no collation 'utf8_general_ci')
#
# Variant 2 fits to
#    version_comment       MySQL Enterprise Server (GPL)
#    version_comment       MySQL Classic Server (Commercial)
#    version_comment       MySQL Pushbuild Edition, build <number>
#   (version_comment       Source distribution
#    and
#    compile was without "max" - > collation 'utf8_general_ci' exists)
#
# Difference between variant 1 and 2 is the collation 'utf8_general_ci'.
#
# Variant 3 fits to
#    version_comment       MySQL Community Server (GPL)
#    version_comment       MySQL Cluster Server (Commercial)
#    version_comment       MySQL Advanced Server (GPL)         5.1
#    version_comment       MySQL Advanced Server (Commercial)  5.1
#
# Difference between variant 3 and 2 is within the collation properties
# IS_COMPILED and SORTLEN.
#
# 2008-06 All time excluded variant is "vanilla".
# How to build "vanilla":
#    ./BUILD/autorun.sh
#    ./configure
#    ./make
# Some properties of "vanilla"
#    version_comment       Source distribution
#    Compared to the variants 1 to 3 a lot of character sets are missing.
#    Example: "ucs2_bin" is in variant 1 to 3 but not in "vanilla".
#
# Created:
# 2007-12-18 mleich - remove the unstable character_set/collation subtests
#                     from include/datadict-master.inc
#                   - create this new test
#

# Create a low privileged user.
--error 0, ER_CANNOT_USER
DROP USER dbdict_test@localhost;
CREATE USER dbdict_test@localhost;

--echo # Establish connection con (user=dbdict_test)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (con,localhost,dbdict_test,,);
################################################################################
#
# The original requirements for the following tests were:
#
# 3.2.2.2: Ensure that the table (information_schema.character_sets) shows the
#          relevant information on every character set for which the current
#          user or PUBLIC have the USAGE privilege.
#
# 3.2.2.3: Ensure that the table (information_schema.character_sets) does not
#          show any information on any character set for which the current user
#          or PUBLIC have no USAGE privilege.
#
#
# 3.2.3.2: Ensure that the table (information_schema.collations) shows the
#          relevant information on every collation for which the current user
#          or PUBLIC have the USAGE privilege.
#
# 3.2.3.3: Ensure that the table (information_schema.collations) does not show
#          any information on any collations for which the current user and
#          PUBLIC have no USAGE privilege.
#
#
# 3.2.4.2: Ensure that the table
#                information_schema.collation_character_set_applicability
#          shows the relevant information on every collation/character set
#          combination for which the current user or PUBLIC have the USAGE
#          privilege.
#
# 3.2.4.3: Ensure that the table
#                information_schema.collation_character_set_applicability
#          does not show any information on any collation/character set
#          combinations for which the current user and PUBLIC have no
#          USAGE privilege.
#
# Notes (2007-12-19 mleich):
# - The requirements are outdated because grant/revoke privilege for using a
#   characterset/collation were never implemented.
#   Therefore the tests should simply check the content of these tables.
#
# - The amount of collations/character sets grows with new MySQL releases.
#
# - Even within the same release the amount of records within these tables
#   can differ between different build types (community, enterprise, source,...)
#
#
################################################################################
--echo
SELECT *
FROM information_schema.character_sets
ORDER BY character_set_name;

--echo
SELECT *
FROM information_schema.collations
ORDER BY collation_name;

echo;
--echo
SELECT *
FROM information_schema.collation_character_set_applicability
ORDER BY collation_name, character_set_name;


# Cleanup
--echo # Switch to connection default + disconnect con
connection default;
disconnect con;
DROP USER dbdict_test@localhost;

