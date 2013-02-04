# Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

#! /usr/bin/env python
#""""""

import sys
import optparse
import platform
import shutil
import os
import os.path
import zipfile
import tempfile

def num_pyver(vn):
    if isinstance(vn, str): 
        return int(filter(str.isdigit, vn))
    return vn

def num_py_major_minor_tuple():
    return map(num_pyver, platform.python_version_tuple()[0:2])

def add_dir_to_zip(zf, d):
    for dirpath, dirnames, filenames in os.walk(d):
        if '.hg' in dirpath:
            continue
        dst_dirpath = os.path.join('mcc', dirpath);
        print "adding directory:", dirpath, ' as ', dst_dirpath
        zf.write(dirpath, dst_dirpath)
        for f in filenames:
            print "adding file: ", os.path.join(dirpath, f), ' as ', os.path.join(dst_dirpath, f)
            zf.write(os.path.join(dirpath, f), os.path.join(dst_dirpath, f))

def get_entries(d):
    entries = []
    for dirpath, dirnames, filenames in os.walk(d):
        if dirpath.startswith('.hg'):
            continue

        entries.append(dirpath);
        for f in filenames:
            if f.endswith('.zip'):
                continue
            entries.append(os.path.join(dirpath, f))
    return entries

if __name__ == '__main__':
    (pymajor, pyminor) = num_py_major_minor_tuple()
    assert pymajor == 2 and pyminor >= 6, 'Unsupported Python version: '+platform.python_version()
    #sys.path.append('/usr/local/lib/bzr-2.1.0-2010.03/lib/python2.6/site-packages')
    print "sys.argv[0]:", os.path.normpath(sys.argv[0])
    cfgdir = os.path.dirname(sys.argv[0])
    if cfgdir == '':
        cfgdir = os.getcwd()
        
    cmdln_parser = optparse.OptionParser()
    cmdln_parser.add_option('-o', '--zip-name', action='store', type='string', default='mcc.zip', 
                            help='name of package zip file: [default: %default ]')
    cmdln_parser.add_option('-p', '--python-installation', action='store', type='string', default='/usr/local/cluwin/cluster-mgt/mcc-deps/python', 
                            help='location of python installation to bundle: [default: %default ]')

    (options, arguments) = cmdln_parser.parse_args()

    if os.path.isabs(options.zip_name):
        zipname = options.zip_name
    else:
        zipname = tempfile.NamedTemporaryFile().name
    
    mcc = zipfile.ZipFile(zipname, 'w')

    here = os.getcwd()
    print "cfgdir: ", cfgdir
    os.chdir(cfgdir)
    add_dir_to_zip(mcc, '.')

    os.chdir(os.path.dirname(options.python_installation))
    add_dir_to_zip(mcc, os.path.basename(options.python_installation))
    mcc.close()

    if not os.path.isabs(options.zip_name):
        os.chdir(here)
        tmpz = open(zipname, mode='rb')
        realzip = open(options.zip_name, 'wb')
        realzip.write(tmpz.read())
        realzip.close()
        tmpz.close()
        
        
