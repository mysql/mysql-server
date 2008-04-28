#
# MySQL NDB Cluster Small Sample Configuration File
#
# This files assumes that you are using 1 to 3 hosts
# for running the cluster. Hostnames and paths listed
# below should be changed to match your setup.
#
# Note: You can change localhost for a different host
#

[NDBD DEFAULT]
NoOfReplicas: 2
DataDir: /add/path/here
FileSystemPath: /add/path/here

# Data Memory, Index Memory, and String Memory

DataMemory: 600M
IndexMemory: 100M
BackupMemory: 64M

[MGM DEFAULT]
PortNumber: 1186
DataDir: /add/path/here

[NDB_MGMD]
Id: 1
HostName: localhost
ArbitrationRank: 1

[NDBD]
Id: 2
HostName: localhost

[NDBD]
Id: 3
HostName: localhost

#
# Note: The following can be MySQLD connections or
#      NDB API application connecting to the cluster
#

[API]
Id: 4
HostName: localhost
ArbitrationRank: 2

[API]
Id: 5
HostName: localhost

[API]
Id: 6
HostName: localhost

[API]
Id: 7

[API]
Id: 8

[API]
Id: 9

