--let $support_xa = 1
if (!`SELECT xa='YES' FROM INFORMATION_SCHEMA.ENGINES WHERE engine = '$storage_engine'`)
{
  --echo # -- WARNING ----------------------------------------------------------------
  --echo # According to I_S.ENGINES, $storage_engine does not support XA.
  --echo # If it is true, the test will most likely fail; you can 
  --echo # either create an rdiff file, or add the test to disabled.def.
  --echo # If XA should be supported, check the data in Information Schema.
  --echo # ---------------------------------------------------------------------------
  --let $support_xa = 0
}

