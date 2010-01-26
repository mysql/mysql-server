use strict;
use warnings FATAL => 'all';

package InnoDBParser;

# This program is copyright (c) 2006 Baron Schwartz, baron at xaprb dot com.
# Feedback and improvements are gratefully received.
#
# THIS PROGRAM IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
# MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, version 2; OR the Perl Artistic License.  On UNIX and similar
# systems, you can issue `man perlgpl' or `man perlartistic' to read these

# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 Temple
# Place, Suite 330, Boston, MA  02111-1307  USA

our $VERSION = '1.6.0';

use Data::Dumper;
$Data::Dumper::Sortkeys = 1;
use English qw(-no_match_vars);
use List::Util qw(max);

# Some common patterns
my $d  = qr/(\d+)/;                    # Digit
my $f  = qr/(\d+\.\d+)/;               # Float
my $t  = qr/(\d+ \d+)/;                # Transaction ID
my $i  = qr/((?:\d{1,3}\.){3}\d+)/;    # IP address
my $n  = qr/([^`\s]+)/;                # MySQL object name
my $w  = qr/(\w+)/;                    # Words
my $fl = qr/([\w\.\/]+) line $d/;      # Filename and line number
my $h  = qr/((?:0x)?[0-9a-f]*)/;       # Hex
my $s  = qr/(\d{6} .\d:\d\d:\d\d)/;    # InnoDB timestamp

# If you update this variable, also update the SYNOPSIS in the pod.
my %innodb_section_headers = (
   "TRANSACTIONS"                          => "tx",
   "BUFFER POOL AND MEMORY"                => "bp",
   "SEMAPHORES"                            => "sm",
   "LOG"                                   => "lg",
   "ROW OPERATIONS"                        => "ro",
   "INSERT BUFFER AND ADAPTIVE HASH INDEX" => "ib",
   "FILE I/O"                              => "io",
   "LATEST DETECTED DEADLOCK"              => "dl",
   "LATEST FOREIGN KEY ERROR"              => "fk",
);

my %parser_for = (
   tx => \&parse_tx_section,
   bp => \&parse_bp_section,
   sm => \&parse_sm_section,
   lg => \&parse_lg_section,
   ro => \&parse_ro_section,
   ib => \&parse_ib_section,
   io => \&parse_io_section,
   dl => \&parse_dl_section,
   fk => \&parse_fk_section,
);

my %fk_parser_for = (
   Transaction => \&parse_fk_transaction_error,
   Error       => \&parse_fk_bad_constraint_error,
   Cannot      => \&parse_fk_cant_drop_parent_error,
);

# A thread's proc_info can be at least 98 different things I've found in the
# source.  Fortunately, most of them begin with a gerunded verb.  These are
# the ones that don't.
my %is_proc_info = (
   'After create'                 => 1,
   'Execution of init_command'    => 1,
   'FULLTEXT initialization'      => 1,
   'Reopen tables'                => 1,
   'Repair done'                  => 1,
   'Repair with keycache'         => 1,
   'System lock'                  => 1,
   'Table lock'                   => 1,
   'Thread initialized'           => 1,
   'User lock'                    => 1,
   'copy to tmp table'            => 1,
   'discard_or_import_tablespace' => 1,
   'end'                          => 1,
   'got handler lock'             => 1,
   'got old table'                => 1,
   'init'                         => 1,
   'key cache'                    => 1,
   'locks'                        => 1,
   'malloc'                       => 1,
   'query end'                    => 1,
   'rename result table'          => 1,
   'rename'                       => 1,
   'setup'                        => 1,
   'statistics'                   => 1,
   'status'                       => 1,
   'table cache'                  => 1,
   'update'                       => 1,
);

sub new {
   bless {}, shift;
}

# Parse the status and return it.
# See srv_printf_innodb_monitor in innobase/srv/srv0srv.c
# Pass in the text to parse, whether to be in debugging mode, which sections
# to parse (hashref; if empty, parse all), and whether to parse full info from
# locks and such (probably shouldn't unless you need to).
sub parse_status_text {
   my ( $self, $fulltext, $debug, $sections, $full ) = @_;

   die "I can't parse undef" unless defined $fulltext;
   $fulltext =~ s/[\r\n]+/\n/g;

   $sections ||= {};
   die '$sections must be a hashref' unless ref($sections) eq 'HASH';

   my %innodb_data = (
      got_all   => 0,         # Whether I was able to get the whole thing
      ts        => '',        # Timestamp the server put on it
      last_secs => 0,         # Num seconds the averages are over
      sections  => {},        # Parsed values from each section
   );

   if ( $debug ) {
      $innodb_data{'fulltext'} = $fulltext;
   }

   # Get the most basic info about the status: beginning and end, and whether
   # I got the whole thing (if there has been a big deadlock and there are
   # too many locks to print, the output might be truncated)
   my ( $time_text ) = $fulltext =~ m/^$s INNODB MONITOR OUTPUT$/m;
   $innodb_data{'ts'} = [ parse_innodb_timestamp( $time_text ) ];
   $innodb_data{'timestring'} = ts_to_string($innodb_data{'ts'});
   ( $innodb_data{'last_secs'} ) = $fulltext
      =~ m/Per second averages calculated from the last $d seconds/;

   ( my $got_all ) = $fulltext =~ m/END OF INNODB MONITOR OUTPUT/;
   $innodb_data{'got_all'} = $got_all || 0;

   # Split it into sections.  Each section begins with
   # -----
   # LABEL
   # -----
   my %innodb_sections;
   my @matches = $fulltext
      =~ m#\n(---+)\n([A-Z /]+)\n\1\n(.*?)(?=\n(---+)\n[A-Z /]+\n\4\n|$)#gs;
   while ( my ( $start, $name, $text, $end ) = splice(@matches, 0, 4) ) {
      $innodb_sections{$name} = [ $text, $end ? 1 : 0 ];
   }
   # The Row Operations section is a special case, because instead of ending
   # with the beginning of another section, it ends with the end of the file.
   # So this section is complete if the entire file is complete.
   $innodb_sections{'ROW OPERATIONS'}->[1] ||= $innodb_data{'got_all'};

   # Just for sanity's sake, make sure I understand what to do with each
   # section
   eval {
      foreach my $section ( keys %innodb_sections ) {
         my $header = $innodb_section_headers{$section};
         die "Unknown section $section in $fulltext\n"
            unless $header;
         $innodb_data{'sections'}->{ $header }
            ->{'fulltext'} = $innodb_sections{$section}->[0];
         $innodb_data{'sections'}->{ $header }
            ->{'complete'} = $innodb_sections{$section}->[1];
      }
   };
   if ( $EVAL_ERROR ) {
      _debug( $debug, $EVAL_ERROR);
   }

   # ################################################################
   # Parse the detailed data out of the sections.
   # ################################################################
   eval {
      foreach my $section ( keys %parser_for ) {
         if ( defined $innodb_data{'sections'}->{$section}
               && (!%$sections || (defined($sections->{$section} && $sections->{$section})) )) {
            $parser_for{$section}->(
                  $innodb_data{'sections'}->{$section},
                  $innodb_data{'sections'}->{$section}->{'complete'},
                  $debug,
                  $full )
               or delete $innodb_data{'sections'}->{$section};
         }
         else {
            delete $innodb_data{'sections'}->{$section};
         }
      }
   };
   if ( $EVAL_ERROR ) {
      _debug( $debug, $EVAL_ERROR);
   }

   return \%innodb_data;
}

# Parses the status text and returns it flattened out as a single hash.
sub get_status_hash {
   my ( $self, $fulltext, $debug, $sections, $full ) = @_;

   # Parse the status text...
   my $innodb_status
      = $self->parse_status_text($fulltext, $debug, $sections, $full );

   # Flatten the hierarchical structure into a single list by grabbing desired
   # sections from it.
   return
      (map { 'IB_' . $_ => $innodb_status->{$_} } qw(timestring last_secs got_all)),
      (map { 'IB_bp_' . $_ => $innodb_status->{'sections'}->{'bp'}->{$_} }
         qw( writes_pending buf_pool_hit_rate total_mem_alloc buf_pool_reads
            awe_mem_alloc pages_modified writes_pending_lru page_creates_sec
            reads_pending pages_total buf_pool_hits writes_pending_single_page
            page_writes_sec pages_read pages_written page_reads_sec
            writes_pending_flush_list buf_pool_size add_pool_alloc
            dict_mem_alloc pages_created buf_free complete )),
      (map { 'IB_tx_' . $_ => $innodb_status->{'sections'}->{'tx'}->{$_} }
         qw( num_lock_structs history_list_len purge_done_for transactions
            purge_undo_for is_truncated trx_id_counter complete )),
      (map { 'IB_ib_' . $_ => $innodb_status->{'sections'}->{'ib'}->{$_} }
         qw( hash_table_size hash_searches_s non_hash_searches_s
            bufs_in_node_heap used_cells size free_list_len seg_size inserts
            merged_recs merges complete )),
      (map { 'IB_lg_' . $_ => $innodb_status->{'sections'}->{'lg'}->{$_} }
         qw( log_ios_done pending_chkp_writes last_chkp log_ios_s
            log_flushed_to log_seq_no pending_log_writes complete )),
      (map { 'IB_sm_' . $_ => $innodb_status->{'sections'}->{'sm'}->{$_} }
         qw( wait_array_size rw_shared_spins rw_excl_os_waits mutex_os_waits
            mutex_spin_rounds mutex_spin_waits rw_excl_spins rw_shared_os_waits
            waits signal_count reservation_count complete )),
      (map { 'IB_ro_' . $_ => $innodb_status->{'sections'}->{'ro'}->{$_} }
         qw( queries_in_queue n_reserved_extents main_thread_state
         main_thread_proc_no main_thread_id read_sec del_sec upd_sec ins_sec
         read_views_open num_rows_upd num_rows_ins num_rows_read
         queries_inside num_rows_del complete )),
      (map { 'IB_fk_' . $_ => $innodb_status->{'sections'}->{'fk'}->{$_} }
         qw( trigger parent_table child_index parent_index attempted_op
         child_db timestring fk_name records col_name reason txn parent_db
         type child_table parent_col complete )),
      (map { 'IB_io_' . $_ => $innodb_status->{'sections'}->{'io'}->{$_} }
         qw( pending_buffer_pool_flushes pending_pwrites pending_preads
         pending_normal_aio_reads fsyncs_s os_file_writes pending_sync_ios
         reads_s flush_type avg_bytes_s pending_ibuf_aio_reads writes_s
         threads os_file_reads pending_aio_writes pending_log_ios os_fsyncs
         pending_log_flushes complete )),
      (map { 'IB_dl_' . $_ => $innodb_status->{'sections'}->{'dl'}->{$_} }
         qw( timestring rolled_back txns complete ));

}

sub ts_to_string {
   my $parts = shift;
   return sprintf('%02d-%02d-%02d %02d:%02d:%02d', @$parts);
}

sub parse_innodb_timestamp {
   my $text = shift;
   my ( $y, $m, $d, $h, $i, $s )
      = $text =~ m/^(\d\d)(\d\d)(\d\d) +(\d+):(\d+):(\d+)$/;
   die("Can't get timestamp from $text\n") unless $y;
   $y += 2000;
   return ( $y, $m, $d, $h, $i, $s );
}

sub parse_fk_section {
   my ( $section, $complete, $debug, $full ) = @_;
   my $fulltext = $section->{'fulltext'};

   return 0 unless $fulltext;

   my ( $ts, $type ) = $fulltext =~ m/^$s\s+(\w+)/m;
   $section->{'ts'} = [ parse_innodb_timestamp( $ts ) ];
   $section->{'timestring'} = ts_to_string($section->{'ts'});
   $section->{'type'} = $type;

   # Decide which type of FK error happened, and dispatch to the right parser.
   if ( $type && $fk_parser_for{$type} ) {
      $fk_parser_for{$type}->( $section, $complete, $debug, $fulltext, $full );
   }

   delete $section->{'fulltext'} unless $debug;

   return 1;
}

sub parse_fk_cant_drop_parent_error {
   my ( $section, $complete, $debug, $fulltext, $full ) = @_;

   # Parse the parent/child table info out
   @{$section}{ qw(attempted_op parent_db parent_table) } = $fulltext
      =~ m{Cannot $w table `(.*)/(.*)`}m;
   @{$section}{ qw(child_db child_table) } = $fulltext
      =~ m{because it is referenced by `(.*)/(.*)`}m;

   ( $section->{'reason'} ) = $fulltext =~ m/(Cannot .*)/s;
   $section->{'reason'} =~ s/\n(?:InnoDB: )?/ /gm
      if $section->{'reason'};

   # Certain data may not be present.  Make them '' if not present.
   map { $section->{$_} ||= "" }
      qw(child_index fk_name col_name parent_col);
}

# See dict/dict0dict.c, function dict_foreign_error_report
# I don't care much about these.  There are lots of different messages, and
# they come from someone trying to create a foreign key, or similar
# statements.  They aren't indicative of some transaction trying to insert,
# delete or update data.  Sometimes it is possible to parse out a lot of
# information about the tables and indexes involved, but often the message
# contains the DDL string the user entered, which is way too much for this
# module to try to handle.
sub parse_fk_bad_constraint_error {
   my ( $section, $complete, $debug, $fulltext, $full ) = @_;

   # Parse the parent/child table and index info out
   @{$section}{ qw(child_db child_table) } = $fulltext
      =~ m{Error in foreign key constraint of table (.*)/(.*):$}m;
   $section->{'attempted_op'} = 'DDL';

   # FK name, parent info... if possible.
   @{$section}{ qw(fk_name col_name parent_db parent_table parent_col) }
      = $fulltext
      =~ m/CONSTRAINT `?$n`? FOREIGN KEY \(`?$n`?\) REFERENCES (?:`?$n`?\.)?`?$n`? \(`?$n`?\)/;

   if ( !defined($section->{'fk_name'}) ) {
      # Try to parse SQL a user might have typed in a CREATE statement or such
      @{$section}{ qw(col_name parent_db parent_table parent_col) }
         = $fulltext
         =~ m/FOREIGN\s+KEY\s*\(`?$n`?\)\s+REFERENCES\s+(?:`?$n`?\.)?`?$n`?\s*\(`?$n`?\)/i;
   }
   $section->{'parent_db'} ||= $section->{'child_db'};

   # Name of the child index (index in the same table where the FK is, see
   # definition of dict_foreign_struct in include/dict0mem.h, where it is
   # called foreign_index, as opposed to referenced_index which is in the
   # parent table.  This may not be possible to find.
   @{$section}{ qw(child_index) } = $fulltext
      =~ m/^The index in the foreign key in table is $n$/m;

   @{$section}{ qw(reason) } = $fulltext =~ m/:\s*([^:]+)(?= Constraint:|$)/ms;
   $section->{'reason'} =~ s/\s+/ /g
      if $section->{'reason'};
   
   # Certain data may not be present.  Make them '' if not present.
   map { $section->{$_} ||= "" }
      qw(child_index fk_name col_name parent_table parent_col);
}

# see source file row/row0ins.c
sub parse_fk_transaction_error {
   my ( $section, $complete, $debug, $fulltext, $full ) = @_;

   # Parse the txn info out
   my ( $txn ) = $fulltext
      =~ m/Transaction:\n(TRANSACTION.*)\nForeign key constraint fails/s;
   if ( $txn ) {
      $section->{'txn'} = parse_tx_text( $txn, $complete, $debug, $full );
   }

   # Parse the parent/child table and index info out.  There are two types: an
   # update or a delete of a parent record leaves a child orphaned
   # (row_ins_foreign_report_err), and an insert or update of a child record has
   # no matching parent record (row_ins_foreign_report_add_err).

   @{$section}{ qw(reason child_db child_table) }
      = $fulltext =~ m{^(Foreign key constraint fails for table `(.*)/(.*)`:)$}m;

   @{$section}{ qw(fk_name col_name parent_db parent_table parent_col) }
      = $fulltext
      =~ m/CONSTRAINT `$n` FOREIGN KEY \(`$n`\) REFERENCES (?:`$n`\.)?`$n` \(`$n`\)/;
   $section->{'parent_db'} ||= $section->{'child_db'};

   # Special case, which I don't know how to trigger, but see
   # innobase/row/row0ins.c row_ins_check_foreign_constraint
   if ( $fulltext =~ m/ibd file does not currently exist!/ ) {
      my ( $attempted_op, $index, $records )
         = $fulltext =~ m/^Trying to (add to index) `$n` tuple:\n(.*))?/sm;
      $section->{'child_index'} = $index;
      $section->{'attempted_op'} = $attempted_op || '';
      if ( $records && $full ) {
         ( $section->{'records'} )
            = parse_innodb_record_dump( $records, $complete, $debug );
      }
      @{$section}{qw(parent_db parent_table)}
         =~ m/^But the parent table `$n`\.`$n`$/m;
   }
   else {
      my ( $attempted_op, $which, $index )
         = $fulltext =~ m/^Trying to ([\w ]*) in (child|parent) table, in index `$n` tuple:$/m;
      if ( $which ) {
         $section->{$which . '_index'} = $index;
         $section->{'attempted_op'} = $attempted_op || '';

         # Parse out the related records in the other table.
         my ( $search_index, $records );
         if ( $which eq 'child' ) {
            ( $search_index, $records ) = $fulltext
               =~ m/^But in parent table [^,]*, in index `$n`,\nthe closest match we can find is record:\n(.*)/ms;
            $section->{'parent_index'} = $search_index;
         }
         else {
            ( $search_index, $records ) = $fulltext
               =~ m/^But in child table [^,]*, in index `$n`, (?:the record is not available|there is a record:\n(.*))?/ms;
            $section->{'child_index'} = $search_index;
         }
         if ( $records && $full ) {
            $section->{'records'}
               = parse_innodb_record_dump( $records, $complete, $debug );
         }
         else {
            $section->{'records'} = '';
         }
      }
   }

   # Parse out the tuple trying to be updated, deleted or inserted.
   my ( $trigger ) = $fulltext =~ m/^(DATA TUPLE: \d+ fields;\n.*)$/m;
   if ( $trigger ) {
      $section->{'trigger'} = parse_innodb_record_dump( $trigger, $complete, $debug );
   }

   # Certain data may not be present.  Make them '' if not present.
   map { $section->{$_} ||= "" }
      qw(child_index fk_name col_name parent_table parent_col);
}

# There are new-style and old-style record formats.  See rem/rem0rec.c
# TODO: write some tests for this
sub parse_innodb_record_dump {
   my ( $dump, $complete, $debug ) = @_;
   return undef unless $dump;

   my $result = {};

   if ( $dump =~ m/PHYSICAL RECORD/ ) {
      my $style = $dump =~ m/compact format/ ? 'new' : 'old';
      $result->{'style'} = $style;

      # This is a new-style record.
      if ( $style eq 'new' ) {
         @{$result}{qw( heap_no type num_fields info_bits )}
            = $dump
            =~ m/^(?:Record lock, heap no $d )?([A-Z ]+): n_fields $d; compact format; info bits $d$/m;
      }

      # OK, it's old-style.  Unfortunately there are variations here too.
      elsif ( $dump =~ m/-byte offs / ) {
         # Older-old style.
         @{$result}{qw( heap_no type num_fields byte_offset info_bits )}
            = $dump
            =~ m/^(?:Record lock, heap no $d )?([A-Z ]+): n_fields $d; $d-byte offs [A-Z]+; info bits $d$/m;
            if ( $dump !~ m/-byte offs TRUE/ ) {
               $result->{'byte_offset'} = 0;
            }
      }
      else {
         # Newer-old style.
         @{$result}{qw( heap_no type num_fields byte_offset info_bits )}
            = $dump
            =~ m/^(?:Record lock, heap no $d )?([A-Z ]+): n_fields $d; $d-byte offsets; info bits $d$/m;
      }

   }
   else {
      $result->{'style'} = 'tuple';
      @{$result}{qw( type num_fields )}
         = $dump =~ m/^(DATA TUPLE): $d fields;$/m;
   }

   # Fill in default values for things that couldn't be parsed.
   map { $result->{$_} ||= 0 }
      qw(heap_no num_fields byte_offset info_bits);
   map { $result->{$_} ||= '' }
      qw(style type );

   my @fields = $dump =~ m/ (\d+:.*?;?);(?=$| \d+:)/gm;
   $result->{'fields'} = [ map { parse_field($_, $complete, $debug ) } @fields ];

   return $result;
}

# New/old-style applies here.  See rem/rem0rec.c
# $text should not include the leading space or the second trailing semicolon.
sub parse_field {
   my ( $text, $complete, $debug ) = @_;

   # Sample fields:
   # '4: SQL NULL, size 4 '
   # '1: len 6; hex 000000005601; asc     V ;'
   # '6: SQL NULL'
   # '5: len 30; hex 687474703a2f2f7777772e737765657477617465722e636f6d2f73746f72; asc http://www.sweetwater.com/stor;...(truncated)'
   my ( $id, $nullsize, $len, $hex, $asc, $truncated );
   ( $id, $nullsize ) = $text =~ m/^$d: SQL NULL, size $d $/;
   if ( !defined($id) ) {
      ( $id ) = $text =~ m/^$d: SQL NULL$/;
   }
   if ( !defined($id) ) {
      ( $id, $len, $hex, $asc, $truncated )
         = $text =~ m/^$d: len $d; hex $h; asc (.*);(\.\.\.\(truncated\))?$/;
   }

   die "Could not parse this field: '$text'" unless defined $id;
   return {
      id    => $id,
      len   => defined($len) ? $len : defined($nullsize) ? $nullsize : 0,
      'hex' => defined($hex) ? $hex : '',
      asc   => defined($asc) ? $asc : '',
      trunc => $truncated ? 1 : 0,
   };

}

sub parse_dl_section {
   my ( $dl, $complete, $debug, $full ) = @_;
   return unless $dl;
   my $fulltext = $dl->{'fulltext'};
   return 0 unless $fulltext;

   my ( $ts ) = $fulltext =~ m/^$s$/m;
   return 0 unless $ts;

   $dl->{'ts'} = [ parse_innodb_timestamp( $ts ) ];
   $dl->{'timestring'} = ts_to_string($dl->{'ts'});
   $dl->{'txns'} = {};

   my @sections
      = $fulltext
      =~ m{
         ^\*{3}\s([^\n]*)  # *** (1) WAITING FOR THIS...
         (.*?)             # Followed by anything, non-greedy
         (?=(?:^\*{3})|\z) # Followed by another three stars or EOF
      }gmsx;


   # Loop through each section.  There are no assumptions about how many
   # there are, who holds and wants what locks, and who gets rolled back.
   while ( my ($header, $body) = splice(@sections, 0, 2) ) {
      my ( $txn_id, $what ) = $header =~ m/^\($d\) (.*):$/;
      next unless $txn_id;
      $dl->{'txns'}->{$txn_id} ||= {};
      my $txn = $dl->{'txns'}->{$txn_id};

      if ( $what eq 'TRANSACTION' ) {
         $txn->{'tx'} = parse_tx_text( $body, $complete, $debug, $full );
      }
      else {
         push @{$txn->{'locks'}}, parse_innodb_record_locks( $body, $complete, $debug, $full );
      }
   }

   @{ $dl }{ qw(rolled_back) }
      = $fulltext =~ m/^\*\*\* WE ROLL BACK TRANSACTION \($d\)$/m;

   # Make sure certain values aren't undef
   map { $dl->{$_} ||= '' } qw(rolled_back);

   delete $dl->{'fulltext'} unless $debug;
   return 1;
}

sub parse_innodb_record_locks {
   my ( $text, $complete, $debug, $full ) = @_;
   my @result;

   foreach my $lock ( $text =~ m/(^(?:RECORD|TABLE) LOCKS?.*$)/gm ) {
      my $hash = {};
      @{$hash}{ qw(lock_type space_id page_no n_bits index db table txn_id lock_mode) }
         = $lock
         =~ m{^(RECORD|TABLE) LOCKS? (?:space id $d page no $d n bits $d index `?$n`? of )?table `$n(?:/|`\.`)$n` trx id $t lock.mode (\S+)}m;
      ( $hash->{'special'} )
         = $lock =~ m/^(?:RECORD|TABLE) .*? locks (rec but not gap|gap before rec)/m;
      $hash->{'insert_intention'}
         = $lock =~ m/^(?:RECORD|TABLE) .*? insert intention/m ? 1 : 0;
      $hash->{'waiting'}
         = $lock =~ m/^(?:RECORD|TABLE) .*? waiting/m ? 1 : 0;

      # Some things may not be in the text, so make sure they are not
      # undef.
      map { $hash->{$_} ||= 0 } qw(n_bits page_no space_id);
      map { $hash->{$_} ||= "" } qw(index special);
      push @result, $hash;
   }

   return @result;
}

sub parse_tx_text {
   my ( $txn, $complete, $debug, $full ) = @_;

   my ( $txn_id, $txn_status, $active_secs, $proc_no, $os_thread_id )
      = $txn
      =~ m/^(?:---)?TRANSACTION $t, (\D*?)(?: $d sec)?, (?:process no $d, )?OS thread id $d/m;
   my ( $thread_status, $thread_decl_inside )
      = $txn
      =~ m/OS thread id \d+(?: ([^,]+?))?(?:, thread declared inside InnoDB $d)?$/m;

   # Parsing the line that begins 'MySQL thread id' is complicated.  The only
   # thing always in the line is the thread and query id.  See function
   # innobase_mysql_print_thd in InnoDB source file sql/ha_innodb.cc.
   my ( $thread_line ) = $txn =~ m/^(MySQL thread id .*)$/m;
   my ( $mysql_thread_id, $query_id, $hostname, $ip, $user, $query_status );

   if ( $thread_line ) {
      # These parts can always be gotten.
      ( $mysql_thread_id, $query_id ) = $thread_line =~ m/^MySQL thread id $d, query id $d/m;

      # If it's a master/slave thread, "Has (read|sent) all" may be the thread's
      # proc_info.  In these cases, there won't be any host/ip/user info
      ( $query_status ) = $thread_line =~ m/(Has (?:read|sent) all .*$)/m;
      if ( defined($query_status) ) {
         $user = 'system user';
      }

      # It may be the case that the query id is the last thing in the line.
      elsif ( $thread_line =~ m/query id \d+ / ) {
         # The IP address is the only non-word thing left, so it's the most
         # useful marker for where I have to start guessing.
         ( $hostname, $ip ) = $thread_line =~ m/query id \d+(?: ([A-Za-z]\S+))? $i/m;
         if ( defined $ip ) {
            ( $user, $query_status ) = $thread_line =~ m/$ip $w(?: (.*))?$/;
         }
         else { # OK, there wasn't an IP address.
            # There might not be ANYTHING except the query status.
            ( $query_status ) = $thread_line =~ m/query id \d+ (.*)$/;
            if ( $query_status !~ m/^\w+ing/ && !exists($is_proc_info{$query_status}) ) {
               # The remaining tokens are, in order: hostname, user, query_status.
               # It's basically impossible to know which is which.
               ( $hostname, $user, $query_status ) = $thread_line
                  =~ m/query id \d+(?: ([A-Za-z]\S+))?(?: $w(?: (.*))?)?$/m;
            }
            else {
               $user = 'system user';
            }
         }
      }
   }

   my ( $lock_wait_status, $lock_structs, $heap_size, $row_locks, $undo_log_entries )
      = $txn
      =~ m/^(?:(\D*) )?$d lock struct\(s\), heap size $d(?:, $d row lock\(s\))?(?:, undo log entries $d)?$/m;
   my ( $lock_wait_time )
      = $txn
      =~ m/^------- TRX HAS BEEN WAITING $d SEC/m;

   my $locks;
   # If the transaction has locks, grab the locks.
   if ( $txn =~ m/^TABLE LOCK|RECORD LOCKS/ ) {
      $locks = [parse_innodb_record_locks($txn, $complete, $debug, $full)];
   }
   
   my ( $tables_in_use, $tables_locked )
      = $txn
      =~ m/^mysql tables in use $d, locked $d$/m;
   my ( $txn_doesnt_see_ge, $txn_sees_lt )
      = $txn
      =~ m/^Trx read view will not see trx with id >= $t, sees < $t$/m;
   my $has_read_view = defined($txn_doesnt_see_ge);
   # Only a certain number of bytes of the query text are included here, at least
   # under some circumstances.  Some versions include 300, some 600.
   my ( $query_text )
      = $txn
      =~ m{
         ^MySQL\sthread\sid\s[^\n]+\n           # This comes before the query text
         (.*?)                                  # The query text
         (?=                                    # Followed by any of...
            ^Trx\sread\sview
            |^-------\sTRX\sHAS\sBEEN\sWAITING
            |^TABLE\sLOCK
            |^RECORD\sLOCKS\sspace\sid
            |^(?:---)?TRANSACTION
            |^\*\*\*\s\(\d\)
            |\Z
         )
      }xms;
   if ( $query_text ) {
      $query_text =~ s/\s+$//;
   }
   else {
      $query_text = '';
   }

   my %stuff = (
      active_secs        => $active_secs,
      has_read_view      => $has_read_view,
      heap_size          => $heap_size,
      hostname           => $hostname,
      ip                 => $ip,
      lock_structs       => $lock_structs,
      lock_wait_status   => $lock_wait_status,
      lock_wait_time     => $lock_wait_time,
      mysql_thread_id    => $mysql_thread_id,
      os_thread_id       => $os_thread_id,
      proc_no            => $proc_no,
      query_id           => $query_id,
      query_status       => $query_status,
      query_text         => $query_text,
      row_locks          => $row_locks,
      tables_in_use      => $tables_in_use,
      tables_locked      => $tables_locked,
      thread_decl_inside => $thread_decl_inside,
      thread_status      => $thread_status,
      txn_doesnt_see_ge  => $txn_doesnt_see_ge,
      txn_id             => $txn_id,
      txn_sees_lt        => $txn_sees_lt,
      txn_status         => $txn_status,
      undo_log_entries   => $undo_log_entries,
      user               => $user,
   );
   $stuff{'fulltext'} = $txn if $debug;
   $stuff{'locks'} = $locks if $locks;

   # Some things may not be in the txn text, so make sure they are not
   # undef.
   map { $stuff{$_} ||= 0 } qw(active_secs heap_size lock_structs
         tables_in_use undo_log_entries tables_locked has_read_view
         thread_decl_inside lock_wait_time proc_no row_locks);
   map { $stuff{$_} ||= "" } qw(thread_status txn_doesnt_see_ge
         txn_sees_lt query_status ip query_text lock_wait_status user);
   $stuff{'hostname'} ||= $stuff{'ip'};

   return \%stuff;
}

sub parse_tx_section {
   my ( $section, $complete, $debug, $full ) = @_;
   return unless $section && $section->{'fulltext'};
   my $fulltext = $section->{'fulltext'};
   $section->{'transactions'} = [];

   # Handle the individual transactions
   my @transactions = $fulltext =~ m/(---TRANSACTION \d.*?)(?=\n---TRANSACTION|$)/gs;
   foreach my $txn ( @transactions ) {
      my $stuff = parse_tx_text( $txn, $complete, $debug, $full );
      delete $stuff->{'fulltext'} unless $debug;
      push @{$section->{'transactions'}}, $stuff;
   }

   # Handle the general info
   @{$section}{ 'trx_id_counter' }
      = $fulltext =~ m/^Trx id counter $t$/m;
   @{$section}{ 'purge_done_for', 'purge_undo_for' }
      = $fulltext =~ m/^Purge done for trx's n:o < $t undo n:o < $t$/m;
   @{$section}{ 'history_list_len' } # This isn't present in some 4.x versions
      = $fulltext =~ m/^History list length $d$/m;
   @{$section}{ 'num_lock_structs' }
      = $fulltext =~ m/^Total number of lock structs in row lock hash table $d$/m;
   @{$section}{ 'is_truncated' }
      = $fulltext =~ m/^\.\.\. truncated\.\.\.$/m ? 1 : 0;

   # Fill in things that might not be present
   foreach ( qw(history_list_len) ) {
      $section->{$_} ||= 0;
   }

   delete $section->{'fulltext'} unless $debug;
   return 1;
}

# I've read the source for this section.
sub parse_ro_section {
   my ( $section, $complete, $debug, $full ) = @_;
   return unless $section && $section->{'fulltext'};
   my $fulltext = $section->{'fulltext'};

   # Grab the info
   @{$section}{ 'queries_inside', 'queries_in_queue' }
      = $fulltext =~ m/^$d queries inside InnoDB, $d queries in queue$/m;
   ( $section->{ 'read_views_open' } )
      = $fulltext =~ m/^$d read views open inside InnoDB$/m;
   ( $section->{ 'n_reserved_extents' } )
      = $fulltext =~ m/^$d tablespace extents now reserved for B-tree/m;
   @{$section}{ 'main_thread_proc_no', 'main_thread_id', 'main_thread_state' }
      = $fulltext =~ m/^Main thread (?:process no. $d, )?id $d, state: (.*)$/m;
   @{$section}{ 'num_rows_ins', 'num_rows_upd', 'num_rows_del', 'num_rows_read' }
      = $fulltext =~ m/^Number of rows inserted $d, updated $d, deleted $d, read $d$/m;
   @{$section}{ 'ins_sec', 'upd_sec', 'del_sec', 'read_sec' }
      = $fulltext =~ m#^$f inserts/s, $f updates/s, $f deletes/s, $f reads/s$#m;
   $section->{'main_thread_proc_no'} ||= 0;

   map { $section->{$_} ||= 0 } qw(read_views_open n_reserved_extents);
   delete $section->{'fulltext'} unless $debug;
   return 1;
}

sub parse_lg_section {
   my ( $section, $complete, $debug, $full ) = @_;
   return unless $section;
   my $fulltext = $section->{'fulltext'};

   # Grab the info
   ( $section->{ 'log_seq_no' } )
      = $fulltext =~ m/Log sequence number \s*(\d.*)$/m;
   ( $section->{ 'log_flushed_to' } )
      = $fulltext =~ m/Log flushed up to \s*(\d.*)$/m;
   ( $section->{ 'last_chkp' } )
      = $fulltext =~ m/Last checkpoint at \s*(\d.*)$/m;
   @{$section}{ 'pending_log_writes', 'pending_chkp_writes' }
      = $fulltext =~ m/$d pending log writes, $d pending chkp writes/;
   @{$section}{ 'log_ios_done', 'log_ios_s' }
      = $fulltext =~ m#$d log i/o's done, $f log i/o's/second#;

   delete $section->{'fulltext'} unless $debug;
   return 1;
}

sub parse_ib_section {
   my ( $section, $complete, $debug, $full ) = @_;
   return unless $section && $section->{'fulltext'};
   my $fulltext = $section->{'fulltext'};

   # Some servers will output ibuf information for tablespace 0, as though there
   # might be many tablespaces with insert buffers.  (In practice I believe
   # the source code shows there will only ever be one).  I have to parse both
   # cases here, but I assume there will only be one.
   @{$section}{ 'size', 'free_list_len', 'seg_size' }
      = $fulltext =~ m/^Ibuf(?: for space 0)?: size $d, free list len $d, seg size $d,$/m;
   @{$section}{ 'inserts', 'merged_recs', 'merges' }
      = $fulltext =~ m/^$d inserts, $d merged recs, $d merges$/m;

   @{$section}{ 'hash_table_size', 'used_cells', 'bufs_in_node_heap' }
      = $fulltext =~ m/^Hash table size $d, used cells $d, node heap has $d buffer\(s\)$/m;
   @{$section}{ 'hash_searches_s', 'non_hash_searches_s' }
      = $fulltext =~ m{^$f hash searches/s, $f non-hash searches/s$}m;

   delete $section->{'fulltext'} unless $debug;
   return 1;
}

sub parse_wait_array {
   my ( $text, $complete, $debug, $full ) = @_;
   my %result;

   @result{ qw(thread waited_at_filename waited_at_line waited_secs) }
      = $text =~ m/^--Thread $d has waited at $fl for $f seconds/m;

   # Depending on whether it's a SYNC_MUTEX,RW_LOCK_EX,RW_LOCK_SHARED,
   # there will be different text output
   if ( $text =~ m/^Mutex at/m ) {
      $result{'request_type'} = 'M';
      @result{ qw( lock_mem_addr lock_cfile_name lock_cline lock_var) }
         = $text =~ m/^Mutex at $h created file $fl, lock var $d$/m;
      @result{ qw( waiters_flag )}
         = $text =~ m/^waiters flag $d$/m;
   }
   else {
      @result{ qw( request_type lock_mem_addr lock_cfile_name lock_cline) }
         = $text =~ m/^(.)-lock on RW-latch at $h created in file $fl$/m;
      @result{ qw( writer_thread writer_lock_mode ) }
         = $text =~ m/^a writer \(thread id $d\) has reserved it in mode  (.*)$/m;
      @result{ qw( num_readers waiters_flag )}
         = $text =~ m/^number of readers $d, waiters flag $d$/m;
      @result{ qw(last_s_file_name last_s_line ) }
         = $text =~ m/Last time read locked in file $fl$/m;
      @result{ qw(last_x_file_name last_x_line ) }
         = $text =~ m/Last time write locked in file $fl$/m;
   }

   $result{'cell_waiting'} = $text =~ m/^wait has ended$/m ? 0 : 1;
   $result{'cell_event_set'} = $text =~ m/^wait is ending$/m ? 1 : 0;

   # Because there are two code paths, some things won't get set.
   map { $result{$_} ||= '' }
      qw(last_s_file_name last_x_file_name writer_lock_mode);
   map { $result{$_} ||= 0 }
      qw(num_readers lock_var last_s_line last_x_line writer_thread);

   return \%result;
}

sub parse_sm_section {
   my ( $section, $complete, $debug, $full ) = @_;
   return 0 unless $section && $section->{'fulltext'};
   my $fulltext = $section->{'fulltext'};

   # Grab the info
   @{$section}{ 'reservation_count', 'signal_count' }
      = $fulltext =~ m/^OS WAIT ARRAY INFO: reservation count $d, signal count $d$/m;
   @{$section}{ 'mutex_spin_waits', 'mutex_spin_rounds', 'mutex_os_waits' }
      = $fulltext =~ m/^Mutex spin waits $d, rounds $d, OS waits $d$/m;
   @{$section}{ 'rw_shared_spins', 'rw_shared_os_waits', 'rw_excl_spins', 'rw_excl_os_waits' }
      = $fulltext =~ m/^RW-shared spins $d, OS waits $d; RW-excl spins $d, OS waits $d$/m;

   # Look for info on waits.
   my @waits = $fulltext =~ m/^(--Thread.*?)^(?=Mutex spin|--Thread)/gms;
   $section->{'waits'} = [ map { parse_wait_array($_, $complete, $debug) } @waits ];
   $section->{'wait_array_size'} = scalar(@waits);

   delete $section->{'fulltext'} unless $debug;
   return 1;
}

# I've read the source for this section.
sub parse_bp_section {
   my ( $section, $complete, $debug, $full ) = @_;
   return unless $section && $section->{'fulltext'};
   my $fulltext = $section->{'fulltext'};

   # Grab the info
   @{$section}{ 'total_mem_alloc', 'add_pool_alloc' }
      = $fulltext =~ m/^Total memory allocated $d; in additional pool allocated $d$/m;
   @{$section}{'dict_mem_alloc'}     = $fulltext =~ m/Dictionary memory allocated $d/;
   @{$section}{'awe_mem_alloc'}      = $fulltext =~ m/$d MB of AWE memory/;
   @{$section}{'buf_pool_size'}      = $fulltext =~ m/^Buffer pool size\s*$d$/m;
   @{$section}{'buf_free'}           = $fulltext =~ m/^Free buffers\s*$d$/m;
   @{$section}{'pages_total'}        = $fulltext =~ m/^Database pages\s*$d$/m;
   @{$section}{'pages_modified'}     = $fulltext =~ m/^Modified db pages\s*$d$/m;
   @{$section}{'pages_read', 'pages_created', 'pages_written'}
      = $fulltext =~ m/^Pages read $d, created $d, written $d$/m;
   @{$section}{'page_reads_sec', 'page_creates_sec', 'page_writes_sec'}
      = $fulltext =~ m{^$f reads/s, $f creates/s, $f writes/s$}m;
   @{$section}{'buf_pool_hits', 'buf_pool_reads'}
      = $fulltext =~ m{Buffer pool hit rate $d / $d$}m;
   if ($fulltext =~ m/^No buffer pool page gets since the last printout$/m) {
      @{$section}{'buf_pool_hits', 'buf_pool_reads'} = (0, 0);
      @{$section}{'buf_pool_hit_rate'} = '--';
   }
   else {
      @{$section}{'buf_pool_hit_rate'}
         = $fulltext =~ m{Buffer pool hit rate (\d+ / \d+)$}m;
   }
   @{$section}{'reads_pending'} = $fulltext =~ m/^Pending reads $d/m;
   @{$section}{'writes_pending_lru', 'writes_pending_flush_list', 'writes_pending_single_page' }
      = $fulltext =~ m/^Pending writes: LRU $d, flush list $d, single page $d$/m;

   map { $section->{$_} ||= 0 }
      qw(writes_pending_lru writes_pending_flush_list writes_pending_single_page
      awe_mem_alloc dict_mem_alloc);
   @{$section}{'writes_pending'} = List::Util::sum(
      @{$section}{ qw(writes_pending_lru writes_pending_flush_list writes_pending_single_page) });

   delete $section->{'fulltext'} unless $debug;
   return 1;
}

# I've read the source for this.
sub parse_io_section {
   my ( $section, $complete, $debug, $full ) = @_;
   return unless $section && $section->{'fulltext'};
   my $fulltext = $section->{'fulltext'};
   $section->{'threads'} = {};

   # Grab the I/O thread info
   my @threads = $fulltext =~ m<^(I/O thread \d+ .*)$>gm;
   foreach my $thread (@threads) {
      my ( $tid, $state, $purpose, $event_set )
         = $thread =~ m{I/O thread $d state: (.+?) \((.*)\)(?: ev set)?$}m;
      if ( defined $tid ) {
         $section->{'threads'}->{$tid} = {
            thread    => $tid,
            state     => $state,
            purpose   => $purpose,
            event_set => $event_set ? 1 : 0,
         };
      }
   }

   # Grab the reads/writes/flushes info
   @{$section}{ 'pending_normal_aio_reads', 'pending_aio_writes' }
      = $fulltext =~ m/^Pending normal aio reads: $d, aio writes: $d,$/m;
   @{$section}{ 'pending_ibuf_aio_reads', 'pending_log_ios', 'pending_sync_ios' }
      = $fulltext =~ m{^ ibuf aio reads: $d, log i/o's: $d, sync i/o's: $d$}m;
   @{$section}{ 'flush_type', 'pending_log_flushes', 'pending_buffer_pool_flushes' }
      = $fulltext =~ m/^Pending flushes \($w\) log: $d; buffer pool: $d$/m;
   @{$section}{ 'os_file_reads', 'os_file_writes', 'os_fsyncs' }
      = $fulltext =~ m/^$d OS file reads, $d OS file writes, $d OS fsyncs$/m;
   @{$section}{ 'reads_s', 'avg_bytes_s', 'writes_s', 'fsyncs_s' }
      = $fulltext =~ m{^$f reads/s, $d avg bytes/read, $f writes/s, $f fsyncs/s$}m;
   @{$section}{ 'pending_preads', 'pending_pwrites' }
      = $fulltext =~ m/$d pending preads, $d pending pwrites$/m;
   @{$section}{ 'pending_preads', 'pending_pwrites' } = (0, 0)
      unless defined($section->{'pending_preads'});

   delete $section->{'fulltext'} unless $debug;
   return 1;
}

sub _debug {
   my ( $debug, $msg ) = @_;
   if ( $debug ) {
      die $msg;
   }
   else {
      warn $msg;
   }
   return 1;
}

1;

# end_of_package
# ############################################################################
# Perldoc section.  I put this last as per the Dog book.
# ############################################################################
=pod

=head1 NAME

InnoDBParser - Parse InnoDB monitor text.

=head1 DESCRIPTION

InnoDBParser tries to parse the output of the InnoDB monitor.  One way to get
this output is to connect to a MySQL server and issue the command SHOW ENGINE
INNODB STATUS (omit 'ENGINE' on earlier versions of MySQL).  The goal is to
turn text into data that something else (e.g. innotop) can use.

The output comes from all over, but the place to start in the source is
innobase/srv/srv0srv.c.

=head1 SYNOPSIS

   use InnoDBParser;
   use DBI;

   # Get the status text.
   my $dbh = DBI->connect(
      "DBI::mysql:test;host=localhost",
      'user',
      'password'
   );
   my $query = 'SHOW /*!5 ENGINE */ INNODB STATUS';
   my $text  = $dbh->selectcol_arrayref($query)->[0];

   # 1 or 0
   my $debug = 1;

   # Choose sections of the monitor text you want.  Possible values:
   # TRANSACTIONS                          => tx
   # BUFFER POOL AND MEMORY                => bp
   # SEMAPHORES                            => sm
   # LOG                                   => lg
   # ROW OPERATIONS                        => ro
   # INSERT BUFFER AND ADAPTIVE HASH INDEX => ib
   # FILE I/O                              => io
   # LATEST DETECTED DEADLOCK              => dl
   # LATEST FOREIGN KEY ERROR              => fk

   my $required_sections = {
      tx => 1,
   };

   # Parse the status text.
   my $parser = InnoDBParser->new;
   $innodb_status = $parser->parse_status_text(
      $text,
      $debug,
      # Omit the following parameter to get all sections.
      $required_sections,
   );

=head1 COPYRIGHT, LICENSE AND WARRANTY

This package is copyright (c) 2006 Baron Schwartz, baron at xaprb dot com.
Feedback and improvements are gratefully received.

THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, version 2; OR the Perl Artistic License.  On UNIX and similar
systems, you can issue `man perlgpl' or `man perlartistic' to read these
licenses.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA  02111-1307  USA

=head1 AUTHOR

Baron Schwartz, baron at xaprb dot com.

=head1 BUGS

None known, but I bet there are some.  The InnoDB monitor text wasn't really
designed to be parsable.

=head1 SEE ALSO

innotop - a program that can format the parsed status information for humans
to read and enjoy.

=cut
