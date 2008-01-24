# To use this convenience library in a trigger, simply require it at
# at the top of the script.  For example:
#
# #! /usr/bin/perl
#
# use FindBin;
# require "$FindBin::Bin/triggers-lib.pl";
#
# FindBin is needed, because sometimes a trigger is called from the
# RESYNC directory, and the trigger dir is ../BitKeeper/triggers

use strict;
use warnings;

use Carp;
use FindBin;


my $mysql_version = "5.1";

# These addresses must be kept current in all MySQL versions.
# See the wiki page InnoDBandOracle.
#my @innodb_to_email = ('dev_innodb_ww@oracle.com');
#my @innodb_cc_email = ('dev-innodb@mysql.com');
# FIXME: Keep this for testing; remove it once it's been used for a
# week or two.
my @innodb_to_email = ('tim@mysql.com');
my @innodb_cc_email = ();

# This is for MySQL >= 5.1.  Regex which defines the InnoDB files
# which should generally not be touched by MySQL developers.
my $innodb_files_description = <<EOF;
  storage/innobase/*
  mysql-test/t/innodb*    (except mysql-test/t/innodb_mysql*)
  mysql-test/r/innodb*    (except mysql-test/r/innodb_mysql*)
EOF
my $innodb_files_regex = qr{
  ^
  (
  # Case 1: innobase/*
  storage/innobase/
  |
  # Case 2: mysql-test/[tr]/innodb* (except innodb_mysql*)
  mysql-test/(t|r)/SCCS/s.innodb
    # The mysql-test/[tr]/innodb_mysql* are OK to edit
    (?!_mysql)
  )
}x;


# See 'bk help log', and the format of, e.g., $BK_PENDING.
# Important: this already contains the terminating newline!
my $file_rev_dspec = ':SFILE:|:REV:\n';

my $bktmp = "$FindBin::Bin/../tmp";

my $sendmail;
foreach ('/usr/sbin/sendmail', 'sendmail') {
  $sendmail = $_;
  last if -x $sendmail;
}
my $from = $ENV{REAL_EMAIL} || $ENV{USER} . '@mysql.com';


# close_or_warn
#   $fh  file handle to be closed
#   $description  description of the file handle
#   RETURN  Return value of close($fh)
#
# Print a nice warning message if close() isn't successful.  See
# perldoc perlvar and perldoc -f close for details.

sub close_or_warn (*$)
{
  my ($fh, $description) = @_;

  my $status = close $fh;
  if (not $status) {
    warn "$0: error on close of '$description': ",
         ($! ? "$!" : "exit status " . ($? >> 8)), "\n";
  }

  return $status;
}


# check_status
#   $warn  If true, warn about bad status
#   RETURN  TRUE, if $BK_STATUS is "OK"; FALSE otherwise
#
# Also checks the undocumented $BK_COMMIT env variable

sub check_status
{
  my ($warn) = @_;

  my $status = (grep { defined $_ }
                     $ENV{BK_STATUS}, $ENV{BK_COMMIT}, '<undef>')[0];

  unless ($status eq 'OK')
  {
    warn "Bad BK_STATUS '$status'\n" if $warn;
    return undef;
  }

  return 1;
}


# repository_location
#
# RETURN  ('HOST', 'ROOT') for the repository being modified

sub repository_location
{
  if ($ENV{BK_SIDE} eq 'client') {
    return ($ENV{BK_HOST}, $ENV{BK_ROOT});
  } else {
    return ($ENV{BKD_HOST}, $ENV{BKD_ROOT});
  }
}


# repository_type
# RETURN:
#   'main' for repo on bk-internal with post-incoming.bugdb trigger
#   'team' for repo on bk-internal with post-incoming.queuepush.pl trigger
#   'local' otherwise
#
# This definition may need to be modified if the host name or triggers change.

sub repository_type
{
  my ($host, $root) = repository_location();

  return 'local'
    unless uc($host) eq 'BK-INTERNAL.MYSQL.COM'
           and -e "$root/BitKeeper/triggers/post-incoming.queuepush.pl";

  return 'main' if -e "$root/BitKeeper/triggers/post-incoming.bugdb";

  return 'team';
}


# latest_cset
#   RETURN  Key for most recent ChangeSet

sub latest_cset {
  chomp(my $retval = `bk changes -r+ -k`);
  return $retval;
}


# read_bk_csetlist
#   RETURN  list of cset keys from $BK_CSETLIST file
sub read_bk_csetlist
{
  die "$0: script error: \$BK_CSETLIST not set\n"
    unless defined $ENV{BK_CSETLIST};

  open CSETS, '<', $ENV{BK_CSETLIST}
    or die "$0: can't read \$BK_CSETLIST='$ENV{BK_CSETLIST}': $!\n";
  chomp(my @csets = <CSETS>);
  close_or_warn(CSETS, "\$BK_CSETLIST='$ENV{BK_CSETLIST}'");

  return @csets;
}


# innodb_get_changes
#   $type   'file' or 'cset'
#   $value  file name (e.g., $BK_PENDING) or ChangeSet key
#   $want_merge_changes  flag; if false, merge changes will be ignored
#   RETURN  A string describing the InnoDB changes, or undef if no changes
#
# The return value does *not* include ChangeSet comments, only per-file
# comments.

sub innodb_get_changes
{
  my ($type, $value, $want_merge_changes) = @_;

  if ($type eq 'file')
  {
    open CHANGES, '<', $value
      or die "$0: can't read '$value': $!\n";
  }
  elsif ($type eq 'cset')
  {
    open CHANGES, '-|', "bk changes -r'$value' -v -d'$file_rev_dspec'"
      or die "$0: can't exec 'bk changes': $!\n";
  }
  else
  {
    croak "$0: script error: invalid type '$type'";
  }

  my @changes = grep { /$innodb_files_regex/ } <CHANGES>;

  close_or_warn(CHANGES, "($type, '$value')");

  return undef unless @changes;


  # Set up a pipeline of 'bk log' commands to weed out unwanted deltas.  We
  # never want deltas which contain no actual changes.  We may not want deltas
  # which are merges.

  my @filters;

  # This tests if :LI: (lines inserted) or :LD: (lines deleted) is
  # non-zero.  That is, did this delta change the file contents?
  push @filters,
    "bk log -d'"
    . "\$if(:LI: -gt 0){$file_rev_dspec}"
    . "\$if(:LI: -eq 0){\$if(:LD: -gt 0){$file_rev_dspec}}"
    . "' -";

  push @filters, "bk log -d'\$unless(:MERGE:){$file_rev_dspec}' -"
    unless $want_merge_changes;

  my $tmpname = "$bktmp/ibchanges.txt";
  my $pipeline = join(' | ', @filters) . " > $tmpname";
  open TMP, '|-', $pipeline
      or die "$0: can't exec [[$pipeline]]: $!\n";

  print TMP @changes;
  close_or_warn(TMP, "| $pipeline");

  # Use bk log to describe the changes
  open LOG, "bk log - < $tmpname |"
    or die "$0: can't exec 'bk log - < $tmpname': $!\n";
  my @log = <LOG>;
  close_or_warn(LOG, "bk log - < $tmpname |");

  unlink $tmpname;

  return undef unless @log;

  return join('', @log);
}


# Ask user if they really want to commit.
#   RETURN  TRUE = YES, commit; FALSE = NO, do not commit

sub innodb_inform_and_query_user
{
  my ($description) = @_;

  my $tmpname = "$bktmp/ibquery.txt";

  open MESSAGE, "> $tmpname"
    or die "$0: can't write message to '$tmpname': $!";

  print MESSAGE <<EOF;
This ChangeSet modifies some files which should normally be changed by
InnoDB developers only.  In general, MySQL developers should not change:

$innodb_files_description
The following InnoDB files were modified:
=========================================================
$description
=========================================================

If you understand this, you may Commit these changes.  The changes
will be sent to the InnoDB developers at @{[join ', ', @innodb_to_email]},
CC @{[join ', ', @innodb_cc_email]}.
EOF

  close_or_warn(MESSAGE, "$tmpname");

  my $status = system('bk', 'prompt', '-w',
      '-yCommit these changes', '-nDo not Commit', "-f$tmpname");

  unlink $tmpname;

  return ($status == 0 ? 1 : undef);
}


# innodb_send_changes_email
#   $cset  The ChangeSet key
#   $description  A (maybe brief) description of the changes
#   RETURN  TRUE = Success, e-mail sent; FALSE = Failure
#
# Sends a complete diff of changes in $cset by e-mail.

sub innodb_send_changes_email
{
  my ($cset, $description) = @_;

  # FIXME: Much of this is duplicated in the 'post-commit' Bourne shell
  # trigger

  my $cset_short = `bk changes -r'$cset' -d':P:::I:'`;
  my $cset_key = `bk changes -r'$cset' -d':KEY:'`;

  my ($host, $bk_root) = repository_location();
  my $type = repository_type();
  (my $treename = $bk_root) =~ s,^.*/,,;

  print "Nofifying InnoDB developers at ",
        (join ', ', @innodb_to_email, @innodb_cc_email), "\n";

  open SENDMAIL, '|-', "$sendmail -t"
    or die "Can't exec '$sendmail -t': $!\n";

  my @headers;
  push @headers, "List-ID: <bk.innodb-$mysql_version>";
  push @headers, "From: $from";
  push @headers, "To: " . (join ', ', @innodb_to_email);
  push @headers, "Cc: " . (join ', ', @innodb_cc_email) if @innodb_cc_email;
  push @headers,
       "Subject: InnoDB changes in $type $mysql_version tree ($cset_short)";
  push @headers, "X-CSetKey: <$cset_key>";

  print SENDMAIL map { "$_\n" } @headers, '';

  if ($type eq 'main')
  {
    print SENDMAIL <<EOF;
Changes pushed to $treename by $ENV{USER} affect the following
files.  These changes are in a $mysql_version main tree.  They
will be available publicly within 24 hours.
EOF
  }
  elsif ($type eq 'team')
  {
    print SENDMAIL <<EOF;
Changes added to $treename by $ENV{USER} affect the
following files.  These changes are in a $mysql_version team tree.
EOF
  }
  else
  {
    print SENDMAIL <<EOF;
A local commit by $ENV{USER} affects the following files.  These
changes are in a clone of a $mysql_version tree.
EOF
  }
  print SENDMAIL "\n";
  print SENDMAIL qx(bk changes -r'$cset');
  print SENDMAIL "$description";
  print SENDMAIL "The complete ChangeSet diffs follow.\n\n";
  print SENDMAIL qx(bk rset -r'$cset' -ah | bk gnupatch -h -dup -T);

  close_or_warn(SENDMAIL, "$sendmail -t")
    or return undef;

  return 1;
}


1;
