package NDB::Run;

use strict;
use Carp;
require Exporter;

use NDB::Net;

use vars qw(@ISA @EXPORT @EXPORT_OK);
@ISA = qw(Exporter);

use vars qw(@modules);
@modules = qw(
    NDB::Run::Base
    NDB::Run::Database
    NDB::Run::Env
    NDB::Run::Node
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

# methods

sub getenv {
    my $class = shift;
    return NDB::Run::Env->new(@_);
}

1;
# vim:set sw=4:
