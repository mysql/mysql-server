#
# MySQL-shared-compat.spec
#
# RPM build instructions to create a "meta" package that includes two
# versions of the MySQL shared libraries (for compatibility with
# distributions that ship older versions of MySQL and do not provide a
# separate "MySQL-shared" package. This spec file simply repackages two
# already existing MySQL-shared RPMs into a single package.
# 
# Copyright (C) 2003 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc., 59
# Temple Place, Suite 330, Boston, MA  02111-1307  USA

#
# Change this to match the version of the shared libs you want to include
#
%define version41 @MYSQL_NO_DASH_VERSION@
%define version40 4.0.23
%define version3 3.23.58

Name:         MySQL-shared-compat
Packager:     Lenz Grimmer <build@mysql.com>
Vendor:       MySQL AB
License:      GPL
Group:        Applications/Databases
URL:          http://www.mysql.com/
Autoreqprov:  on
Version:      %{version41}
Release:      0
BuildRoot:    %{_tmppath}/%{name}-%{version}-build
Obsoletes:    MySQL-shared, mysql-shared
Provides:     MySQL-shared
Summary:      MySQL shared client libraries for MySQL %{version41}, %{version40} and %{version3}
# We simply use the "MySQL-shared" subpackages as input sources instead of
# rebuilding all from source
Source0:      MySQL-shared-%{version41}-0.%{_arch}.rpm
Source1:      MySQL-shared-%{version40}-0.%{_arch}.rpm
Source2:      MySQL-shared-%{version3}-1.%{_arch}.rpm
# No need to include the RPMs once more - they can be downloaded seperately
# if you want to rebuild this package
NoSource:     0
NoSource:     1
NoSource:     2
BuildRoot:    %{_tmppath}/%{name}-%{version}-build

%description
This package includes the shared libraries for both MySQL %{version3},
MySQL %{version40} as well as MySQL %{version41}.
Install this package instead of "MySQL-shared", if you have applications
installed that are dynamically linked against older versions of the MySQL
client library but you want to upgrade to MySQL 4.1.xx without breaking the
library dependencies.

%install
[ "$RPM_BUILD_ROOT" != "/" ] && [ -d $RPM_BUILD_ROOT ] && rm -rf $RPM_BUILD_ROOT;
mkdir -p $RPM_BUILD_ROOT
cd $RPM_BUILD_ROOT
rpm2cpio %{SOURCE0} | cpio -iv --make-directories
rpm2cpio %{SOURCE1} | cpio -iv --make-directories
rpm2cpio %{SOURCE2} | cpio -iv --make-directories
/sbin/ldconfig -n $RPM_BUILD_ROOT%{_libdir}

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && [ -d $RPM_BUILD_ROOT ] && rm -rf $RPM_BUILD_ROOT;

%files
%defattr(-, root, root)
%{_libdir}/libmysqlclient*
