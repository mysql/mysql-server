# -*- cperl -*-

# Copyright (c) 2026, Amazon.com, Inc. or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#
# Manifest lifecycle: save a pre-existing manifest, write an MTR stub,
# and restore the original on exit.
#
# The marker file (<path>.mtr_stub) records that MTR owned the path
# when the current (or crashed-prior) run started.  At the next start
# a file beside a marker is MTR-owned: a leftover stub is unlinked,
# and anything else (e.g. a manifest written by an interrupted test)
# is quarantined to <path>.mtr_leftover with a warning rather than
# being restored into the installation.  The residual case — someone
# who deliberately overwrote MTR's leftover stub — finds their file
# in .mtr_leftover, not lost.
#
# On exit the stub is unlinked only if it still holds MTR's own text;
# a stub that changed during the run (e.g. a test that failed before
# its teardown) is quarantined to <path>.mtr_leftover at cleanup.
#
# Moving (renaming) the manifest to <path>.mtr_saved in the same
# directory is atomic, preserves mode and ownership, and needs only
# directory write permission, so a read-only manifest is handled
# without changing its permissions.
#
# A run holds an exclusive flock on <path>.mtr_lock from manifest_create
# until the original is restored, so a concurrent run sharing the same
# basedir is refused instead of clobbering the backup (flock is also
# available in Win32 Perl; mtr_unique.pm relies on it too).  The lock
# file is created mode 0666, like mtr_unique.pm's build-thread id files.
#
# Extracted from mysql-test-run.pl so the three-state restore logic
# can be unit-tested.
#

package My::Manifest;

use strict;
use warnings;
use Carp;
use Fcntl qw(:flock O_RDWR O_CREAT);

use base qw(Exporter);
our @EXPORT = qw(manifest_create manifest_restore manifest_reset
                 manifest_hand_off);

# ---- module-level state (one manifest per MTR process) ----------------

# Path to the backed-up pre-existing manifest, if one was preserved.
# Cleared after restore to make the operation idempotent.
my $preserved_manifest;

# True when MTR itself created the manifest stub (no pre-existing file).
# Only when this flag is set is it safe to unlink on cleanup.
my $manifest_stub_created;

# Handle holding the exclusive flock on <path>.mtr_lock.  The kernel
# drops the lock when the process dies, so a stale lock file is
# harmless.  The file itself is never unlinked: a new run could lock
# the old inode while another run creates and locks a new one.
my $lock_fh;

# True once servers started by --start-and-exit have been handed the
# manifest; manifest_restore then leaves everything for the next run.
my $handed_off = 0;

# ---- private helpers --------------------------------------------------

# _release_lock() — drop the ownership lock, if held.
sub _release_lock {
  return unless $lock_fh;
  close $lock_fh;
  undef $lock_fh;
}

# _acquire_lock($manifest_file_path) — take the ownership lock or die
# naming the run that holds it.  Records this pid in the lock file.
sub _acquire_lock {
  my ($manifest_file_path) = @_;
  return if $lock_fh;
  my $lock_path = $manifest_file_path . ".mtr_lock";
  # Shared-mode lock file, as mtr_unique.pm does for build-thread ids, so
  # sequential runs by different users in a group-writable directory can
  # reuse it.  The chmod is best-effort (only the owner can change it).
  my $old_umask = umask(0);
  my $opened = sysopen(my $fh, $lock_path, O_RDWR | O_CREAT, 0666);
  umask($old_umask);
  $opened or die "Could not open manifest lock $lock_path: $!";
  chmod 0666, $lock_path;
  if (!flock($fh, LOCK_EX | LOCK_NB)) {
    my $owner = '?';
    if (open(my $rh, '<', $lock_path)) {
      my $line = <$rh>;
      close $rh;
      $owner = $1 if defined $line && $line =~ /^(\d+)/;
    }
    close $fh;
    die "Another MTR run (pid $owner) is using $manifest_file_path; "
      . "run MTR runs that share a basedir one after another.\n";
  }
  # The pid is informational (named in the error above); best-effort.
  truncate($fh, 0);
  my $old = select($fh); $| = 1; select($old);
  print $fh "$$\n";
  $lock_fh = $fh;
}

# _content_matches($path, $content) — slurp the file and byte-compare.
# Dies on open failure: every caller is about to truncate or unlink
# the path, so an unreadable manifest must fail loudly.
sub _content_matches {
  my ($path, $content) = @_;
  open my $fh, '<', $path or die "Could not read manifest $path: $!";
  local $/;
  my $data = <$fh>;
  close $fh;
  return defined $data && $data eq $content;
}

# _write_file($path, $content) — create/truncate $path and write $content.
# Dies on any open/print/close failure.
sub _write_file {
  my ($path, $content) = @_;
  open(my $fh, '>', $path) or
    die "Could not create manifest file $path: $!";
  print $fh $content or
    die "Could not write manifest file $path: $!";
  close($fh) or die "Could not write manifest file $path: $!";
}

# ---- public API -------------------------------------------------------

# manifest_create($manifest_file_path, $config_content)
#
# Save a pre-existing manifest (if any) to <path>.mtr_saved, adopting
# an existing backup rather than clobbering it, then write the
# caller-supplied stub content.  A marker file (<path>.mtr_stub) is
# created alongside the stub so that a leftover stub from a killed run
# can be recognised without comparing file content.
sub manifest_create {
  my ($manifest_file_path, $config_content) = @_;
  croak "usage: manifest_create(<path>, <content>)"
    unless defined $manifest_file_path && defined $config_content;

  # Serialize ownership before any -e check: two runs that both saw
  # no backup would otherwise rename one's stub over the other's backup.
  _acquire_lock($manifest_file_path);

  my $backup = $manifest_file_path . ".mtr_saved";
  my $marker = $manifest_file_path . ".mtr_stub";

  if (-e $backup) {
    # A backup from a prior run that died before restoring already
    # holds the real manifest.  Adopt it only when the file at <path>
    # is absent or is MTR's own stub (left by the crashed run); never
    # silently prefer either candidate when both hold real content.
    if (-e $manifest_file_path &&
        !_content_matches($manifest_file_path, $config_content)) {
      die "Both $manifest_file_path and its backup $backup exist with "
        . "different content (a previous run was interrupted and a new "
        . "manifest was installed since).  Refusing to guess which one "
        . "is current; reconcile them and remove $backup before "
        . "re-running.\n";
    }
    $preserved_manifest = $backup;
    # Remove a stale marker if present.
    unlink $marker if -e $marker;
  } elsif (-e $manifest_file_path && -e $marker) {
    # Marker present ⇒ MTR owned this path when the interrupted run
    # started.  Whatever content is here now was written after MTR took
    # over (a leftover stub, an interrupted test, or someone who
    # overwrote the leftover).
    if (_content_matches($manifest_file_path, $config_content)) {
      # Leftover MTR stub — just re-use the path; unlink on restore.
      $manifest_stub_created = 1;
    } else {
      # Content differs from the stub (e.g. an interrupted keyring test
      # wrote a custom manifest).  Quarantine it so it is never restored
      # into the installation, but also never silently destroyed.
      # An existing .mtr_leftover is overwritten by rename (acceptable:
      # the previous leftover was equally MTR-era content).
      my $leftover = $manifest_file_path . ".mtr_leftover";
      rename($manifest_file_path, $leftover)
        or die "Could not move manifest $manifest_file_path "
             . "to $leftover: $!";
      warn "Manifest $manifest_file_path was written after MTR created "
         . "its stub (marker $marker present, e.g. by an interrupted "
         . "test run); moved to $leftover and not restored\n";
      $manifest_stub_created = 1;
    }
  } elsif (-e $manifest_file_path) {
    # No marker, no backup — a manifest installed while no MTR run was
    # active.  This is a user manifest; move it aside for restore.
    # A same-directory rename is atomic, preserves mode and ownership,
    # and needs only directory write permission.
    rename($manifest_file_path, $backup)
      or die "Could not move manifest $manifest_file_path to $backup: $!";
    $preserved_manifest = $backup;
  } else {
    $manifest_stub_created = 1;
  }

  # Drop the marker BEFORE writing the stub so that a crash between
  # the two leaves marker-without-stub (harmless: next run sees no
  # manifest and overwrites the marker).
  if ($manifest_stub_created) {
    open(my $mh, '>', $marker)
      or die "Could not create marker $marker: $!";
    close($mh)
      or die "Could not write marker $marker: $!";
  }

  _write_file($manifest_file_path, $config_content);
}

# manifest_restore($manifest_file_path, $config_content)
#
# Idempotent restore helper; never dies so the END-block safety net
# always runs to completion.  Three states:
#   1. $preserved_manifest set & file exists -> rename backup over manifest;
#      clear state only on success so the END-block can retry.
#   2. $manifest_stub_created true           -> unlink the MTR-written stub
#                                               only if it still holds MTR's
#                                               own text; a manifest changed
#                                               during the run is quarantined
#                                               to <path>.mtr_leftover.
#                                               An unreadable stub is reported
#                                               and kept for a later retry.
#                                               The marker is removed only
#                                               when the stub is handled.
#                                               Clear state only when
#                                               everything succeeded.
#   3. Neither                               -> true no-op (second call).
sub manifest_restore {
  my ($manifest_file_path, $config_content) = @_;
  croak "usage: manifest_restore(<path>, <content>)"
    unless defined $manifest_file_path && defined $config_content;
  # Servers detached by --start-and-exit keep reading the manifest.
  return if $handed_off;

  if (defined $preserved_manifest && -e $preserved_manifest) {
    # Same-directory rename: atomic, no copy fallback, so a failure leaves
    # the stub and backup intact for retry or next-run adoption.
    if (rename($preserved_manifest, $manifest_file_path)) {
      $preserved_manifest = undef;
    } else {
      warn "Could not restore manifest from $preserved_manifest: $!";
      # Leave $preserved_manifest set so the END-block safety net can retry.
    }
  } elsif ($manifest_stub_created) {
    # Stub-only: unlink both the stub and the marker; clear state only
    # when both are actually gone so the END-block safety net can retry.
    my $marker = $manifest_file_path . ".mtr_stub";
    my $ok = 1;
    if (-e $manifest_file_path) {
      my $matches = eval { _content_matches($manifest_file_path,
                                            $config_content) };
      if ($@) {
        warn "Could not read stub $manifest_file_path; "
           . "leaving it in place: $@";
        $ok = 0;
      } elsif ($matches) {
        unlink $manifest_file_path
          or do { warn "Could not unlink stub $manifest_file_path: $!";
                  $ok = 0 };
      } else {
        # Changed during the run (e.g. a test that failed before its
        # teardown).  Quarantine it now so the next run never adopts it
        # as a user manifest; keep the marker if that fails so the next
        # run's manifest_create quarantines it instead.
        my $leftover = $manifest_file_path . ".mtr_leftover";
        if (rename($manifest_file_path, $leftover)) {
          warn "Manifest $manifest_file_path changed during the run; "
             . "moved to $leftover\n";
        } else {
          warn "Manifest $manifest_file_path changed during the run and "
             . "could not be moved to $leftover: $!";
          $ok = 0;
        }
      }
    }
    if ($ok && -e $marker) {
      unlink $marker
        or do { warn "Could not unlink marker $marker: $!";
                $ok = 0 };
    }
    $manifest_stub_created = undef if $ok;
  }
  # Otherwise: nothing MTR owns to touch -- true no-op.

  # Hold the lock until the original is fully back in place.
  _release_lock()
    unless defined $preserved_manifest || $manifest_stub_created;
}

# manifest_hand_off()
#
# Record that servers left running by --start-and-exit now own the
# manifest: manifest_restore becomes a no-op and the backup stays
# adjacent for the next run to adopt.
sub manifest_hand_off { $handed_off = 1 }

# manifest_reset()
#
# Reset module state.  Intended for unit tests only.
sub manifest_reset {
  $preserved_manifest  = undef;
  $manifest_stub_created = undef;
  $handed_off = 0;
  _release_lock();
}

1;
