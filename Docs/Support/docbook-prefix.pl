#!/usr/bin/perl -w

# Preprocess the input of `makeinfo --docbook` version 4.0c
# Authors: Arjen Lentz and Zak Greant (started by arjen 2002-05-01)

use strict;

my $data  = '';

msg ("-- Pre-processing `makeinfo --docbook` input --");
msg ("** Written to work with makeinfo version 4.0c **\n");

# <> is a magic filehandle - either reading lines from stdin or from file(s) specified on the command line
msg ("Get the data");
$data = join "", <>;

msg ("Replacing '\@-' with FIXUPmdashFIXUP");
$data =~ s/\@-/FIXUPmdashFIXUP/g;

msg ("Replacing '--' with FIXUPdoubledashFIXUP");
$data =~ s/--/FIXUPdoubledashFIXUP/g;

msg ("Turning \@strong{} into LITERAL blocks");
$data =~ s/\@strong\{(.*?)\}/FIXUPstrongFIXUP$1FIXUPendstrongFIXUP/gs;

msg ("Turning \@emph{} into LITERAL blocks");
$data =~ s/\@emph\{(.*?)\}/FIXUPemphFIXUP$1FIXUPendemphFIXUP/gs;

msg ("Turning \@file{} into LITERAL blocks");
$data =~ s/\@file\{(.*?)\}/FIXUPfileFIXUP$1FIXUPendfileFIXUP/gs;

msg ("Turning \@samp{} into LITERAL blocks");
$data =~ s/\@samp\{\@\{\}/FIXUPsampFIXUP\@\{FIXUPendsampFIXUP/g;
$data =~ s/\@samp\{\@\}\}/FIXUPsampFIXUP\@\}FIXUPendsampFIXUP/g;
$data =~ s/\@samp\{\@\{n\@\}\}/FIXUPsampFIXUP\@\{n\@\}FIXUPendsampFIXUP/g;
$data =~ s/\@samp\{(.*?)\}/FIXUPsampFIXUP$1FIXUPendsampFIXUP/gs;


msg ("Write the data");
print STDOUT $data;
exit;

#
# Definitions for helper sub-routines
#

sub msg {
    print STDERR "docbook-prefix: ", shift, "\n";
}

