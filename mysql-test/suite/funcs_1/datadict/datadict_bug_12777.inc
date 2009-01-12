#### --source suite/funcs_1/datadict/datadict_bug_12777.inc
#

# columns in INFORMATION_SCHEMA with VARCHAR(4096) on Linux and Intel or AMD
# processor are shown as VARCHAR(512) on Windows, VARCHAR(1023) on AIX and HPUX,
# VARCHAR(1024) on Solaris10, ... see below and in bug #12777 for details.
# So we need to replace the output for these systems. There may be other still
# not tested / detected systems.
#
# Setting the variables used below has been moved to the beginning of the datadict
# tests to "suite/funcs_1/datadict/datadict_load.inc".
#
# SELECT character_maximum_length INTO @CML
#   FROM information_schema.columns
#  WHERE table_schema = 'information_schema'
#    AND table_name   = 'columns'
#    AND column_name  = 'table_catalog';

# this enables the --replace_result only if needed, using this we never replace
# results on 'simple Linux' and so we will see any changes that might be
# suppressed by the - only on some systems used - replacements.


# Windows XP 32bit
if ($bug_12777_0512)
{
   # switch next 2 lines on for debugging the correct detection of the operating systems
   # let $message= value 512 detected - 1st replace statement activated!;
   # --source include/show_msg.inc
   #                nnnn       3*n
   --replace_result 512 4096  1536 12288
}

# aix52, aix52-64bit, hp3750, hp3750-64bit, hpux11, hpux11-64bit,
if ($bug_12777_1023)
{
   #                nnnn       3*n
   --replace_result 1023 4096  3069 12288
}

# Solaris10, 32bit
if ($bug_12777_1024)
{
   #                nnnn       3*n
   --replace_result 1024 4096  3072 12288
}

# Linux Suse 9.3 32bit Intel/AMD
if ($bug_12777_2048)
{
   #                nnnn       3*n
   --replace_result 2048 4096  6144 12288
}

# build-5.0-standard
if ($bug_12777_4095)
{
   #                nnnn       3*n
   --replace_result 4095 4096  12285 12288
}
