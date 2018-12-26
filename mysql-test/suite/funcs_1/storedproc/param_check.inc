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
# Modified:
# 2008-11-17 pcrews
# added --disable / --enable_warning statements to minimize differences
# between platforms (Bug#40177 Test funcs_1.storedproc failing on Pushbuild)
#
# TODO:  (After 5.1 GA)
# 1)  Examine reordering statements in this file to minimize the number of
#     --disable / --enable_warning statements.  Unsure if performance gains
#     warrant the working time
# 2)  We could probably add a comparison of the # of warnings before the
#     assignment of @v1_proc and @v1_func to the # of warnings after assignment
#     The difference of these values should be zero
# Refer to Bug#40177 - http://bugs.mysql.com/bug.php?id=40177 for notes as well
 
eval UPDATE t1_aux SET f1 = NULL;
# Enforce that all user variables have the same data type and initial value.
SELECT f1,f1,f1,f1 INTO @v1_tab,@v1_proc,@v2_proc,@v1_func FROM t1_aux;

--disable_warnings
eval UPDATE t1_aux SET f1 = $test_value;
--enable_warnings
SELECT f1 INTO @v1_tab FROM t1_aux;
--disable_warnings
eval CALL sproc_1($test_value, @v1_proc);
eval SET @v1_func = func_1($test_value);
--enable_warnings

if (`SELECT @v1_tab <> @v1_proc OR @v1_tab <> @v2_proc OR @v1_tab <> @v1_func`)
{
   --echo Error: @v1_tab, @v1_proc, @v2_proc, @v1_func are not all equal
   SELECT @v1_tab, @v1_proc, @v2_proc, @v1_func;
}
