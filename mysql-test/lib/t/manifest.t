#!/usr/bin/perl
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

use strict;
use warnings 'FATAL';
use lib "lib";

use Fcntl ();
use File::Temp qw(tempdir);
use POSIX ();
use Test::More tests => 185;

BEGIN { use_ok("My::Manifest"); }

my $stub_content = '{ "read_local_manifest": true }';

# ---- helpers ----------------------------------------------------------

# Read the whole file and return its content.
sub slurp {
  my ($path) = @_;
  open my $fh, '<', $path or die "Cannot read $path: $!";
  local $/;
  my $data = <$fh>;
  close $fh;
  return $data;
}

# Write arbitrary content to a file.
sub spew {
  my ($path, $data) = @_;
  open my $fh, '>', $path or die "Cannot write $path: $!";
  print $fh $data;
  close $fh;
}

# ======================================================================
# TEST 1: No pre-existing manifest -> stub created, removed on restore;
#         marker exists during the run and is removed after restore
# ======================================================================
{
  my $dir  = tempdir(CLEANUP => 1);
  my $mf   = "$dir/mysqld.my";
  manifest_reset();

  ok(!-e $mf, "T1: manifest does not exist before create");
  manifest_create($mf, $stub_content);
  ok(-e $mf, "T1: stub created");
  is(slurp($mf), $stub_content, "T1: stub content matches");
  ok(!-e "$mf.mtr_saved", "T1: no backup created (nothing to back up)");
  ok(-e "$mf.mtr_stub", "T1: marker exists during the run");

  manifest_restore($mf, $stub_content);
  ok(!-e $mf, "T1: stub removed on restore");
  ok(!-e "$mf.mtr_stub", "T1: marker removed on restore");
}

# ======================================================================
# TEST 2: Pre-existing manifest -> moved aside, stub in place, original
#         byte-identical after restore
# ======================================================================
{
  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $original = "original-manifest-content-42\n";
  manifest_reset();

  spew($mf, $original);
  manifest_create($mf, $stub_content);

  ok(-e "$mf.mtr_saved", "T2: backup created");
  is(slurp("$mf.mtr_saved"), $original, "T2: backup is original content");
  is(slurp($mf), $stub_content, "T2: stub overwrote manifest");
  ok(!-e "$mf.mtr_stub", "T2: no marker (user manifest, not stub)");

  manifest_restore($mf, $stub_content);
  ok(-e $mf, "T2: manifest restored");
  is(slurp($mf), $original, "T2: restored content byte-identical");
  ok(!-e "$mf.mtr_saved", "T2: backup removed after restore");
}

# ======================================================================
# TEST 3: Pre-existing manifest AND stale .mtr_saved from crashed prior
#         run -> backup NOT clobbered, REAL manifest is what gets
#         restored (path holds MTR's own stub from the crashed run)
# ======================================================================
{
  my $dir         = tempdir(CLEANUP => 1);
  my $mf          = "$dir/mysqld.my";
  my $real_orig   = "real-original-from-first-run\n";
  manifest_reset();

  # Simulate a crashed prior run: .mtr_saved holds the real original,
  # the manifest file itself is MTR's stub from that crashed run.
  spew("$mf.mtr_saved", $real_orig);
  spew($mf, $stub_content);

  manifest_create($mf, $stub_content);

  # The existing backup must NOT have been overwritten.
  is(slurp("$mf.mtr_saved"), $real_orig,
     "T3: stale backup adopted, not clobbered");
  is(slurp($mf), $stub_content, "T3: new stub written");

  manifest_restore($mf, $stub_content);
  ok(-e $mf, "T3: manifest restored");
  is(slurp($mf), $real_orig,
     "T3: real original restored (not the stale stub)");
  ok(!-e "$mf.mtr_saved", "T3: backup removed after restore");
}

# ======================================================================
# TEST 3b: Stale marker cleaned up when backup exists (path is stub)
# ======================================================================
{
  my $dir = tempdir(CLEANUP => 1);
  my $mf  = "$dir/mysqld.my";
  manifest_reset();

  # Simulate: backup exists AND a stale marker from the same crash;
  # the file at <path> is MTR's own stub.
  spew("$mf.mtr_saved", "real-original\n");
  spew($mf, $stub_content);
  spew("$mf.mtr_stub", "");

  manifest_create($mf, $stub_content);
  ok(!-e "$mf.mtr_stub", "T3b: stale marker removed when backup exists");

  manifest_restore($mf, $stub_content);
  is(slurp($mf), "real-original\n", "T3b: real original restored");
}

# ======================================================================
# TEST 4: restore called twice -> second call is a no-op and does NOT
#         delete the restored manifest (double-unlink defect)
# ======================================================================
{
  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $original = "precious-manifest-data\n";
  manifest_reset();

  spew($mf, $original);
  manifest_create($mf, $stub_content);

  manifest_restore($mf, $stub_content);
  ok(-e $mf, "T4: manifest present after first restore");
  is(slurp($mf), $original, "T4: first restore correct");

  # Second restore must be a true no-op.
  manifest_restore($mf, $stub_content);
  ok(-e $mf, "T4: manifest still present after second restore");
  is(slurp($mf), $original,
     "T4: content unchanged after second restore (no double-unlink)");
}

# ======================================================================
# TEST 4b: restore called twice for stub-only case (no pre-existing) ->
#          second call is a no-op
# ======================================================================
{
  my $dir = tempdir(CLEANUP => 1);
  my $mf  = "$dir/mysqld.my";
  manifest_reset();

  manifest_create($mf, $stub_content);
  ok(-e $mf, "T4b: stub created");
  ok(-e "$mf.mtr_stub", "T4b: marker created");

  manifest_restore($mf, $stub_content);
  ok(!-e $mf, "T4b: stub removed after first restore");
  ok(!-e "$mf.mtr_stub", "T4b: marker removed after first restore");

  # Write something new to simulate server dropping a manifest
  spew($mf, "server-manifest\n");

  manifest_restore($mf, $stub_content);
  ok(-e $mf, "T4b: second restore is no-op, file untouched");
  is(slurp($mf), "server-manifest\n",
     "T4b: content unchanged after second restore");
}

# ======================================================================
# TEST 5: move failure -> dies BEFORE the original is lost
# ======================================================================
SKIP: {
  skip "chmod-based failure injection is unreliable as root", 4
    if $> == 0;

  my $dir = tempdir(CLEANUP => 1);
  my $mf  = "$dir/mysqld.my";
  my $original = "must-not-lose-this\n";
  manifest_reset();

  spew($mf, $original);
  # A prior run leaves the lock file behind; with it present the lock
  # needs no directory write, so the rename is what fails.
  spew("$mf.mtr_lock", "");

  # Make the directory read-only so rename (move) fails.
  chmod 0555, $dir;

  my $err = do { eval { manifest_create($mf, $stub_content) }; $@ };

  # Restore write permission for cleanup.
  chmod 0755, $dir;

  ok($err, "T5: manifest_create died on move failure");
  like($err // '', qr/Could not move manifest/,
       "T5: died with correct message");
  is(slurp($mf), $original,
     "T5: original not lost after move failure");
  ok(!-e "$mf.mtr_saved",
     "T5: no partial backup left behind");
}

# ======================================================================
# TEST 6: Leftover MTR stub from a crashed prior run (stub + marker
#          present, no backup) -> recognised by marker, unlinked on
#          restore, NOT backed up
# ======================================================================
{
  my $dir = tempdir(CLEANUP => 1);
  my $mf  = "$dir/mysqld.my";
  manifest_reset();

  # Simulate: stub and marker left behind, no backup.
  spew($mf, $stub_content);
  spew("$mf.mtr_stub", "");

  manifest_create($mf, $stub_content);
  ok(!-e "$mf.mtr_saved",
     "T6: no backup created for leftover stub");
  is(slurp($mf), $stub_content,
     "T6: stub content unchanged after create");

  manifest_restore($mf, $stub_content);
  ok(!-e $mf,
     "T6: leftover stub unlinked on restore");
  ok(!-e "$mf.mtr_stub",
     "T6: marker removed on restore");
}

# ======================================================================
# TEST 6b: File with DIFFERENT content (no backup, no marker) ->
#          still backed up (negative check for marker detection)
# ======================================================================
{
  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $original = "real-manifest-not-a-stub\n";
  manifest_reset();

  spew($mf, $original);

  manifest_create($mf, $stub_content);
  ok(-e "$mf.mtr_saved",
     "T6b: non-stub file is backed up");
  is(slurp("$mf.mtr_saved"), $original,
     "T6b: backup holds original content");
  is(slurp($mf), $stub_content,
     "T6b: stub overwrote manifest");

  manifest_restore($mf, $stub_content);
  is(slurp($mf), $original,
     "T6b: original restored from backup");
  ok(!-e "$mf.mtr_saved",
     "T6b: backup removed after restore");
}

# ======================================================================
# TEST 6c: File whose content EQUALS the stub text but with NO marker
#          -> backed up (.mtr_saved exists with that content) and
#          restored byte-identical.  This is the AutoSDE case: a user
#          manifest that happens to look like the stub must not be lost.
# ======================================================================
{
  my $dir = tempdir(CLEANUP => 1);
  my $mf  = "$dir/mysqld.my";
  manifest_reset();

  # Write a user manifest whose content is identical to the stub text.
  spew($mf, $stub_content);
  # No marker present — this is a user's file, not an MTR leftover.

  manifest_create($mf, $stub_content);
  ok(-e "$mf.mtr_saved",
     "T6c: stub-text user manifest is backed up (no marker)");
  is(slurp("$mf.mtr_saved"), $stub_content,
     "T6c: backup holds stub-text content");

  manifest_restore($mf, $stub_content);
  ok(-e $mf,
     "T6c: manifest restored");
  is(slurp($mf), $stub_content,
     "T6c: restored content byte-identical to stub text");
  ok(!-e "$mf.mtr_saved",
     "T6c: backup removed after restore");
  ok(!-e "$mf.mtr_stub",
     "T6c: no marker left behind");
}

# ======================================================================
# TEST 6d': Marker present + file with DIFFERENT content (no backup)
#           -> after create: <path>.mtr_leftover holds that content,
#           no .mtr_saved, stub written, marker present; after restore:
#           path and marker gone, .mtr_leftover still present with the
#           content.  The marker proves MTR owned the path so the
#           content is quarantined, not restored.
# ======================================================================
{
  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $test_cfg = '{ "components": "file://component_keyring_file" }';
  manifest_reset();

  # Simulate: marker left from a prior killed run, path holds content
  # written after MTR took over (e.g. by an interrupted test).
  spew($mf, $test_cfg);
  spew("$mf.mtr_stub", "");

  my @warnings;
  local $SIG{__WARN__} = sub { push @warnings, $_[0] };
  manifest_create($mf, $stub_content);
  ok(-e "$mf.mtr_leftover",
     "T6d': leftover file created");
  is(slurp("$mf.mtr_leftover"), $test_cfg,
     "T6d': leftover holds quarantined content");
  ok(!-e "$mf.mtr_saved",
     "T6d': no .mtr_saved (content is MTR-owned, not user)");
  is(slurp($mf), $stub_content,
     "T6d': stub written");
  ok(-e "$mf.mtr_stub",
     "T6d': marker present");
  ok(scalar @warnings > 0 && grep { /moved to/ } @warnings,
     "T6d': quarantine warning emitted");

  @warnings = ();
  manifest_restore($mf, $stub_content);
  ok(!-e $mf,
     "T6d': path gone after restore");
  ok(!-e "$mf.mtr_stub",
     "T6d': marker gone after restore");
  ok(-e "$mf.mtr_leftover",
     "T6d': .mtr_leftover still present after restore");
  is(slurp("$mf.mtr_leftover"), $test_cfg,
     "T6d': .mtr_leftover content unchanged after restore");
}

# ======================================================================
# TEST 6e: Interrupted-keyring simulation — marker + path with custom
#          content + <path>.backup holding the stub (as the keyring
#          helper leaves it: rename stub -> .backup, write custom,
#          then interrupted).  Same quarantine outcome, and the .backup
#          file is untouched by us.
# ======================================================================
{
  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $test_cfg = '{ "components": "file://component_keyring_file" }';
  manifest_reset();

  # Simulate: the keyring test helper renamed the stub to .backup and
  # wrote custom content to the path, then MTR was interrupted.
  spew($mf, $test_cfg);
  spew("$mf.mtr_stub", "");
  spew("$mf.backup", $stub_content);

  my @warnings;
  local $SIG{__WARN__} = sub { push @warnings, $_[0] };
  manifest_create($mf, $stub_content);
  ok(-e "$mf.mtr_leftover",
     "T6e: leftover file created");
  is(slurp("$mf.mtr_leftover"), $test_cfg,
     "T6e: leftover holds quarantined content");
  is(slurp($mf), $stub_content,
     "T6e: stub written");
  ok(-e "$mf.mtr_stub",
     "T6e: marker present");
  is(slurp("$mf.backup"), $stub_content,
     "T6e: .backup file untouched by manifest_create");

  @warnings = ();
  manifest_restore($mf, $stub_content);
  ok(!-e $mf,
     "T6e: path gone after restore");
  ok(!-e "$mf.mtr_stub",
     "T6e: marker gone after restore");
  ok(-e "$mf.mtr_leftover",
     "T6e: .mtr_leftover still present after restore");
  is(slurp("$mf.mtr_leftover"), $test_cfg,
     "T6e: .mtr_leftover content unchanged after restore");
  is(slurp("$mf.backup"), $stub_content,
     "T6e: .backup file untouched by manifest_restore");
}

# ======================================================================
# TEST 7b: Mode preservation — a read-only manifest (0444) keeps its
#          mode across backup and restore; inode preserved by rename
# ======================================================================
{
  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $original = "mode-preserved-content\n";
  manifest_reset();

  spew($mf, $original);
  chmod 0444, $mf;
  my $orig_ino = (stat($mf))[1];

  manifest_create($mf, $stub_content);
  ok(-e "$mf.mtr_saved", "T7b: backup created");
  ok(-e $mf, "T7b: stub written");
  is((stat("$mf.mtr_saved"))[2] & 07777, 0444,
     "T7b: backup carries original mode (0444)");
  # Inode should follow the rename.
  is((stat("$mf.mtr_saved"))[1], $orig_ino,
     "T7b: backup inode equals original inode (rename, not copy)");

  manifest_restore($mf, $stub_content);
  ok(-e $mf, "T7b: manifest restored");
  is((stat($mf))[2] & 07777, 0444,
     "T7b: restored manifest mode is 0444");
  is((stat($mf))[1], $orig_ino,
     "T7b: restored inode equals original inode");
  is(slurp($mf), $original,
     "T7b: restored content byte-identical");
  ok(!-e "$mf.mtr_saved",
     "T7b: backup removed after restore");
}

# ======================================================================
# TEST 8: Filename with whitespace — the three-arg open form handles
#          it correctly; the old two-arg form ("> $path") would
#          silently misinterpret the leading space.
# ======================================================================
{
  my $dir = tempdir(CLEANUP => 1);
  # Create a subdirectory whose name starts with a space.
  my $spacedir = "$dir/ spaced";
  mkdir $spacedir or die "mkdir: $!";
  my $mf = "$spacedir/mysqld.my";
  manifest_reset();

  manifest_create($mf, $stub_content);
  ok(-e $mf,
     "T8: manifest created at whitespace-containing path");
  is(slurp($mf), $stub_content,
     "T8: stub content written correctly to whitespace path");

  manifest_restore($mf, $stub_content);
  ok(!-e $mf,
     "T8: manifest removed on restore (stub-only path)");
}

# ======================================================================
# TEST 8a: backup exists + <path> == stub text → adopted, restore
#          yields backup content (covers the content==stub guard)
# ======================================================================
{
  my $dir       = tempdir(CLEANUP => 1);
  my $mf        = "$dir/mysqld.my";
  my $real_orig = "original-from-crashed-run\n";
  manifest_reset();

  # Path holds MTR's own stub (from a crashed run); backup holds the
  # real manifest.  No marker present.
  spew("$mf.mtr_saved", $real_orig);
  spew($mf, $stub_content);

  manifest_create($mf, $stub_content);
  is(slurp("$mf.mtr_saved"), $real_orig,
     "T8a: backup adopted (not clobbered)");
  is(slurp($mf), $stub_content,
     "T8a: stub written over previous stub");

  manifest_restore($mf, $stub_content);
  is(slurp($mf), $real_orig,
     "T8a: real original restored from backup");
  ok(!-e "$mf.mtr_saved",
     "T8a: backup removed after restore");
}

# ======================================================================
# TEST 8b: backup exists + <path> with DIFFERENT content → die with
#          message matching /different content/; both files untouched
# ======================================================================
{
  my $dir       = tempdir(CLEANUP => 1);
  my $mf        = "$dir/mysqld.my";
  my $real_orig = "original-manifest-from-old-run\n";
  my $new_user  = '{ "components": "file://new_keyring" }';
  manifest_reset();

  # Simulate: backup from a prior crash AND a newly installed manifest
  # with different content.
  spew("$mf.mtr_saved", $real_orig);
  spew($mf, $new_user);

  my $err = do { eval { manifest_create($mf, $stub_content) }; $@ };

  ok($err, "T8b: manifest_create dies on conflicting backup");
  like($err // '', qr/different content/,
       "T8b: error message mentions 'different content'");
  # Both files must be byte-identical to what they were before.
  is(slurp($mf), $new_user,
     "T8b: manifest at path untouched after die");
  is(slurp("$mf.mtr_saved"), $real_orig,
     "T8b: backup untouched after die");
  ok(!-e "$mf.mtr_stub",
     "T8b: no stub marker written");
}

# ======================================================================
# TEST 8c: restore failure retry — unlink fails, state not cleared,
#          second restore after restoring dir perms removes both
# ======================================================================
SKIP: {
  skip "chmod-based failure injection is unreliable as root", 8
    if $> == 0;

  my $dir = tempdir(CLEANUP => 1);
  my $mf  = "$dir/mysqld.my";
  manifest_reset();

  # Create a stub normally (no pre-existing file).
  manifest_create($mf, $stub_content);
  ok(-e $mf, "T8c: stub created");
  ok(-e "$mf.mtr_stub", "T8c: marker created");

  # Make the directory read-only so unlink fails.
  chmod 0555, $dir;

  my @warnings;
  local $SIG{__WARN__} = sub { push @warnings, $_[0] };
  manifest_restore($mf, $stub_content);

  # Stub and marker must still exist (unlink failed).
  ok(-e $mf, "T8c: stub still present after failed unlink");
  ok(-e "$mf.mtr_stub", "T8c: marker still present after failed unlink");
  ok(scalar @warnings > 0, "T8c: warnings issued on unlink failure");

  # Restore write permission.
  chmod 0755, $dir;

  # Second restore (simulating END-block retry) must succeed now.
  @warnings = ();
  manifest_restore($mf, $stub_content);
  ok(!-e $mf, "T8c: stub removed on retry");
  ok(!-e "$mf.mtr_stub", "T8c: marker removed on retry");
  ok(scalar @warnings == 0, "T8c: no warnings on successful retry");
}

# ======================================================================
# TEST 9: restore failure keeps state — rename fails, the stub and
#         backup are left untouched, and the reference is NOT cleared
#         so the END-block safety net can retry the restore
# ======================================================================
SKIP: {
  skip "chmod-based failure injection is unreliable as root", 6
    if $> == 0;

  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $original = "move-retry-test-content\n";
  manifest_reset();

  spew($mf, $original);
  manifest_create($mf, $stub_content);
  ok(-e "$mf.mtr_saved", "T9: backup created");

  # Make directory read-only so the same-directory rename fails.
  chmod 0555, $dir;

  # First restore: rename fails, should warn but NOT clear the reference.
  my @warnings;
  local $SIG{__WARN__} = sub { push @warnings, $_[0] };
  manifest_restore($mf, $stub_content);
  chmod 0755, $dir;

  ok(scalar @warnings > 0, "T9: warn issued on rename failure");

  # rename has no copy fallback: neither file was touched.
  is(slurp($mf), $stub_content,
     "T9: manifest still holds the stub after failed rename");
  is(slurp("$mf.mtr_saved"), $original,
     "T9: backup still holds the original after failed rename");

  # Second restore (simulating END-block retry) must succeed now.
  manifest_restore($mf, $stub_content);
  is(slurp($mf), $original,
     "T9: retry after rename failure restores the original");
  ok(!-e "$mf.mtr_saved",
     "T9: backup removed after successful retry");
}

# ======================================================================
# TEST 9': restore failure and MTR exits before any retry — the next
#          run adopts the untouched backup (stub at path) without a
#          conflict die and restores the original
# ======================================================================
SKIP: {
  skip "chmod-based failure injection is unreliable as root", 8
    if $> == 0;

  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $original = "next-run-adoption-content\n";
  manifest_reset();

  spew($mf, $original);
  manifest_create($mf, $stub_content);
  ok(-e "$mf.mtr_saved", "T9': backup created");

  chmod 0555, $dir;
  my @warnings;
  local $SIG{__WARN__} = sub { push @warnings, $_[0] };
  manifest_restore($mf, $stub_content);
  ok(scalar @warnings > 0, "T9': warn issued on rename failure");

  # Simulate the next MTR run: fresh state, permissions back.
  manifest_reset();
  chmod 0755, $dir;
  is(slurp($mf), $stub_content,
     "T9': stub left at path by the failed run");

  my $err = do { eval { manifest_create($mf, $stub_content) }; $@ };
  ok(!$err, "T9': next run's manifest_create does not die on conflict");
  is(slurp($mf), $stub_content, "T9': next run wrote the stub");

  manifest_restore($mf, $stub_content);
  is(slurp($mf), $original,
     "T9': next run restores the original");
  ok(!-e "$mf.mtr_saved", "T9': backup removed after restore");
  ok(!-e "$mf.mtr_stub", "T9': no marker left behind");
}

# ======================================================================
# TEST 9a: Manifest replaced during the run (e.g. a keyring test that
#          failed before its teardown) — quarantined to .mtr_leftover
#          at cleanup, marker removed, nothing left at the path
# ======================================================================
{
  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $new_cfg  = '{ "components": "file://component_keyring_file" }';
  manifest_reset();

  # Create the stub normally (no pre-existing file).
  manifest_create($mf, $stub_content);
  ok(-e $mf, "T9a: stub created");
  ok(-e "$mf.mtr_stub", "T9a: marker created");

  # Simulate a mid-run install: overwrite the stub with different content.
  spew($mf, $new_cfg);

  my @warnings;
  local $SIG{__WARN__} = sub { push @warnings, $_[0] };
  manifest_restore($mf, $stub_content);

  ok(scalar @warnings > 0 && grep { /changed during the run/ } @warnings,
     "T9a: warning mentions 'changed during the run'");
  ok(grep({ /moved to \Q$mf.mtr_leftover\E/ } @warnings),
     "T9a: warning names the .mtr_leftover destination");
  ok(!-e $mf, "T9a: manifest path gone after restore");
  is(slurp("$mf.mtr_leftover"), $new_cfg,
     "T9a: .mtr_leftover holds the mid-run content");
  ok(!-e "$mf.mtr_stub", "T9a: marker removed");

  # Second restore must be a no-op (state cleared).
  manifest_restore($mf, $stub_content);
  ok(!-e $mf, "T9a: second restore is a no-op, path still absent");
  is(slurp("$mf.mtr_leftover"), $new_cfg,
     "T9a: second restore leaves .mtr_leftover unchanged");
}

# ======================================================================
# TEST 9a': Same as 9a, then another MTR run — the quarantined content
#           is never adopted as a user manifest: no .mtr_saved, stub
#           written, and the stub is unlinked again at cleanup
# ======================================================================
{
  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $new_cfg  = '{ "components": "file://component_keyring_file" }';
  manifest_reset();

  manifest_create($mf, $stub_content);
  ok(-e "$mf.mtr_stub", "T9a': marker created");
  spew($mf, $new_cfg);

  my @warnings;
  local $SIG{__WARN__} = sub { push @warnings, $_[0] };
  manifest_restore($mf, $stub_content);
  is(slurp("$mf.mtr_leftover"), $new_cfg,
     "T9a': changed stub quarantined at cleanup");

  # Next MTR run.
  manifest_reset();
  @warnings = ();
  manifest_create($mf, $stub_content);
  ok(!-e "$mf.mtr_saved",
     "T9a': next run creates no .mtr_saved (nothing adopted)");
  is(slurp($mf), $stub_content, "T9a': next run wrote the stub");
  ok(-e "$mf.mtr_stub", "T9a': next run dropped a marker");

  manifest_restore($mf, $stub_content);
  ok(!-e $mf, "T9a': next run's stub unlinked at cleanup");
  ok(!-e "$mf.mtr_stub", "T9a': next run's marker removed");
  is(slurp("$mf.mtr_leftover"), $new_cfg,
     "T9a': .mtr_leftover untouched by the next run");
}

# ======================================================================
# TEST 9b: Unreadable manifest in the conflict guard — accurate error
#          message (not misreported as 'different content')
# ======================================================================
SKIP: {
  skip "chmod-based failure injection is unreliable as root", 2
    if $> == 0;

  my $dir = tempdir(CLEANUP => 1);
  my $mf  = "$dir/mysqld.my";
  manifest_reset();

  # Set up: backup exists and manifest exists but is unreadable.
  spew("$mf.mtr_saved", "real-original\n");
  spew($mf, "some-content\n");
  chmod 0000, $mf;

  my $err = do { eval { manifest_create($mf, $stub_content) }; $@ };

  # Restore permissions for cleanup.
  chmod 0644, $mf;

  like($err // '', qr/Could not read manifest/,
       "T9b: error mentions 'Could not read manifest'");
  unlike($err // '', qr/different content/,
         "T9b: error does NOT mention 'different content'");
}

# ======================================================================
# TEST 9c: Unreadable stub at restore — reported and kept for retry;
#          a second restore after restoring permissions removes it
# ======================================================================
SKIP: {
  skip "chmod-based failure injection is unreliable as root", 9
    if $> == 0;

  my $dir = tempdir(CLEANUP => 1);
  my $mf  = "$dir/mysqld.my";
  manifest_reset();

  # Create a stub normally (no pre-existing file).
  manifest_create($mf, $stub_content);
  ok(-e $mf, "T9c: stub created");
  ok(-e "$mf.mtr_stub", "T9c: marker created");

  # Make the stub unreadable.
  chmod 0000, $mf;

  my @warnings;
  local $SIG{__WARN__} = sub { push @warnings, $_[0] };
  manifest_restore($mf, $stub_content);

  ok(-e $mf, "T9c: stub still present (unreadable, kept)");
  ok(-e "$mf.mtr_stub", "T9c: marker still present (state kept for retry)");
  ok(scalar @warnings > 0 && grep { /Could not read stub/ } @warnings,
     "T9c: warning mentions 'Could not read stub'");

  # Restore permissions so the retry can read and remove the stub.
  chmod 0644, $mf;

  @warnings = ();
  manifest_restore($mf, $stub_content);
  ok(!-e $mf, "T9c: stub removed on retry after chmod");
  ok(!-e "$mf.mtr_stub", "T9c: marker removed on retry");
  ok(scalar @warnings == 0, "T9c: no warnings on successful retry");

  # Third restore is a true no-op.
  manifest_restore($mf, $stub_content);
  ok(!-e $mf, "T9c: third restore is a no-op");
}

# ======================================================================
# TEST 10: Stub write fails after the user manifest was moved aside —
#          manifest_create dies, and manifest_restore (the END-block
#          path) brings the original back byte-identical; a later run
#          starts cleanly with no conflict
# ======================================================================
{
  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $original = "stub-write-failure-test-content\n";
  manifest_reset();

  spew($mf, $original);

  my $err = do {
    no warnings 'redefine';
    local *My::Manifest::_write_file = sub { die "injected\n" };
    eval { manifest_create($mf, $stub_content) };
    $@;
  };
  is($err, "injected\n", "T10: manifest_create dies on stub-write failure");
  is(slurp("$mf.mtr_saved"), $original,
     "T10: .mtr_saved holds the original after the failure");

  # END-block safety net.
  manifest_restore($mf, $stub_content);
  is(slurp($mf), $original,
     "T10: restore brings the original back byte-identical");
  ok(!-e "$mf.mtr_saved", "T10: no .mtr_saved left after restore");
  ok(!-e "$mf.mtr_stub", "T10: no .mtr_stub left after restore");

  # A fresh run starts cleanly.
  manifest_reset();
  my $err2 = do { eval { manifest_create($mf, $stub_content) }; $@ };
  ok(!$err2, "T10: fresh manifest_create succeeds with no conflict");
  is(slurp($mf), $stub_content, "T10: fresh run wrote the stub");
  manifest_restore($mf, $stub_content);
  is(slurp($mf), $original, "T10: fresh run restores the original");
  ok(!-e "$mf.mtr_saved", "T10: fresh run leaves no .mtr_saved");
}

# True when no other open file description holds the ownership lock.
sub lock_is_free {
  my ($mf) = @_;
  open my $fh, '>>', "$mf.mtr_lock" or die "Cannot open $mf.mtr_lock: $!";
  my $free = flock($fh, Fcntl::LOCK_EX() | Fcntl::LOCK_NB());
  close $fh;
  return $free;
}

# ======================================================================
# TEST 11: Two concurrent runs sharing a manifest path — the second
#          dies before touching anything; the first restores the
#          original byte-identical
# ======================================================================
SKIP: {
  skip "fork-based concurrency test is not run on Windows", 7
    if $^O eq 'MSWin32';

  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $original = "concurrent-run-test-content\n";
  manifest_reset();

  spew($mf, $original);

  pipe(my $ready_r, my $ready_w) or die "pipe: $!";
  pipe(my $go_r, my $go_w) or die "pipe: $!";
  my $pid = fork();
  die "fork: $!" unless defined $pid;
  if ($pid == 0) {
    close $ready_r;
    close $go_w;
    my $rc = eval { manifest_create($mf, $stub_content); 1 } ? 0 : 1;
    syswrite($ready_w, $rc ? "fail\n" : "ok\n");
    <$go_r>;    # block until the parent has tried to take over
    manifest_restore($mf, $stub_content);
    POSIX::_exit($rc);
  }
  close $ready_w;
  close $go_r;
  my $line = <$ready_r>;
  is($line, "ok\n", "T11: first run created its stub");

  my $err = do { eval { manifest_create($mf, $stub_content) }; $@ };
  like($err // '', qr/Another MTR run \(pid \d+\)/,
       "T11: second run dies naming the owner");
  is(slurp("$mf.mtr_saved"), $original,
     "T11: .mtr_saved still holds the original");
  is(slurp($mf), $stub_content, "T11: path still holds the first run's stub");

  close $go_w;    # let the first run finish
  waitpid($pid, 0);
  is($?, 0, "T11: first run exited cleanly");
  is(slurp($mf), $original, "T11: first run restored the original");
  ok(!-e "$mf.mtr_saved", "T11: no .mtr_saved left");
  manifest_reset();
}

# ======================================================================
# TEST 11b: Stale lock — the owning run died without restoring; the
#           kernel released its lock, so the next run adopts the backup
# ======================================================================
SKIP: {
  skip "fork-based concurrency test is not run on Windows", 5
    if $^O eq 'MSWin32';

  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $original = "stale-lock-test-content\n";
  manifest_reset();

  spew($mf, $original);

  my $pid = fork();
  die "fork: $!" unless defined $pid;
  if ($pid == 0) {
    my $rc = eval { manifest_create($mf, $stub_content); 1 } ? 0 : 1;
    POSIX::_exit($rc);    # die without restoring
  }
  waitpid($pid, 0);
  is($?, 0, "T11b: crashed run had created its stub");

  my $err = do { eval { manifest_create($mf, $stub_content) }; $@ };
  ok(!$err, "T11b: next run takes over the stale lock");
  is(slurp($mf), $stub_content, "T11b: next run wrote the stub");
  manifest_restore($mf, $stub_content);
  is(slurp($mf), $original, "T11b: adopted backup restored");
  ok(!-e "$mf.mtr_saved", "T11b: no .mtr_saved left");
}

# ======================================================================
# TEST 12a: --start-and-exit setup fails before the hand-off (stub
#           write dies) — the restore still runs and releases the lock
# ======================================================================
{
  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $original = "failed-start-and-exit-content\n";
  manifest_reset();

  spew($mf, $original);

  my $err = do {
    no warnings 'redefine';
    local *My::Manifest::_write_file = sub { die "injected\n" };
    eval { manifest_create($mf, $stub_content) };
    $@;
  };
  is($err, "injected\n", "T12a: manifest_create dies on stub-write failure");

  # No manifest_hand_off(): nothing was detached.
  manifest_restore($mf, $stub_content);
  is(slurp($mf), $original, "T12a: original restored");
  ok(!-e "$mf.mtr_saved", "T12a: no .mtr_saved left");
  ok(lock_is_free($mf), "T12a: ownership lock released");
}

# ======================================================================
# TEST 12b: Successful --start-and-exit hand-off — restore leaves the
#           stub and backup for the detached servers; the next run
#           adopts the backup and restores the original
# ======================================================================
{
  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $original = "handed-off-content\n";
  manifest_reset();

  spew($mf, $original);

  manifest_create($mf, $stub_content);
  manifest_hand_off();
  manifest_restore($mf, $stub_content);
  is(slurp($mf), $stub_content, "T12b: stub left for the detached servers");
  is(slurp("$mf.mtr_saved"), $original, "T12b: .mtr_saved untouched");
  ok(!lock_is_free($mf), "T12b: lock held until the process exits");

  # The next run.
  manifest_reset();
  ok(lock_is_free($mf), "T12b: lock free once the run is gone");
  my $err = do { eval { manifest_create($mf, $stub_content) }; $@ };
  ok(!$err, "T12b: next run adopts the backup");
  is(slurp($mf), $stub_content, "T12b: next run wrote the stub");
  manifest_restore($mf, $stub_content);
  is(slurp($mf), $original, "T12b: next run restores the original");
  ok(!-e "$mf.mtr_saved", "T12b: no .mtr_saved left");
  ok(!-e "$mf.mtr_stub", "T12b: no .mtr_stub left");
}

# ======================================================================
# TEST 13: The lock file is created mode 0666 regardless of the umask
#          (as mtr_unique.pm does), so a later run by another user with
#          write access to the directory can open it
# ======================================================================
SKIP: {
  skip "POSIX file modes are not checked on Windows", 3
    if $^O eq 'MSWin32';

  my $dir = tempdir(CLEANUP => 1);
  my $mf  = "$dir/mysqld.my";
  manifest_reset();

  my $saved_umask = umask(022);
  manifest_create($mf, $stub_content);
  is(umask(), 022, "T13: caller's umask restored after manifest_create");
  umask($saved_umask);
  is((stat("$mf.mtr_lock"))[2] & 0777, 0666, "T13: lock file is mode 0666");
  manifest_restore($mf, $stub_content);
  is((stat("$mf.mtr_lock"))[2] & 0777, 0666,
     "T13: lock file kept, still 0666, after restore");
  manifest_reset();
}

# ======================================================================
# TEST 13b: A lock file left by an earlier run is reused.  Opening it
#           needs only read-write permission on the file, so a 0666
#           lock owned by another user is equally openable; here the
#           earlier run is our own (a test cannot switch users)
# ======================================================================
SKIP: {
  skip "POSIX file modes are not checked on Windows", 6
    if $^O eq 'MSWin32';

  my $dir      = tempdir(CLEANUP => 1);
  my $mf       = "$dir/mysqld.my";
  my $original = "reused-lock-content\n";
  manifest_reset();

  spew($mf, $original);
  spew("$mf.mtr_lock", "99999\n");
  chmod 0666, "$mf.mtr_lock";

  my $err = do { eval { manifest_create($mf, $stub_content) }; $@ };
  ok(!$err, "T13b: manifest_create reuses an existing 0666 lock");
  is(slurp("$mf.mtr_lock"), "$$\n", "T13b: lock now names this run");
  manifest_restore($mf, $stub_content);
  is(slurp($mf), $original, "T13b: original restored");
  manifest_reset();

  # A lock left 0644 by an older MTR is widened by its owner on reuse.
  chmod 0644, "$mf.mtr_lock";
  $err = do { eval { manifest_create($mf, $stub_content) }; $@ };
  ok(!$err, "T13b: manifest_create reuses an existing 0644 lock");
  is((stat("$mf.mtr_lock"))[2] & 0777, 0666, "T13b: owner widened it to 0666");
  manifest_restore($mf, $stub_content);
  is(slurp($mf), $original, "T13b: original restored again");
  manifest_reset();
}
