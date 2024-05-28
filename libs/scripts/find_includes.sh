#!/bin/sh
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


if [ $# = 0 ]; then
    cat <<EOF
$0 {--output-tempfile-dir DIR} FILES...

Print all include directives in the given FILES.

PARAMETERS:

--output-tempfile-dir DIR  Generate a temporary file in directory DIR. Write
                           output to the file instead of to stdout.

OUTPUT FORMAT:

  Each output line has the form:

    <FILE>:<CHAR><INCFILE>

  where:

  - FILE is the file containing the #include directive
  - CHAR is either " or <, depending on the syntax used in the include directive.
  - INCFILE is the included file.
EOF
    exit
fi

# Determine where to write output.
tempfile=
if [ "$1" = --output-tempfile-dir ] ; then
    tempfile=`mktemp -p $2`
    shift
    shift
fi

# Find all lines in the input files that contain either an #include
# directive, or a comment/string/char delimiter, or line-continuation escape
rg --no-heading --no-line-number \
    '\s*#\s*include\s+(["<][^">]*)[">].*|"|/\*|\*/|//|\\$|'"'" \
    $* |

    # Trim comments
    ./libs/scripts/trim_comments.sh - |

    # Find all #include directives, and output just <FILE>:<CHAR><INCFILE>
    rg --replace '$1$2' \
        '^([^:]*:)\s*#\s*include\s+(["<][^">]*)[">].*' - |

    # Write to tempfile if requested. Otherwise, write to stdout.
    if [ "$tempfile" != '' ] ; then
        cat > $tempfile
    else
        cat
    fi
