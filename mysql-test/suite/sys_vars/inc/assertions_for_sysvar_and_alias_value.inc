# ==== Purpose ====
#
# Like $assertions_for_sysvar_value, but check both a variable and its
# alias.
#
# ==== Usage ====
#
# --let $name = NAME
# --let $alias = ALIAS
# # other parameters, except $variable, are as for
# # assertions_for_sysvar_value.inc
# --source check_syvar_value_with_alias.inc


# As an optimization, we load the persisted variables file here, instead
# of in each invocation of check_sysvar_value.inc.
# Restore $persisted_json at the end of the script to not confuse the caller.
--let $_csaav_was_load = 0
if ($persisted_json == LOAD) {
  --let $_csaav_was_load = 1
  --source load_persisted_json.inc
}

--let $variable = $name
--source assertions_for_sysvar_value.inc
--let $variable = $alias
--source assertions_for_sysvar_value.inc

if ($_csaav_was_load) {
  --let $persisted_json = LOAD
}
