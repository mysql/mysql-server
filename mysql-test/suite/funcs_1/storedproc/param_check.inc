# suite/funcs_1/storedproc/param_check.inc
#
# Auxiliary routine to be sourced by
# suite/funcs_1/t/storedproc.test
#
# Purpose:
#    The assignment of float values to objects of type DECIMAL causes
#    conversions and in some cases an adjustment of the value to
#    a border of the value range.
#    Try to reveal that function and procedure parameters get a similar
#    mangling of the value like columns.
#    
# Variables to be set before sourcing this routine
#    $test_value  - value to be checked
#
# Created:
# 2008-08-27 mleich
#

eval UPDATE t1_aux SET f1 = NULL;
# Enforce that all user variables have the same data type and initial value.
SELECT f1,f1,f1,f1 INTO @v1_tab,@v1_proc,@v2_proc,@v1_func FROM t1_aux;

eval UPDATE t1_aux SET f1 = $test_value;
SELECT f1 INTO @v1_tab FROM t1_aux;
eval CALL sproc_1($test_value, @v1_proc);
eval SET @v1_func = func_1($test_value);

if (`SELECT @v1_tab <> @v1_proc OR @v1_tab <> @v2_proc OR @v1_tab <> @v1_func`)
{
   --echo Error: @v1_tab, @v1_proc, @v2_proc, @v1_func are not all equal
   SELECT @v1_tab, @v1_proc, @v2_proc, @v1_func;
}
