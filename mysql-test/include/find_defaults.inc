# ==== Purpose =====
#
# Check if the defaults file sets an option or not.
#
# The caller need to provide a regular expression for both the option name and
# expected value and also the groups that should be checked.
#
# The value of the last matching option is used when checking the value.
#
# ==== Usage ====
#
# --let $find_defaults_option= REGEX
# [--let $find_defaults_value= REGEX]
# --let $find_defaults_groups= TEXT
# [--let $find_defaults_group_suffix= TEXT]
# --source include/find_defaults.inc
# if ($find_defaults_status == 0) {
#   # found action
# }
# if ($find_defaults_status == 2) {
#   # not found action
# }
#
# Parameters:
#
#   $find_defaults_option= REGEX
#     The option(s) to check, typically a plain option name but could be a Perl
#     regex. Option name must use minus (-) not underscore (_).
#
#   $find_defaults_value= REGEX
#     A Perl refex to match value against. If not set matches against empty
#     value.
#
#   $find_defaults_groups= TEXT
#     A program typically check several groups, provide them space separated.
#
#   $find_defaults_group_suffix= TEXT
#     In case group suffix is relevant set that too, for example .1 .
#
# Return:
#
#   $find_defaults_status
#     If there is a match for option and value in groups status is set to 0
#     otherwise it is set to 2.

if ($find_defaults_option == '') {
  die ERROR: $find_defaults_option must be set;
}

if ($find_defaults_groups == '') {
  die ERROR: $find_defaults_groups must be set;
}

let _FIND_DEFAULTS_OPTION=$find_defaults_option;
let _FIND_DEFAULTS_VALUE=$find_defaults_value;
let _FIND_DEFAULTS_GROUPS=$find_defaults_groups;
let _FIND_DEFAULTS_GROUP_SUFFIX=$find_defaults_group_suffix;
error 0,2; # 1 is used on Windows when not finding perl
perl;
  use strict;
  use warnings;
  my $cmd= "$ENV{MYSQL_MY_PRINT_DEFAULTS}" .
           " --defaults-file=$ENV{PATH_CONFIG_FILE}" .
           " --defaults-group-suffix=$ENV{_FIND_DEFAULTS_GROUP_SUFFIX}" .
           " $ENV{_FIND_DEFAULTS_GROUPS}";
  my $found= 0;
  my $option= $ENV{_FIND_DEFAULTS_OPTION};
  my $value= $ENV{_FIND_DEFAULTS_VALUE};
  open(my $fh, "-|", $cmd) || die "Can't open pipeline to: $cmd";
    while (<$fh>) {
      if ($_ =~ /^--$option(|=(.*))$/im) {
        $found = ($2 =~ /^$value$/im);
      } elsif ($_ =~ /^--(skip-|disable-)$option$/im) {
        $found = 0;
      }
    }
  close($fh) || die "Can't close pipeline to: $cmd";
  if ($found == 0) { exit 2; }
EOF
let $find_defaults_status=$__error;
let $find_defaults_option=;
let $find_defaults_value=;
let $find_defaults_groups=;
let $find_defaults_group_suffix=;
