#!/bin/sh

/usr/lib/rpm/perl.req $* | grep -v -e "perl(th" -e "perl(lib::mtr" -e "perl(mtr"
