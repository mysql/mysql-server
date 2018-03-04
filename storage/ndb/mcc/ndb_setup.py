#! /usr/bin/env python

# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

"""Launch script for the configurator backend. Parses command line options and starts the web server."""

import sys
import platform
import os.path
import mcc_config

def num_pyver(vn):
    if isinstance(vn, str): 
        return int(''.join(filter(str.isdigit, vn)))
    return vn

def num_py_major_minor_tuple():
    return map(num_pyver, platform.python_version_tuple()[0:2])

if __name__ == '__main__':
    if os.path.isabs(mcc_config.MCC_INSTALL_BINDIR):
        print "Running out of source dir..."
        # abs_install_bindir = mcc_config.MCC_INSTALL_BINDIR
        mcc_config.abs_install_subdir = mcc_config.MCC_INSTALL_BINDIR
        mcc_config.abs_install_frontenddir = os.path.normpath(os.path.join(mcc_config.MCC_INSTALL_BINDIR, mcc_config.MCC_INSTALL_FRONTENDDIR))
    else:
        print "Running out of install dir: "+os.path.dirname(os.path.abspath(sys.argv[0]))
        abs_install_bindir = os.path.dirname(os.path.abspath(sys.argv[0]))
        mcc_config.install_prefix = abs_install_bindir[0:abs_install_bindir.rindex(mcc_config.MCC_INSTALL_BINDIR)]
        mcc_config.abs_install_subdir = os.path.normpath(os.path.join(mcc_config.install_prefix, 
                                                                      mcc_config.MCC_INSTALL_SUBDIR))
        mcc_config.install_frontenddir = os.path.join(mcc_config.install_prefix, mcc_config.MCC_INSTALL_FRONTENDDIR)
    
    sys.path.append(mcc_config.abs_install_subdir)

    (pymajor, pyminor) = num_py_major_minor_tuple()
    assert (pymajor == 2 and pyminor >= 6), 'Unsupported Python version: '+str(platform.python_version())
    sys.path.append('/opt/csw/lib/python/site-packages')

    import request_handler
    request_handler.main(mcc_config.install_prefix, mcc_config.abs_install_subdir)
        
