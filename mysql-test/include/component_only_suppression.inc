if (!$component_only_suppression_pattern)
{
  --die Parameter 'component_only_suppression_pattern' is required.
}

if ($MTR_ROUTER_MRS_USE_COMPONENT)
{
  --disable_query_log
  --disable_result_log
  --eval CALL mtr.add_suppression("$component_only_suppression_pattern");
  --enable_result_log
  --enable_query_log
}

--let $component_only_suppression_pattern=
