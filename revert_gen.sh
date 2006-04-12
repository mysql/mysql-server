#!/bin/bash
#
# revert changes to all generated files. this is useful in some situations
# when merging changes between branches.

set -eu

svn revert include/pars0grm.h pars/pars0grm.h pars/lexyy.c pars/pars0grm.c
