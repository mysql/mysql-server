# Print the default value of $variable and try to set the value to default
# Inputs expected:
#   $variable: Name of the variable to be printed with SELECT @@<var>;
#   $is_global: If true, use SET GLOBAL <var>=DEFAULT;
#               If false, use SET SESSION <var>=DEFAULT;
#   $err:       Error to catch if SET <var>=DEFAULT will fail (for example, this
#               will fail if the variable is read-only

--eval SELECT @@$variable

if ($err) {
  --error $err
}
if ($is_global) {
  --eval SET GLOBAL $variable = DEFAULT
}
if (!$is_global) {
  --eval SET SESSION $variable = DEFAULT
}

--eval SELECT @@$variable
