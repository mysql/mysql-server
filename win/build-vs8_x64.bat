@echo off

REM Copyright (C) 2006 MySQL AB
REM 
REM This program is free software; you can redistribute it and/or modify
REM it under the terms of the GNU General Public License as published by
REM the Free Software Foundation; version 2 of the License.
REM 
REM This program is distributed in the hope that it will be useful,
REM but WITHOUT ANY WARRANTY; without even the implied warranty of
REM MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
REM GNU General Public License for more details.
REM 
REM You should have received a copy of the GNU General Public License
REM along with this program; if not, write to the Free Software
REM Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

if exist cmakecache.txt del cmakecache.txt
copy win\vs8cache.txt cmakecache.txt
cmake -G "Visual Studio 8 2005 Win64"
copy cmakecache.txt win\vs8cache.txt
