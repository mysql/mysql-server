# Copyright (c) 2005 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
#
# Example Ndbcluster storage engine config file.
#
[ndbd default]
NoOfReplicas= 2
MaxNoOfConcurrentOperations= 10000
DataMemory= 80M
IndexMemory= 24M
TimeBetweenWatchDogCheck= 30000
DataDir= /var/lib/mysql-cluster
MaxNoOfOrderedIndexes= 512

[ndb_mgmd default]
DataDir= /var/lib/mysql-cluster

[ndb_mgmd]
Id=1
HostName= localhost

[ndbd]
Id= 2
HostName= localhost

[ndbd]
Id= 3
HostName= localhost

[mysqld]
Id= 4

[mysqld]
Id= 5

[mysqld]
Id= 6

[mysqld]
Id= 7

# choose an unused port number
# in this configuration 63132, 63133, and 63134
# will be used
[tcp default]
PortNumber= 63132
