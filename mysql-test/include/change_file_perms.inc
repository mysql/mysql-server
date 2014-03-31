# $file_perms need to be in octal base
# Provide a file name and the required new file permissions.
# The file's permissions will be changed as per the input file
# permissions.

--perl
use strict;
my $file_perms= $ENV{'_FILE_PERMS_'} or die "File permissions are not set";
my $input_file= $ENV{'_INPUT_FILE_'} or die "Input file is not set";
die ('Unable to find file \'$input_file\'') unless (-e $input_file);
chmod(oct($file_perms), $input_file) or die ('Unable to set file permissions for file \'$input_file\'');
EOF
