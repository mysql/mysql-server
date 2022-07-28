# -*- cperl -*-
# Copyright (c) 2007, 2022, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

package My::ConfigFactory;

use strict;
use warnings;
use Carp;
use Cwd qw(abs_path);
use File::Basename;

use My::Config;
use My::Find;
use My::Platform;

# Rules to run first of all
my @pre_rules = (\&pre_check_no_hosts_in_cluster_config);

sub get_basedir {
  my ($self, $group) = @_;
  my $basedir = $group->if_exist('basedir') ||
    $self->{ARGS}->{basedir};
  return $basedir;
}

sub get_testdir {
  my ($self, $group) = @_;
  my $testdir = $group->if_exist('testdir') ||
    $self->{ARGS}->{testdir};
  return $testdir;
}

# Retrieve build directory (which is different from basedir
# in out-of-source build).
sub get_bindir {
  if (defined $ENV{MTR_BINDIR}) {
    return $ENV{MTR_BINDIR};
  }
  my ($self, $group) = @_;
  return $self->get_basedir($group);
}

sub fix_charset_dir {
  my ($self, $config, $group_name, $group) = @_;
  return my_find_dir($self->get_basedir($group), \@::share_locations,
                     "charsets");
}

sub fix_datadir {
  my ($self, $config, $group_name) = @_;
  my $vardir = $self->{ARGS}->{vardir};
  return "$vardir/$group_name/data";
}

# Resolve the symbolic path and return the absolute path
# to the datadir it is pointing to.
sub fix_abs_datadir {
  my ($self, $config, $group_name) = @_;

  my $vardir;
  if ($::opt_mem) {
    # Resolve the symbolic path
    $vardir = abs_path($self->{ARGS}->{vardir});
  } else {
    $vardir = $self->{ARGS}->{vardir};
  }

  return "$vardir/$group_name/data";
}

sub fix_pidfile {
  my ($self, $config, $group_name, $group) = @_;
  my $vardir = $self->{ARGS}->{vardir};
  return "$vardir/run/$group_name.pid";
}

sub fix_port {
  my ($self, $config, $group_name, $group) = @_;
  my $hostname = $group->value('#host');
  return $self->{HOSTS}->{$hostname}++;
}

sub fix_admin_port {
  my ($self, $config, $group_name, $group) = @_;
  my $hostname = $group->value('#host');
  return $self->{HOSTS}->{$hostname}++;
}

sub fix_x_port {
  my ($self, $config, $group_name, $group) = @_;
  return $self->{ARGS}->{mysqlxbaseport}++;
}

sub fix_host {
  my ($self) = @_;
  # Get next host from HOSTS array
  my @hosts   = keys(%{ $self->{HOSTS} });
  my $host_no = $self->{NEXT_HOST}++ % @hosts;
  return $hosts[$host_no];
}

sub fix_cpubind {
  my ($self, $config, $group_name, $group) = @_;
  # Replace cpubind with #cpubind
  my $value = $group->if_exist('cpubind');
  if (defined $value) {
    $group->remove('cpubind');
  }
  return $value;
}

sub fix_cluster_config_suffix {
  my ($self, $config, $group_name, $group) = @_;

  my ($process_type, $idx, $suffix) = split(/\./, $group_name);
  return ".$suffix";
}

sub is_unique {
  my ($config, $name, $value) = @_;

  foreach my $group ($config->groups()) {
    if ($group->option($name)) {
      if ($group->value($name) eq $value) {
        return 0;
      }
    }
  }
  return 1;
}

sub fix_server_id {
  my ($self, $config, $group_name, $group) = @_;

  # Define in the order that mysqlds are listed in my.cnf
  my $server_id = $group->if_exist('server-id');
  if (defined $server_id) {
    if (!is_unique($config, 'server-id', $server_id)) {
      croak "The server-id($server_id) for '$group_name' is not unique";
    }
    return $server_id;
  }

  do {
    $server_id = $self->{SERVER_ID}++;
  } while (!is_unique($config, 'server-id', $server_id));

  return $server_id;
}

sub fix_x_socket {
  my ($self, $config, $group_name, $group) = @_;

  # Replace the mysqld prefix with mysqlx in order
  # to generate a socket named mysqlx*.sock
  $group_name =~ s/^mysqld/mysqlx/ or die;

  return fix_socket($self, $config, $group_name, $group);
}

sub fix_socket {
  my ($self, $config, $group_name, $group) = @_;

  # Put socket file in tmpdir
  my $dir    = $self->{ARGS}->{tmpdir};
  my $socket = "$dir/$group_name.sock";

  # Make sure the socket path does not become longer then the path
  # which mtr uses to test if a new tmpdir should be created.
  if (length($socket) > length("$dir/mysql*.NN.sock")) {
    # Too long socket path, generate shorter based on port
    my $port = $group->value('port');
    my $group_prefix = substr($group_name, 0, index($group_name, '.'));
    $socket = "$dir/$group_prefix-$port.sock";
  }

  return $socket;
}

sub fix_tmpdir {
  my ($self, $config, $group_name, $group) = @_;
  my $dir = $self->{ARGS}->{tmpdir};
  return "$dir/$group_name";
}

sub fix_log_error {
  my ($self, $config, $group_name, $group) = @_;
  my $dir = $self->{ARGS}->{vardir};
  if ($::opt_valgrind and $::opt_debug) {
    return "$dir/log/$group_name.trace";
  } else {
    return "$dir/log/$group_name.err";
  }
}

sub fix_log {
  my ($self, $config, $group_name, $group) = @_;
  my $dir = dirname($group->value('datadir'));
  return "$dir/mysqld.log";
}

sub fix_group_setting {
  my ($self, $config, $group_name, $group, $option_name) = @_;
  # Use value from [mysqld.X], [mysqld](if group exists) or 1
  return $group->if_exist($option_name) ||
         ($config->group_exists('mysqld') &&
          $config->group('mysqld')->if_exist($option_name)) || 1;
}

sub fix_slow_query_log {
  my ($self, $config, $group_name, $group) = @_;
  return fix_group_setting($self, $config, $group_name,
                           $group, 'slow_query_log');
}

sub fix_general_log {
  my ($self, $config, $group_name, $group) = @_;
  return fix_group_setting($self, $config, $group_name,
                           $group, 'general_log');
}

sub fix_log_slow_queries {
  my ($self, $config, $group_name, $group) = @_;
  my $dir = dirname($group->value('datadir'));
  return "$dir/mysqld-slow.log";
}

sub fix_secure_file_priv {
  my ($self) = @_;
  my $vardir = $self->{ARGS}->{vardir};

  # By default, prevent the started mysqld to access files
  # outside of vardir.
  return $vardir;
}

sub fix_std_data {
  my ($self, $config, $group_name, $group) = @_;
  my $testdir = $self->get_testdir($group);
  return "$testdir/std_data";
}

sub fix_ssl_disabled {

  # Add ssl-mode=DISABLED to avoid that mysqltest
  # connects with SSL by default.
  return "DISABLED";
}

sub fix_ssl_ca {
  my $std_data = fix_std_data(@_);
  return "$std_data/cacert.pem";
}

sub fix_ssl_server_cert {
  my $std_data = fix_std_data(@_);
  return "$std_data/server-cert.pem";
}

sub fix_ssl_server_key {
  my $std_data = fix_std_data(@_);
  return "$std_data/server-key.pem";
}

sub fix_rsa_private_key {
  my $std_data = fix_std_data(@_);
  return "$std_data/rsa_private_key.pem";
}

sub fix_rsa_public_key {
  my $std_data = fix_std_data(@_);
  return "$std_data/rsa_public_key.pem";
}

# Rules to run for each mysqld in the config
#  - some rules depend on each other and thus need to be run
#    in the order listed here
my @mysqld_rules = (
  { '#abs_datadir'                                 => \&fix_abs_datadir },
  { '#cpubind'                                     => \&fix_cpubind },
  { '#host'                                        => \&fix_host },
  { '#log-error'                                   => \&fix_log_error },
  { 'caching_sha2_password_private_key_path'       => \&fix_rsa_private_key },
  { 'caching_sha2_password_public_key_path'        => \&fix_rsa_public_key },
  { 'loose-sha256_password_private_key_path'       => \&fix_rsa_private_key },
  { 'loose-sha256_password_public_key_path'        => \&fix_rsa_public_key },
  { 'character-sets-dir'                           => \&fix_charset_dir },
  { 'datadir'                                      => \&fix_datadir },
  { 'port'                                         => \&fix_port },
  { 'admin-port'                                   => \&fix_admin_port },
  { 'general_log'                                  => \&fix_general_log },
  { 'general_log_file'                             => \&fix_log },
  { 'loose-mysqlx-port'                            => \&fix_x_port },
  { 'loose-mysqlx-socket'                          => \&fix_x_socket },
  { 'loose-mysqlx-ssl'                             => \&fix_ssl_disabled },
  { 'loose-mysqlx-ssl-ca'                          => "" },
  { 'loose-mysqlx-ssl-cert'                        => "" },
  { 'loose-mysqlx-ssl-key'                         => "" },
  { 'pid-file'                                     => \&fix_pidfile },
  { 'server-id'                                    => \&fix_server_id, },
  { 'slow_query_log'                               => \&fix_slow_query_log },
  { 'slow_query_log_file'                          => \&fix_log_slow_queries },
  { 'socket'                                       => \&fix_socket },
  { 'ssl-ca'                                       => \&fix_ssl_ca },
  { 'ssl-cert'                                     => \&fix_ssl_server_cert },
  { 'ssl-key'                                      => \&fix_ssl_server_key },
  { 'tmpdir'                                       => \&fix_tmpdir },
  { 'loose-sha256_password_auto_generate_rsa_keys' => "0" },
  { 'loose-caching_sha2_password_auto_generate_rsa_keys' => "0" },

  { '#mtr_basedir' => sub { return shift->{ARGS}->{basedir}; }
  },

  { '#password' => sub { return shift->{ARGS}->{password} || ""; }
  },

  { '#user' => sub { return shift->{ARGS}->{user} || ""; }
  },

  # By default, prevent the started mysqld to access files outside of vardir
  { 'secure-file-priv' => sub { return shift->{ARGS}->{vardir}; }
  },);

if (IS_WINDOWS) {
  # For simplicity, we use the same names for shared memory and
  # named pipes.
  push(@mysqld_rules, { 'shared-memory-base-name' => \&fix_socket });
}

sub fix_ndb_mgmd_port {
  my ($self, $config, $group_name, $group) = @_;
  my $hostname = $group->value('HostName');
  return $self->{HOSTS}->{$hostname}++;
}

sub fix_cluster_dir {
  my ($self, $config, $group_name, $group) = @_;
  my $vardir = $self->{ARGS}->{vardir};
  my (undef, $process_type, $idx, $suffix) = split(/\./, $group_name);
  return "$vardir/mysql_cluster.$suffix/$process_type.$idx";
}

sub fix_cluster_backup_dir {
  my ($self, $config, $group_name, $group) = @_;
  my $vardir = $self->{ARGS}->{vardir};
  my (undef, $process_type, $idx, $suffix) = split(/\./, $group_name);
  return "$vardir/mysql_cluster.$suffix/";
}

# Rules to run for each ndb_mgmd in the config
#  - will be run in order listed here
my @ndb_mgmd_rules =
  ({ 'DataDir' => \&fix_cluster_dir },
   { 'PortNumber' => \&fix_ndb_mgmd_port },
   { '#cpubind'   => \&fix_cpubind },);

# Rules to run for each ndbd in the config
#  - will be run in order listed here
my @ndbd_rules = ({ 'BackupDataDir' => \&fix_cluster_backup_dir },
                  { 'DataDir'       => \&fix_cluster_dir },
                  { 'HostName'      => \&fix_host },
                  { '#cpubind'      => \&fix_cpubind },);

# Rules to run for each memcached in the config
#  - will be run in order listed here
my @memcached_rules = ({ '#host' => \&fix_host }, { 'port' => \&fix_port },);

# Rules to run for each cluster_config section
#  - will be run in order listed here
my @cluster_config_rules = ({ 'mysqld'   => \&fix_host },
                            { 'ndb_mgmd' => \&fix_host },
                            { 'ndbapi'   => \&fix_host },
                            { 'ndbd'     => \&fix_host },);

# Rules to run for [client] section
#  - will be run in order listed here
my @client_rules = ();

# Rules to run for [mysqltest] section
#  - will be run in order listed here
my @mysqltest_rules = ({ 'server-public-key-path' => \&fix_rsa_public_key },
                       { 'ssl-mode'               => \&fix_ssl_disabled },
                       { '#cpubind'               => \&fix_cpubind },);

# Rules to run for [mysqlbinlog] section
#  - will be run in order listed here
my @mysqlbinlog_rules = (
  { 'character-sets-dir' => \&fix_charset_dir },
  { 'local-load' => sub { return shift->{ARGS}->{tmpdir}; }
  },);

# Rules to run for [mysql_upgrade] section
#  - will be run in order listed here
my @mysql_upgrade_rules = ();

# For [cluster_config] section there should be no ndbd, ndb_mgmd, mysqld,
# ndbapi options set.  These should only be set in cluster instance specific
# sections, [cluster_config.X].  This is crucial when determining in the order
# ndb_mgmd will enumerate node ids for nodes without explicit node id.
sub pre_check_no_hosts_in_cluster_config {
  my ($self, $config) = @_;

  my $group = $config->group('cluster_config');
  if (defined $group) {
    if (defined $group->if_exist('ndbd') ||
        defined $group->if_exist('ndb_mgmd') ||
        defined $group->if_exist('mysqld') ||
        defined $group->if_exist('ndbapi')) {
      croak "Configuration error: Do not set ndbd, ndb_mgmd, mysqld, ndbapi ".
            "in [cluster_config] section.  Use cluster specific sections ".
            "[cluster_config.X] instead.";
    }
  }
}

# For each [cluster_config.X] record all allocated node ids (set using NodeId
# property).  This will be used when determining what node id nodes with no
# configured node id will get.
sub track_allocated_nodeid {
  my ($self, $config, $group_name, $group) = @_;

  my (undef, $process_type, $idx, $suffix) = split(/\./, $group_name);

  if (my $nodeid = $group->if_exist('NodeId')) {
    if (!defined $suffix) {
      croak "Configuration error: Do not set NodeId in general group ".
            "[$group_name] but use instance specific group like ".
            "[cluster_config.$process_type.1.cluster] instead.";
    }
    $config->{"cluster_config.$suffix"}->{"ALLOCATED NODEIDS"}->{$nodeid} = 1;
  }
  return;
}

# Generate a [client.<suffix>] group to be used for
# connecting to [mysqld.<suffix>].
sub post_check_client_group {
  my ($self, $config, $client_group_name, $mysqld_group_name) = @_;

  #  Settings needed for client, copied from its "mysqld"
  my %client_needs = (host     => '#host',
                      password => '#password',
                      port     => 'port',
                      socket   => 'socket',
                      user     => '#user',);

  my $group_to_copy_from = $config->group($mysqld_group_name);
  while (my ($name_to, $name_from) = each(%client_needs)) {
    my $option = $group_to_copy_from->option($name_from);
    if (!defined $option) {
      croak "Could not get value for '$name_from'";
    }
    $config->insert($client_group_name, $name_to, $option->value());
  }
}

sub post_check_client_groups {
  my ($self, $config) = @_;

  my $first_mysqld = $config->first_like('mysqld.');

  return unless $first_mysqld;

  # Always generate [client] pointing to the first
  # [mysqld.<suffix>].
  $self->post_check_client_group($config, 'client', $first_mysqld->name());

  # Then generate [client.<suffix>] for each [mysqld.<suffix>].
  foreach my $mysqld ($config->like('mysqld.')) {
    $self->post_check_client_group($config, 'client' . $mysqld->after('mysqld'),
                                   $mysqld->name());
  }
}

sub resolve_at_variable {
  my ($self, $config, $group, $option) = @_;

  # Split the options value on last '.'
  my @parts       = split(/\./, $option->value());
  my $option_name = pop(@parts);
  my $group_name  = join('.', @parts);

  # Remove '@'
  $group_name =~ s/^\@//;

  my $from;
  if ($group_name =~ "env") {
    $from = $ENV{$option_name};
  } else {
    my $from_group = $config->group($group_name) or
      croak "There is no group named '$group_name' that ",
      "can be used to resolve '$option_name'";

    $from = $from_group->value($option_name);
  }

  $config->insert($group->name(), $option->name(), $from);
}

sub post_fix_resolve_at_variables {
  my ($self, $config) = @_;

  foreach my $group ($config->groups()) {
    foreach my $option ($group->options()) {
      next unless defined $option->value();
      $self->resolve_at_variable($config, $group, $option)
        if ($option->value() =~ /^\@/);
    }
  }
}

sub post_fix_mysql_cluster_section {
  my ($self, $config) = @_;

  # Add a [mysl_cluster.<suffix>] section for each
  # defined [cluster_config.<suffix>] section.
  foreach my $group ($config->like('cluster_config\.\w*$')) {
    my @urls;
    # Generate ndb_connectstring for this cluster
    foreach my $ndb_mgmd ($config->like('cluster_config.ndb_mgmd.')) {
      if ($ndb_mgmd->suffix() eq $group->suffix()) {
        my $host = $ndb_mgmd->value('HostName');
        my $port = $ndb_mgmd->value('PortNumber');
        push(@urls, "$host:$port");
      }
    }
    croak "Could not generate valid ndb_connectstring for '$group'"
      unless @urls > 0;
    my $ndb_connectstring = join(";", @urls);

    # Add ndb_connectstring to [mysql_cluster.<suffix>]
    $config->insert('mysql_cluster' . $group->suffix(),
                    'ndb_connectstring', $ndb_connectstring);

    # Add ndb_connectstring to each ndbd connected to this
    # cluster.
    foreach my $ndbd ($config->like('cluster_config.ndbd.')) {
      if ($ndbd->suffix() eq $group->suffix()) {
        my $after = $ndbd->after('cluster_config.ndbd');
        $config->insert("ndbd$after",
                        'ndb_connectstring', $ndb_connectstring);
      }
    }

    # Add ndb_connectstring to each ndb_mgmd connected to this
    # cluster.
    foreach my $ndb_mgmd ($config->like('cluster_config.ndb_mgmd.')) {
      if ($ndb_mgmd->suffix() eq $group->suffix()) {
        my $after = $ndb_mgmd->after('cluster_config.ndb_mgmd');
        $config->insert("ndb_mgmd$after",
                        'ndb_connectstring', $ndb_connectstring);
      }
    }

    # Add ndb_connectstring to each mysqld connected to this
    # cluster.
    foreach my $mysqld ($config->like('cluster_config.mysqld.')) {
      if ($mysqld->suffix() eq $group->suffix()) {
        my $after = $mysqld->after('cluster_config.mysqld');
        $config->insert("mysqld$after",
                        'ndb_connectstring', $ndb_connectstring);
      }
    }
  }
}

# Rules to run last of all
my @post_rules = (\&post_check_client_groups,
                  \&post_fix_mysql_cluster_section,
                  \&post_fix_resolve_at_variables,);

sub run_rules_for_group {
  my ($self, $config, $group, @rules) = @_;
  foreach my $hash (@rules) {
    while (my ($option, $rule) = each(%{$hash})) {
      # Only run this rule if the value is not already defined
      if (!$config->exists($group->name(), $option)) {
        my $value;
        if (ref $rule eq "CODE") {
          # Call the rule function
          $value =
            &$rule($self, $config, $group->name(),
                   $config->group($group->name()));
        } else {
          $value = $rule;
        }
        if (defined $value) {
          $config->insert($group->name(), $option, $value, 1);
        }
      }
    }
  }
}

sub run_section_rules {
  my ($self, $config, $name, @rules) = @_;

  foreach my $group ($config->like($name)) {
    $self->run_rules_for_group($config, $group, @rules);
  }
}

sub run_generate_sections_from_cluster_config {
  my ($self, $config) = @_;

  foreach my $group ($config->like('cluster_config\.\w*$')) {

    # @options will be a list of ndbd, ndb_mgmd, mysqld, ndbapi ordered in the
    # way that ndb_mgmd will assign node ids to nodes without id.
    my @options;

    foreach my $option ($group->options()) {
      my $option_name = $option->name();
      if ($option_name =~ /^(ndbd|ndb_mgmd|mysqld|ndbapi)$/)
      {
        push @options, $option_name;
      }
    }

    # Keep track of next node id to use if not explicitly set.
    my $next_nodeid = 1;

    # Keep track of current index per process type
    my %idxes;
    map { $idxes{$_} = 1; } @options;

    foreach my $option_name (@options) {
      my $value = $group->value($option_name);
      my @hosts = split(/,/, $value, -1);    # -1 => return also empty strings

      # Add at least one host
      push(@hosts, undef) unless scalar(@hosts);

      # Assign hosts unless already fixed
      @hosts = map { $self->fix_host() unless $_; } @hosts;

      # Write the hosts value back
      $group->insert($option_name, join(",", @hosts));

      # Generate sections for each host
      my $instances = @hosts;
      foreach my $host (@hosts) {
        my $idx    = $idxes{$option_name}++;
        my $suffix = $group->suffix();

        # Generate a section for ndb_mgmd to read
        $config->insert("cluster_config.$option_name.$idx$suffix",
                        "HostName", $host);

        # Predict how ndbd_mgm will assign node id for current node.
        my $node = $config->group("cluster_config.$option_name.$idx$suffix");
        my $nodeid = $node->if_exist('NodeId');
        if (!$nodeid) {
          $nodeid = $next_nodeid;
          while ($config->{"cluster_config$suffix"}->
                          {"ALLOCATED NODEIDS"}->
                          {$nodeid}) {
            $nodeid++;
          }
        }
        $next_nodeid = $nodeid + 1;

        # For data node set matching command line option --ndb-nodeid.
        # If prediction of node id is wrong, and node id for another node type
        # is used, that will cause testcase to fail during setup.
        if ($option_name eq 'ndbd') {
          if ($instances > 1) {
            $config->insert("$option_name.$idx$suffix",
                            'ndb-nodeid', $nodeid);
          }
        }

        if ($option_name eq 'ndb_mgmd') {
          if ($instances > 1) {
            $config->insert("$option_name.$idx$suffix",
                            'ndb-nodeid', $nodeid);
          }
          $config->insert("$option_name.$idx$suffix",
                          'cluster-config-suffix', $suffix);
        }

        if ($option_name eq 'mysqld') {
          my $datadir =
            $self->fix_cluster_dir($config, "cluster_config.mysqld.$idx$suffix",
                                   $group);
          $config->insert("mysqld.$idx$suffix", 'datadir', "$datadir/data");
        }
      }
    }
  }
}

sub new_config {
  my ($class, $args) = @_;

  my @required_args = ('basedir', 'baseport', 'vardir', 'template_path', 'testdir', 'tmpdir');

  foreach my $required (@required_args) {
    croak "you must pass '$required'" unless defined $args->{$required};
  }

  # Fill in hosts/port hash
  my $hosts    = {};
  my $baseport = $args->{baseport};
  $args->{hosts} = ['localhost'] unless exists($args->{hosts});
  foreach my $host (@{ $args->{hosts} }) {
    $hosts->{$host} = $baseport;
  }

  # Open the config template
  my $config              = My::Config->new($args->{'template_path'});
  my $extra_template_path = $args->{'extra_template_path'};
  if ($extra_template_path) {
    $config->append(My::Config->new($extra_template_path));
  }

  my $self = bless { CONFIG    => $config,
                     ARGS      => $args,
                     HOSTS     => $hosts,
                     NEXT_HOST => 0,
                     SERVER_ID => 1,
  }, $class;

  # Run pre rules
  foreach my $rule (@pre_rules) {
    &$rule($self, $config);
  }

  $self->run_section_rules($config, 'cluster_config\.\w*$',
                           @cluster_config_rules);

  $self->run_generate_sections_from_cluster_config($config);

  $self->run_section_rules($config, 'cluster_config\.\w',
                           ({ 'CODE' => \&track_allocated_nodeid }));

  $self->run_section_rules($config, 'cluster_config.ndb_mgmd.',
                           @ndb_mgmd_rules);

  $self->run_section_rules($config, 'ndb_mgmd.',
    ({ 'cluster-config-suffix' => \&fix_cluster_config_suffix },));

  $self->run_section_rules($config, 'cluster_config.ndbd', @ndbd_rules);

  $self->run_section_rules($config, 'mysqld.', @mysqld_rules);

  $self->run_section_rules($config, 'memcached.', @memcached_rules);

  # [mysqlbinlog] need additional settings
  $self->run_rules_for_group($config, $config->insert('mysqlbinlog'),
                             @mysqlbinlog_rules);

  # [mysql_upgrade] need additional settings
  $self->run_rules_for_group($config, $config->insert('mysql_upgrade'),
                             @mysql_upgrade_rules);

  # Additional rules required for [client]
  $self->run_rules_for_group($config, $config->insert('client'), @client_rules);

  # Additional rules required for [mysqltest]
  $self->run_rules_for_group($config, $config->insert('mysqltest'),
                             @mysqltest_rules);

  if ($::secondary_engine_support) {
    eval 'use mtr_secondary_engine_config; 1';
    # Additional rules required for secondary engine server
    push(@post_rules, \&post_check_secondary_engine_group);
    # Additional rules required for [mysqld] when secondary engine is enabled
    push(@post_rules, \&post_check_secondary_engine_mysqld_group);
  }

  # Run post rules
  foreach my $rule (@post_rules) {
    &$rule($self, $config);
  }

  return $config;
}

1;
