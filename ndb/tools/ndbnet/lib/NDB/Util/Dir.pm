package NDB::Util::Dir;

use strict;
use Carp;
use Symbol;
use Errno;
use File::Basename;

require NDB::Util::Base;

use vars qw(@ISA);
@ISA = qw(NDB::Util::Base);

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

NDB::Util::Dir->attributes(
    path => sub { length > 0 },
);

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $dir = $class->SUPER::new(%attr);
    $dir->setpath($attr{path})
	or $log->push, return undef;
    return $dir;
}

sub desc {
    my $dir = shift;
    return $dir->getpath;
}

sub getparent {
    my $dir = shift;
    @_ == 0 or confess 0+@_;
    my $ppath = dirname($dir->getpath);
    my $pdir = NDB::Util::Dir->new(path => $ppath);
    return $pdir;
}

sub getdir {
    my $dir = shift;
    @_ == 1 or confess 0+@_;
    my($name) = @_;
    my $dirpath = $dir->getpath;
    my $path = $dirpath eq '.' ? $name : File::Spec->catfile($dirpath, $name);
    my $entry = NDB::Util::Dir->new(path => $path);
    return $entry;
}

sub getfile {
    my $dir = shift;
    @_ == 1 or confess 0+@_;
    my($name) = @_;
    my $dirpath = $dir->getpath;
    my $path = $dirpath eq '.' ? $name : File::Spec->catfile($dirpath, $name);
    my $entry = NDB::Util::File->new(path => $path);
    return $entry;
}

# list

sub listdirs {
    my $dir = shift;
    @_ == 0 or confess 0+@_;
    my @list = ();
    my $dirpath = $dir->getpath;
    my $dh = gensym();
    if (! opendir($dh, $dirpath)) {
	$log->put("opendir failed: $!")->push($dir);
	return undef;
    }
    while (defined(my $name = readdir($dh))) {
	if ($name eq '.' || $name eq '..') {
	    next;
	}
	my $path = $dirpath eq '.' ? $name : "$dirpath/$name";
	if (! -l $path && -d $path) {
	    my $dir2 = NDB::Util::Dir->new(path => $path)
		or $log->push, return undef;
	    push(@list, $dir2);
	}
    }
    close($dh);
    return \@list;
}

sub listfiles {
    my $dir = shift;
    @_ == 0 or confess 0+@_;
    my @list = ();
    my $dirpath = $dir->getpath;
    my $dh = gensym();
    if (! opendir($dh, $dirpath)) {
	$log->put("opendir failed: $!")->push($dir);
	return undef;
    }
    while (defined(my $name = readdir($dh))) {
	if ($name eq '.' || $name eq '..') {
	    next;
	}
	my $path = $dirpath eq '.' ? $name : "$dirpath/$name";
	if (! -d $path && -e $path) {
	    my $file2 = NDB::Util::File->new(path => $path)
		or $log->push, return undef;
	    push(@list, $file2);
	}
    }
    close($dh);
    return \@list;
}

# create / remove

sub mkdir {
    my $dir = shift;
    @_ == 0 or confess 0+@_;
    if (! -d $dir->getpath) {
	my $pdir = $dir->getparent;
	if (length($pdir->getpath) >= length($dir->getpath)) {
	    $log->put("mkdir looping")->push($dir);
	    return undef;
	}
	$pdir->mkdir or return undef;
	if (! mkdir($dir->getpath, 0777)) {
	    my $errstr = "$!";
	    if (-d $dir->getpath) {
		return 1;
	    }
	    $log->put("mkdir failed: $errstr")->push($dir);
	    return undef;
	}
    }
    return 1;
}

sub rmdir {
    my $dir = shift;
    my $keep = shift;		# keep top level
    $log->put("remove")->push($dir)->info;
    my $list;
    $list = $dir->listdirs or $log->push, return undef;
    for my $d (@$list) {
	$d->rmdir or $log->push, return undef;
    }
    $list = $dir->listfiles or $log->push, return undef;
    for my $f (@$list) {
	$f->unlink or $log->push, return undef;
    }
    if (! $keep && ! rmdir($dir->getpath)) {
	my $errstr = "$!";
	if (! -e $dir->getpath) {
	    return 1;
	}
	$log->put("rmdir failed: $errstr")->push($dir);
	return undef;
    }
    return 1;
}

1;
# vim:set sw=4:
