# Copyright (c) 2008 MySQL AB
# Use is subject to license terms.
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
######################################################
# MySQL NDB Cluster Huge Sample Configuration File   #
######################################################
# This files assumes that you are using at least 9   #
# hosts for running the cluster. Hostnames and paths #
# listed below should be changed to match your setup #
######################################################

[NDBD DEFAULT]
NoOfReplicas: 2
DataDir: /add/path/here
FileSystemPath: /add/path/here

# Data Memory, Index Memory, and String Memory #
DataMemory: 6000M
IndexMemory: 1500M
StringMemory: 5

# Transaction Parameters #
MaxNoOfConcurrentTransactions: 4096
MaxNoOfConcurrentOperations: 100000
MaxNoOfLocalOperations: 100000

# Transaction Temporary Storage #
MaxNoOfConcurrentIndexOperations: 8192
MaxNoOfFiredTriggers: 4000
TransactionBufferMemory: 1M

# Scans and buffering #
MaxNoOfConcurrentScans: 300
MaxNoOfLocalScans: 32
BatchSizePerLocalScan: 64
LongMessageBuffer: 1M

# Logging and Checkpointing #
NoOfFragmentLogFiles: 300
FragmentLogFileSize: 16M
MaxNoOfOpenFiles: 40
InitialNoOfOpenFiles: 27
MaxNoOfSavedMessages: 25

# Metadata Objects #
MaxNoOfAttributes: 1500
MaxNoOfTables: 400
MaxNoOfOrderedIndexes: 200
MaxNoOfUniqueHashIndexes: 200
MaxNoOfTriggers: 770

# Boolean Parameters #
LockPagesInMainMemory: 0
StopOnError: 1
Diskless: 0
ODirect: 0

# Controlling Timeouts, Intervals, and Disk Paging #
TimeBetweenWatchDogCheck: 6000
TimeBetweenWatchDogCheckInitial: 6000
StartPartialTimeout: 30000
StartPartitionedTimeout: 60000
StartFailureTimeout: 1000000
HeartbeatIntervalDbDb: 2000
HeartbeatIntervalDbApi: 3000
TimeBetweenLocalCheckpoints: 20
TimeBetweenGlobalCheckpoints: 2000
TransactionInactiveTimeout: 0
TransactionDeadlockDetectionTimeout: 1200
DiskSyncSize: 4M
DiskCheckpointSpeed: 10M
DiskCheckpointSpeedInRestart: 100M
ArbitrationTimeout: 10

# Buffering and Logging #
UndoIndexBuffer: 2M
UndoDataBuffer: 1M
RedoBuffer: 32M
LogLevelStartup: 15
LogLevelShutdown: 3
LogLevelStatistic: 0
LogLevelCheckpoint: 0
LogLevelNodeRestart: 0
LogLevelConnection: 0
LogLevelError: 15
LogLevelCongestion: 0
LogLevelInfo: 3
MemReportFrequency: 0

# Backup Parameters #
BackupDataBufferSize: 2M
BackupLogBufferSize: 2M
BackupMemory: 64M
BackupWriteSize: 32K
BackupMaxWriteSize: 256K

[MGM DEFAULT]
PortNumber: 1186
DataDir: /add/path/here

[TCP DEFAULT]
SendBufferMemory: 2M

#######################################
# Change HOST1 to the name of the NDB_MGMD host
# Change HOST2 to the name of the NDB_MGMD host
# Change HOST3 to the name of the NDB_MGMD host
# Change HOST4 to the name of the NDBD host
# Change HOST5 to the name of the NDBD host
# Change HOST6 to the name of the NDBD host
# Change HOST7 to the name of the NDBD host
# Change HOST8 to the name of the NDBD host
# Change HOST9 to the name of the NDBD host
#######################################

[NDB_MGMD]
Id: 1
HostName: HOST1
ArbitrationRank: 1

[NDB_MGMD]
Id: 2
HostName: HOST2
ArbitrationRank: 1

[NDB_MGMD]
Id: 3
HostName: HOST3
ArbitrationRank: 1

[NDBD]
Id: 4
HostName: HOST4

[NDBD]
Id: 5
HostName: HOST5

[NDBD]
Id: 6
HostName: HOST6

[NDBD]
Id: 7
HostName: HOST7

[NDBD]
Id: 8
HostName: HOST8

[NDBD]
Id: 9
HostName: HOST9

######################################################
# Note: The following can be MySQLD connections or   #
#      NDB API application connecting to the cluster #
######################################################

[API]
Id: 10
HostName: HOST1
ArbitrationRank: 2

[API]
Id: 11
HostName: HOST2
ArbitrationRank: 2

[API]
Id: 12
HostName: HOST3

[API]
Id: 13
HostName: HOST4

[API]
Id: 14
HostName: HOST5

[API]
Id: 15
HostName: HOST6

[API]
Id: 16
HostName: HOST7

[API]
Id: 17
HostName: HOST8

[API]
Id: 19
HostName: HOST9

[API]
Id: 20

[API]
Id: 21

[API]
Id: 22

[API]
Id: 23

[API]
Id: 24

[API]
Id: 25

