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
