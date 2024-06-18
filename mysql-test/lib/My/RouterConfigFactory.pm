# -*- cperl -*-
# Copyright (c) 2024, Oracle and/or its affiliates.
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

package My::RouterConfigFactory;

use strict;
use warnings;
use Carp;
use Cwd qw(abs_path);
use File::Basename;

use My::Config;
use My::Find;
use My::Platform;

sub new {
  my ($class) = @_;
  my $self = bless({}, $class);
  return $self;
}

sub get_testdir {
  my ($self, $group) = @_;
  my $testdir = $group->if_exist('testdir') ||
    $self->{ARGS}->{testdir};
  return $testdir;
}

sub fix_std_data {
  my ($self, $config, $group_name, $group) = @_;
  my $testdir = $self->get_testdir($group);
  return "$testdir/std_data";
}

sub fix_plugin_folder {
  my ($self, $config, $group_name, $group) = @_;

  my $plugin_folder = $group->if_exist('plugin_folder') ||
    $self->{ARGS}->{plugin_folder};

  $self->push_env_variable("ROUTER_PLUGIN_DIRECTORY", $plugin_folder);

  return $plugin_folder;
}

sub fix_log_error {
  my ($self, $config, $group_name, $group) = @_;
  my $dir = $self->{ARGS}->{vardir};

 return "$dir/log/mysqlrouter.log";
}

sub fix_logging_folder {
  return dirname(fix_log_error(@_));
}

sub fix_logging_file {
  return basename(fix_log_error(@_));
}

sub fix_runtime_folder {
  my ($self, $config, $group_name, $group) = @_;
  my $dir= $self->{ARGS}->{vardir};

  return "$dir/run";
}

sub fix_data_folder {
  my ($self, $config, $group_name, $group) = @_;
  my $dir= $self->{ARGS}->{vardir};

  return "$dir/data";
}

sub fix_client_ssl_cert {
  my $std_data = fix_std_data(@_);

  return "$std_data/server-cert.pem";
}

sub fix_client_ssl_key {
  my $std_data = fix_std_data(@_);

  return "$std_data/server-key.pem";
}

sub fix_host {
  my ($self) = @_;
  # Get next host from HOSTS array
  my @hosts   = keys(%{ $self->{HOSTS} });
  my $host_no = $self->{NEXT_HOST}++ % @hosts;
  return $hosts[$host_no];
}

 sub fix_bind_port {
  my ($self, $config, $group_name, $group, $option) = @_;
  my $hostname = fix_host(@_);
  my $variable_name = (uc $group_name . "_" . $option) =~ s/:/_/r;
 
  $self->push_env_variable($variable_name, $self->{HOSTS}->{$hostname});
  return $self->{HOSTS}->{$hostname}++;
 }

sub fix_destinations {
  my ($self, $config, $group_name, $group) = @_;
  my $hostname = fix_host(@_);
  my $endpoint = $self->{ARGS}->{endpoint};

  return "$hostname:$endpoint";
}

sub fix_keyring_path {
  my ($self, $config, $group_name, $group) = @_;
  my $dir = $self->{ARGS}->{vardir};

  return "$dir/keyring";
}

sub fix_master_key_path {
  my ($self, $config, $group_name, $group) = @_;
  my $dir = $self->{ARGS}->{vardir};

  return "$dir/mysqlrouter.key";
}

sub fix_dynamic_state {
  my ($self, $config, $group_name, $group) = @_;
  my $dir= $self->{ARGS}->{vardir};

  return "$dir/state.json";
}

sub fix_http_ssl {
  # TODO : implement this!
  my ($self, $config, $group_name, $group) = @_;
  $self->push_env_variable("HTTP_SERVER_SSL", 1);
  return 1;
}

sub fix_http_server_static_folder {
  # TODO : implement this!
  my ($self, $config, $group_name, $group) = @_;
  my $dir = $self->{ARGS}->{vardir};

  return $dir;
}

sub fix_pid_file {
  my ($self, $config, $group_name, $group) = @_;
  my $dir = $self->{ARGS}->{vardir};
  return "$dir/run/mysqlrouter.pid";
}

my @DEFAULT_rules = (
  { 'plugin_folder' => \&fix_plugin_folder },
  { '#log-error' => \&fix_log_error },
  { 'logging_folder' => \&fix_logging_folder },
  { 'filename' => \&fix_logging_file },
  { 'runtime_folder' => \&fix_runtime_folder },
  { 'data_folder' => \&fix_data_folder },
  { 'pid_file' => \&fix_pid_file },

  { 'keyring_path' => \&fix_keyring_path },
  { 'master_key_path' => \&fix_master_key_path },

  { 'dynamic_state' => \&fix_dynamic_state },

  { 'client_ssl_cert' => \&fix_client_ssl_cert },
  { 'client_ssl_key'  => \&fix_client_ssl_key },
  );

my @routing_rules = (
  { 'bind_port' => \&fix_bind_port },
  { 'destinations' => \&fix_destinations },
  );

my @http_server_rules = (
  { 'port' => \&fix_bind_port },
  { 'ssl' => \&fix_http_ssl },
  { 'ssl_cert' => \&fix_client_ssl_cert },
  { 'ssl_key' => \&fix_client_ssl_key },
#  { 'static_folder' => \&fix_http_server_static_folder },
  { 'static_folder' => "" },
  );

my @mrs_rules = (
  { 'mysql_user' => "root" },
  { 'mysql_user_data_access' => "root" },
  { 'router_id' => 1 },
  { 'mysql_read_write_route' => "undertest" },
  #{ 'mysql_read_only_route' => "TODO" },
  { 'metadata_refresh_interval' => 1 },
  );

sub push_env_variable {
  my ($self, $name, $value) = @_;
  push(@{$self->{ENV}}, My::Config::Option->new($name, $value));
}

sub env_variables {
  my ($self) = @_;

  return @{$self->{ENV}};
}

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
                   $config->group($group->name()), $option);
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

sub new_config {
  my ($class, $args) = @_;

  my @required_args = ('basedir', 'baseport', 'plugin_folder',
                       'template_path', 'testdir', 'vardir');

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
  my $config  = My::Config->new($args->{'template_path'});
  $class->{CONFIG}=$config;
  $class->{ARGS}=$args;
  $class->{HOSTS}=$hosts;
  $class->{NEXT_HOST}=0;
  $class->{ENV}=();

  $class->run_section_rules($config, 'DEFAULT', @DEFAULT_rules);
  $class->run_section_rules($config, 'routing', @routing_rules);
  $class->run_section_rules($config, 'http_server', @http_server_rules);
  $class->run_section_rules($config, 'mysql_rest_service', @mrs_rules);

  return $config;
}

1;
