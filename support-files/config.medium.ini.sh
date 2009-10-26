#
# MySQL NDB Cluster Medium Sample Configuration File
#
# This files assumes that you are using at least 6
# hosts for running the cluster. Hostnames and paths
# listed below should be changed to match your setup
#

[NDBD DEFAULT]
NoOfReplicas: 2
DataDir: /add/path/here
FileSystemPath: /add/path/here


# Data Memory, Index Memory, and String Memory

DataMemory: 3000M
IndexMemory: 800M
BackupMemory: 64M

# Transaction Parameters

MaxNoOfConcurrentOperations: 100000
MaxNoOfLocalOperations: 100000

# Buffering and Logging

RedoBuffer: 16M

# Logging and Checkpointing

NoOfFragmentLogFiles: 200

# Metadata Objects

MaxNoOfAttributes: 500
MaxNoOfTables: 100

# Scans and Buffering

MaxNoOfConcurrentScans: 100


[MGM DEFAULT]
PortNumber: 1186
DataDir: /add/path/here

#
# Change HOST1 to the name of the NDB_MGMD host
# Change HOST2 to the name of the NDB_MGMD host
# Change HOST3 to the name of the NDBD host
# Change HOST4 to the name of the NDBD host
# Change HOST5 to the name of the NDBD host
# Change HOST6 to the name of the NDBD host
#

[NDB_MGMD]
Id: 1
HostName: HOST1
ArbitrationRank: 1

[NDB_MGMD]
Id: 2
HostName: HOST2
ArbitrationRank: 1

[NDBD]
Id: 3
HostName: HOST3

[NDBD]
Id: 4
HostName: HOST4

[NDBD]
Id: 5
HostName: HOST5

[NDBD]
Id: 6
HostName: HOST6

#
# Note: The following can be MySQLD connections or
#      NDB API application connecting to the cluster
#

[API]
Id: 7
HostName: HOST1
ArbitrationRank: 2

[API]
Id: 8
HostName: HOST2
ArbitrationRank: 2

[API]
Id: 9
HostName: HOST3
ArbitrationRank: 2

[API]
Id: 10
HostName: HOST4

[API]
Id: 11
HostName: HOST5

[API]
Id: 12
HostName: HOST6

[API]
Id: 13

[API]
Id: 14

[API]
Id: 15


