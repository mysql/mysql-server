package NDB::Util::File;

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

NDB::Util::File->attributes(
    path => sub { length > 0 },
);

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $file = $class->SUPER::new(%attr);
    $file->setpath($attr{path})
	or $log->push, return undef;
    return $file;
}

sub desc {
    my $file = shift;
    return $file->getpath;
}

sub getdir {
    my $file = shift;
    @_ == 0 or confess 0+@_;
    my $dirpath = dirname($file->getpath);
    my $dir = NDB::Util::Dir->new(path => $dirpath);
    return $dir;
}

sub getlock {
    my $file = shift;
    @_ == 0 or confess 0+@_;
    my $lock = NDB::Util::Lock->new(path => $file->getpath);
    return $lock;
}

sub getbasename {
    my $file = shift;
    @_ == 0 or confess 0+@_;
    return basename($file->getpath);
}

# make dir, unlink

sub mkdir {
    my $file = shift;
    @_ == 0 or confess 0+@_;
    return $file->getdir->mkdir;
}

sub unlink {
    my $file = shift;
    @_ == 0 or confess 0+@_;
    $log->put("remove")->push($file)->debug;
    if (-e $file->getpath) {
	if (! unlink($file->getpath)) {
	    my $errstr = "$!";
	    if (! -e $file->getpath) {
		return 1;
	    }
	    $log->put("unlink failed: $errstr")->push($file);
	    return undef;
	}
    }
    return 1;
}

# read /write

sub open {
    my $file = shift;
    @_ == 1 or confess 0+@_;
    my($mode) = @_;
    my $fh = gensym();
    if (! open($fh, $mode.$file->getpath)) {
	$log->put("open$mode failed")->push($file);
	return undef;
    }
    my $io = NDB::Util::IO->new;
    $io->setfh($fh)
	or $log->push, return undef;
    return $io;
}

sub puttext {
    my $file = shift;
    @_ == 1 or confess 0+@_;
    my($text) = @_;
    ref($text) and confess 'oops';
    $file->mkdir
	or $log->push, return undef;
    $file->unlink
	or $log->push, return undef;
    my $io = $file->open(">")
	or $log->push, return undef;
    if (! $io->write($text)) {
	$log->push($file);
	$io->close;
	return undef;
    }
    if (! $io->close) {
	$log->push($file);
	return undef;
    }
    return 1;
}

sub putlines {
    my $file = shift;
    @_ == 1 or confess 0+@_;
    my($lines) = @_;
    ref($lines) eq 'ARRAY' or confess 'oops';
    my $text = join("\n", @$lines) . "\n";
    $file->puttext($text) or $log->push, return undef;
    return 1;
}

sub copyedit {
    my $file1 = shift;
    @_ == 2 or confess 0+@_;
    my($file2, $edit) = @_;
    my $io1 = $file1->open("<")
	or $log->push, return undef;
    my $io2 = $file2->open(">")
	or $log->push, return undef;
    local $_;
    my $fh1 = $io1->getfh;
    my $fh2 = $io2->getfh;
    my $line = 0;
    while (defined($_ = <$fh1>)) {
	$line++;
	if (! &$edit()) {
	    $log->push("line $line")->push($file1);
	    return undef;
	}
	print $fh2 $_;
    }
    $io1->close;
    $io2->close;
    return 1;
}

1;
# vim:set sw=4:
