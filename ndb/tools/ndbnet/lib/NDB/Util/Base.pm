package NDB::Util::Base;

use strict;
use Carp;

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

sub new {
    my $class = shift;
    my $this = bless {}, $class;
    return $this;
}

sub getlog {
    my $this = shift;
    return NDB::Util::Log->instance;
}

# clone an object
# extra attributes override or delete (if value is undef)
sub clone {
    my $this = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $that = bless {}, ref($this);
    for my $attr (sort keys %$this) {
	if (! exists($attr{$attr})) {
	    my $get = "get$attr";
	    $attr{$attr} = $this->$get();
	}
    }
    for my $attr (sort keys %attr) {
	if (defined($attr{$attr})) {
	    my $set = "set$attr";
	    $that->$set($attr{$attr});
	}
    }
    return $that;
}

# methods for member variables:
# - set returns 1 on success and undef on undefined or invalid value
# - get aborts unless value exists or a default (maybe undef) is given
# - has tests existence of value
# - del deletes the value and returns it (maybe undef)

sub attributes {
    @_ % 2 == 1 or confess 0+@_;
    my $class = shift;
    my @attr = @_;
    while (@attr) {
	my $attr = shift @attr;
	my $filter = shift @attr;
	$attr =~ /^\w+$/ or confess $attr;
	ref($filter) eq 'CODE' or confess $attr;
	my $set = sub {
	    @_ == 2 or confess "set$attr: arg count: @_";
	    my $this = shift;
	    my $value = shift;
	    if (! defined($value)) {
		$log->put("set$attr: undefined value")->push($this);
		return undef;
	    }
	    local $_ = $value;
	    if (! &$filter($this)) {
		$log->put("set$attr: invalid value: $value")->push($this);
		return undef;
	    }
	    $value = $_;
	    if (! defined($value)) {
		confess "set$attr: changed to undef";
	    }
	    $this->{$attr} = $value;
	    return 1;
	};
	my $get = sub {
	    @_ == 1 || @_ == 2 or confess "get$attr: arg count: @_";
	    my $this = shift;
	    my $value = $this->{$attr};
	    if (! defined($value)) {
		@_ == 0 and confess "get$attr: no value";
		$value = shift;
	    }
	    return $value;
	};
	my $has = sub {
	    @_ == 1 or confess "has$attr: arg count: @_";
	    my $this = shift;
	    my $value = $this->{$attr};
	    return defined($value);
	};
	my $del = sub {
	    @_ == 1 or confess "del$attr: arg count: @_";
	    my $this = shift;
	    my $value = delete $this->{$attr};
	    return $value;
	};
	no strict 'refs';
	*{"${class}::set$attr"} = $set;
	*{"${class}::get$attr"} = $get;
	*{"${class}::has$attr"} = $has;
	*{"${class}::del$attr"} = $del;
    }
}

1;
# vim:set sw=4:
