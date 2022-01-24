--let $required_icu_version=67
--source include/require_icu_version.inc

--echo #
--echo # Bug #33290090 regression:
--echo #               bool regexp::check_icu_status: Assertion `false' failed.
--echo #

# The metacharacter \X means 'Match a Grapheme Cluster.'
# ICU version 67 introduced a new implementation for '\X'
# This means that for bundled ICU, the query will fail with
# ER_REGEXP_MISSING_RESOURCE while for system ICU we return
# ER_WARN_REGEXP_USING_DEFAULT as a Note.

# For the query below:
#
# System ICU will say
# yesno
# 1
# Warnings:
# Note    4077    Regular expression library used default (root) locale.
#
# Bundled ICU will say
# ERROR 4076 (HY000): Missing locale resource in regular expression library.
#
# But you can do: export ICU_DATA=<path to ICU data>
# and if this path has icudt69l/brkitr/
# containing misc. .res and .brk files
# then bundled ICU will do the same as system ICU.

SELECT "hello hello" regexp '\\X' as yesno;
