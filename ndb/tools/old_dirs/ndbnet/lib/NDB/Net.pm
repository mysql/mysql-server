package NDB::Net;

use strict;
use Carp;
require Exporter;

use NDB::Util;

use vars qw(@ISA @EXPORT @EXPORT_OK);
@ISA = qw(Exporter);

use vars qw(@modules);
@modules = qw(
    NDB::Net::Base
    NDB::Net::Client
    NDB::Net::Command
    NDB::Net::Config
    NDB::Net::Database
    NDB::Net::Env
    NDB::Net::Node
    NDB::Net::NodeApi
    NDB::Net::NodeDb
    NDB::Net::NodeMgmt
    NDB::Net::Server
    NDB::Net::ServerINET
    NDB::Net::ServerUNIX
);

return 1 if $main::onlymodules;

for my $module (@modules) {
    eval "require $module";
    $@ and confess "$module $@";
}

for my $module (@modules) {
    eval "$module->initmodule";
    $@ and confess "$module $@";
}

1;
# vim:set sw=4:
