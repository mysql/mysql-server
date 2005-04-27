package NDB::Util;

use strict;
use Carp;
require Exporter;

use vars qw(@ISA @EXPORT @EXPORT_OK);
@ISA = qw(Exporter);

use vars qw(@modules);
@modules = qw(
    NDB::Util::Base
    NDB::Util::Dir
    NDB::Util::Event
    NDB::Util::File
    NDB::Util::IO
    NDB::Util::Lock
    NDB::Util::Log
    NDB::Util::Socket
    NDB::Util::SocketINET
    NDB::Util::SocketUNIX
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
