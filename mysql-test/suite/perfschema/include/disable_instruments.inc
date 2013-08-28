# Tests for the performance schema

# ==========================================
# HELPER include/disable_instruments
# ==========================================
#
# ==== Usage ====
#
# 1. Define the instrument to be disabled/enabled:
#
#     let $pfs_instrument=<instrument>
#
#   Where <instrument> is the instrument name with or without wildcards, e.g.
#
#     let $pfs_instrument='%statement/sql%'
#
--disable_query_log ONCE

eval update performance_schema.setup_instruments
  set enabled='no', timed='no'
  where name like $pfs_instrument;


