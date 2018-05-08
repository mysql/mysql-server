/*
Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

/******************************************************************************
 ***                                                                        ***
 ***                   Configuration parameter definitions                  ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.configuration.parameters
 *
 *  Description:
 *      Array of relevant configuration parameters and access functions
 *
 *  External interface: 
 *      mcc.configuration.parameters.getPara: Retrieve parameter attribute
 *      mcc.configuration.parameters.setPara: Assign to parameter attribute
 *      mcc.configuration.parameters.visiblePara: Check visibility
 *      mcc.configuration.parameters.isHeading: Check if this is a heading
 *      mcc.configuration.parameters.getAllPara: Retrieve all parameters
 *      mcc.configuration.parameters.resetDefaultValueInstance: Reset predef val
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      None
 *
 *  Internal data: 
 *      processParameterDefaults: Array of all parameters
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
        Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.configuration.parameters");

dojo.require("mcc.util");
dojo.require("mcc.storage");
dojo.require("mcc.configuration");
/**************************** External interface  *****************************/

mcc.configuration.parameters.getPara = getPara;
mcc.configuration.parameters.setPara = setPara;
mcc.configuration.parameters.visiblePara = visiblePara;
mcc.configuration.parameters.isHeading = isHeading;
mcc.configuration.parameters.getAllPara = getAllPara;
mcc.configuration.parameters.resetDefaultValueInstance = 
        resetDefaultValueInstance;

/******************************* Internal data ********************************/
// Layout details, defaults, names, labels, etc. hashed on type and name
var processParameterDefaults= {
    "management": {
        parameters: {
/********************************** General ***********************************/
            NodeIdHeading: {
                label: "<br><b>Node identity and directories</b>",
                heading: true,
                visibleType: true,
                visibleInstance: true,
                advancedLevel: false
            },
            NodeId: {
                label: "NodeId",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-mgm-definition.html#ndbparam-mgmd-nodeid",
                tooltip: "Number identifying the management server node " +
                        "(ndb_mgmd(MGM))",
                constraints: {min: 1, max: 255, places: 0, pattern: "#"},
                attribute: "NodeId",
                destination: "config.ini",
                overridableType: false,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true
            },
            HostName: {
                label: "HostName",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-mgm-definition.html#ndbparam-mgmd-hostname",
                tooltip: "Name of computer for this node",
                attribute: "HostName",
                destination: "config.ini",
                overridableType: false,
                overridableInstance: false,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true
            },
            DataDir: {
                label: "DataDir",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-mgm-definition.html#ndbparam-mgmd-datadir",
                tooltip: "Data directory for this node",
                attribute: "DataDir",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true
            },
            ArbitrationRank: {
                label: "ArbitrationRank",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-mgm-definition.html",
                attribute: "ArbitrationRank",
                tooltip: "0: The node will never be used as an arbitrator " +
                         "1: The node is high priority arbitrator (default) " +
                         "2: Low-priority arbitrator node",
                constraints: {min: 0, max: 2, places: 0, pattern: "#"},
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 1,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true
            },
            /*ArbitrationDelay {} Skipped, irrelevant.*/
            /*PortNumberStats{}   Skipped, do not know where to put in MCC interface.*/
            /*WAN: Skipped, will add another parameter that'll make this deprecated soon */
            /*HeartbeatThreadPriority {} Skipped, seems too fine-grained for MCC.*/
            TotalSendBufferMemory: {
                label: "TotalSendBufferMemory",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-config-send-buffers.html",
                attribute: "TotalSendBufferMemory",
                tooltip: "Total amount of memory to allocate on this node " +
                         "for shared send buffer memory. 0-disabled.",
                constraints: {min: 256, max: 4194303, places: 0, pattern: "#"},
                suffix: "K",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined, //IF set to 0 breaks logic (minval=256K)!
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true
            },
            HeartbeatIntervalMgmdMgmd: {
                label: "HBeatIntervalMgmdMgmd",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-mgm-definition.html",
                attribute: "HeartbeatIntervalMgmdMgmd",
                tooltip: "Time in ms between heartbeat messages used to " +
                         "determine if another mgmt node is in contact." +
                         "After missing 3 intervals, conn.is declared dead.",
                constraints: {min: 100, max: 4294967039, places: 0, pattern: "#"},
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 1500,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true
            },
            //LogDestination {} NOT in since, for example, CONSOLE makes no sense in MCC.
/******************************** Communication *******************************/
            CommunicationHeading: {
                label: "<br><b>Communication</b>",
                heading: true,
                visibleType: true,
                visibleInstance: true,
                advancedLevel: false
            },
            Portbase: {
                label: "Portbase",
                attribute: "Portbase",
                tooltip: "Portbase is not a MySQL Cluster configuration " +
                        "parameter, but is used for convenience to allow " +
                        "changing the base for allocating port numbers for " +
                        "individual ndb_mgmd processes",
                constraints: {min: 0, max: 65535, places: 0, pattern: "#"},
                destination: "mcc",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 1186,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false
            },
            Portnumber: {
                label: "Portnumber",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-mgm-definition.html#ndbparam-mgmd-portnumber",
                tooltip: "Port number to give commands to/fetch " + 
                        "configurations from management server",
                constraints: {min: 0, max: 65535, places: 0, pattern: "#"},
                attribute: "Portnumber",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true
            }
        }
    },
    "data": {
        parameters: {
/*********************************** General **********************************/
            NodeIdHeading: {
                label: "<br><b>Node identity and directories</b>",
                heading: true,
                visibleType: true,
                visibleInstance: true,
                advancedLevel: false
            },
            NodeId: {
                label: "NodeId",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-nodeid", 
                tooltip: "Number identifying the database node (ndbd(DB))",
                constraints: {min: 0, max: 48, places: 0, pattern: "#"},
                attribute: "NodeId",
                destination: "config.ini",
                overridableType: false,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true
            },
            HostName: {
                label: "HostName",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-hostname", 
                tooltip: "Name of computer for this node",
                attribute: "HostName",
                destination: "config.ini",
                overridableType: false,
                overridableInstance: false,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true
            },
            DataDir: {
                label: "DataDir",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-datadir", 
                tooltip: "Data directory for this node",
                attribute: "DataDir",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true
            },
            FileSystemPath: {
                label: "FileSystemPath",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-mgm-definition.html#ndbparam-ndbd-filesystempath",
                tooltip: "Directory for all files created for metadata, " + 
                         "REDO/UNDO logs and data files. Default is DataDir",
                attribute: "FileSystemPath",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true
            },
            NoOfReplicas: {
                label: "NoOfReplicas",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-noofreplicas", 
                tooltip: "Number of copies of all data in the database (1-4)",
                constraints: {min: 1, max: 4, places: 0, pattern: "#"},
                attribute: "NoOfReplicas",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 2,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            ServerPort: {
                label: "ServerPort",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-ServerPort", 
                tooltip: "If you need to be able to open specific ports in a " + 
                    "firewall to permit communication between data nodes and API " + 
                    "nodes (including SQL nodes), you can set this parameter to " + 
                    "the number of the desired port",
                constraints: {min: 0, max: 65535, places: 0, pattern: "#"},
                attribute: "ServerPort",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LateAlloc: {
                label: "LateAlloc",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-latealloc", 
                tooltip: "Alloc. mem. for this data node after a connection " +
                         "to the management server has been established",
                constraints: {min: 0, max: 1, places: 0, pattern: "#"},
                attribute: "LateAlloc",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 1,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            NodeGroup: { 
                //Node group we calculate on the basis of NoOfReplicas!
                //So set RO here (just a placeholder).
                label: "NodeGroup",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-nodegroup", 
                tooltip: "Use to add new node group to running Cluster " +
                         "without rolling restart. For this, set it to " +
                         "65536 (MAX val). You are not required to set " +
                         "this value for all cluster data nodes, only " +
                         "for those which are to be started and added " +
                         "to cluster as a new node group at a later time",
                constraints: {min: 0, max: 65536, places: 0, pattern: "#"},
                attribute: "NodeGroup",
                destination: "config.ini",
                overridableType: false,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true,
                advancedLevel: true
            },
            StartNoNodeGroupTimeout: {
                label: "StartNoNodeGroupTOut",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-startnonodegrouptimeout", 
                tooltip: "Wait-time for nodes with node group set to 65536 " +
                         "These nodes are not needed to get cluster up, so " +
                         "we wait for them a while (default = no wait).",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "StartNoNodeGroupTimeout",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            HeartbeatOrder: {
                label: "HeartbeatOrder",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-heartbeatorder", 
                tooltip: "Please read the manual before setting to anything " +
                         "other than 0 (default)",
                constraints: {min: 0, max: 65535, places: 0, pattern: "#"},
                attribute: "HeartbeatOrder",
                destination: "config.ini",
                overridableType: false,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true,
                advancedLevel: true
            },
            MaxBufferedEpochs: {
                label: "MaxBufferedEpochs",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-maxbufferedepochs", 
                tooltip: "# of unprocessed epochs by which a subscribing " +
                         "node can lag behind",
                constraints: {min: 0, max: 100000, places: 0, pattern: "#"},
                attribute: "MaxBufferedEpochs",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 100,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxBufferedEpochBytes: {
                label: "MaxBufferedEpochBytes",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-maxbufferedepochbytes", 
                tooltip: "Total # bytes allocated for buffering epochs by " +
                         "this node",
                constraints: {min: 26214400, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxBufferedEpochBytes",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 26214400,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxDiskWriteSpeed: {
                label: "MaxDiskWriteSpeed(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxdiskwritespeed", 
                tooltip: "MAX rate of disk writes (MB/s) by LCP and backup " +
                         "operations.",
                constraints: {min: 1, max: 1048576, places: 0, pattern: "#"},
                attribute: "MaxDiskWriteSpeed",
                destination: "config.ini",
                suffix: "M",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 20,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MinDiskWriteSpeed: {
                label: "MinDiskWriteSpeed(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-mindiskwritespeed", 
                tooltip: "MIN rate of disk writes (MB/s) by LCP and backup " +
                         "operations.",
                constraints: {min: 1, max: 1048576, places: 0, pattern: "#"},
                attribute: "MinDiskWriteSpeed",
                destination: "config.ini",
                suffix: "M",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 10,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxDiskWriteSpeedOtherNodeRestart: {
                label: "MaxDiskWriteSpeedOtherNR(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxdiskwritespeedothernoderestart", 
                tooltip: "MAX rate of disk writes (MB/s) by LCP and backup " +
                         "operations when 1 or more data nodes in Cluster " +
                         "are restarting",
                constraints: {min: 1, max: 1048576, places: 0, pattern: "#"},
                attribute: "MaxDiskWriteSpeedOtherNodeRestart",
                destination: "config.ini",
                suffix: "M",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 50,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxDiskWriteSpeedOwnRestart: {
                label: "MaxDiskWriteSpeedOwnNR(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxdiskwritespeedownrestart", 
                tooltip: "MAX rate of disk writes (MB/s) by LCP and backup " +
                         "operations while this data node is restarting",
                constraints: {min: 1, max: 1048576, places: 0, pattern: "#"},
                attribute: "MaxDiskWriteSpeedOwnRestart",
                destination: "config.ini",
                suffix: "M",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 200,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
/*********************************** Backup ***********************************/
            BackupHeading: {
                label: "<br><b>Backup parameters</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            BackupDataDir: {
                label: "BackupDataDir",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-mgm-definition.html#ndbparam-ndbd-backupdatadir",
                tooltip: "Place for backups.'/BACKUP' is always appended." + 
                         "Default is FileSystemPath/BACKUP",
                attribute: "BackupDataDir",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true
            },
            BackupMaxWriteSize: {
                label: "BackupMaxWriteSize(KB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-backupmaxwritesize", 
                tooltip: "Max size of filesystem writes made by backup",
                constraints: {min: 256, max: 4194304, places: 0, pattern: "#"},
                attribute: "BackupMaxWriteSize",
                destination: "config.ini",
                suffix: "K",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 1024,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            BackupWriteSize: {
                label: "BackupWriteSize(KB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-backupwritesize", 
                tooltip: "Size of filesystem writes made by backup",
                constraints: {min: 32, max: 4194304, places: 0, pattern: "#"},
                attribute: "BackupWriteSize",
                destination: "config.ini",
                suffix: "K",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 256,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            BackupDataBufferSize: {
                label: "BackupDataBufferSz(KB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-backupdatabuffersize", 
                tooltip: "Default size of databuffer for a backup",
                constraints: {min: 512, max: 4194304, places: 0, pattern: "#"},
                attribute: "BackupDataBufferSize",
                destination: "config.ini",
                suffix: "K",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 1024,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            BackupLogBufferSize: {
                label: "BackupLogBufferSz(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-backuplogbuffersize", 
                tooltip: "Default size of logbuffer for a backup",
                constraints: {min: 2, max: 4096, places: 0, pattern: "#"},
                attribute: "BackupLogBufferSize",
                destination: "config.ini",
                suffix: "M",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 16,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            BackupReportFrequency: {
                label: "BackupReportFrequency",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-backupreportfrequency", 
                tooltip: "Frequency of backup status reports during backup " + 
                        "in seconds",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "BackupReportFrequency",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 10,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            BackupDiskWriteSpeedPct: {
                label: "BackupDiskWrSpeedPct",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-backupdiskwritespeedpct", 
                tooltip: "Node's max write rate budget reserved prior to " +
                         "sharing out the remainder of the budget among " +
                         "LDM threads for LCPs.",
                constraints: {min: 0, max: 90, places: 0, pattern: "#"},
                attribute: "BackupDiskWriteSpeedPct",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 50,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            CompressedBackup: {
                label: "CompressedBackup",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-compressedbackup", 
                tooltip: "Setting causes backup files to be compressed with " +
                         "equivalent of gzip --fast",
                attribute: "CompressedBackup",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: true,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            CompressedLCP: {
                label: "CompressedLCP",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-compressedlcp", 
                tooltip: "Setting causes local checkpoint files to be " +
                         "compressed with equivalent of gzip --fast",
                attribute: "CompressedLCP",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: false,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
/*********************************** Logging **********************************/
            LoggingHeading: {
                label: "<br><b>Log handling</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MemReportFrequency: {
                label: "MemReportFrequency",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-memreportfrequency", 
                tooltip: "Frequency of mem reports in seconds, 0 = only when " +
                        "passing %-limits",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MemReportFrequency",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0, //30,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LogLevelStartup: {
                label: "LogLevelStartup",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-loglevelstartup", 
                tooltip: "Node startup info printed on stdout(15),0-disabled",
                constraints: {min: 0, max: 15, places: 0, pattern: "#"},
                attribute: "LogLevelStartup",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 1,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LogLevelShutdown: {
                label: "LogLevelShutdown",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-loglevelshutdown", 
                tooltip: "Node shutdowninfo printed on stdout(15),0-disabled",
                constraints: {min: 0, max: 15, places: 0, pattern: "#"},
                attribute: "LogLevelShutdown",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LogLevelStatistic: {
                label: "LogLevelStatistic",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-loglevelstatistic", 
                tooltip: "Reporting level for statistical events.",
                constraints: {min: 0, max: 15, places: 0, pattern: "#"},
                attribute: "LogLevelStatistic",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LogLevelCheckpoint: {
                label: "LogLevelCheckpoint",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-loglevelcheckpoint", 
                tooltip: "Local and Global checkpoint info printed on stdout" +
                         "(15),0-disabled",
                constraints: {min: 0, max: 15, places: 0, pattern: "#"},
                attribute: "LogLevelCheckpoint",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LogLevelNodeRestart: {
                label: "LogLevelNodeRestart",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-loglevelnoderestart", 
                tooltip: "Node restart, node failure info printed on stdout " +
                         "(15),0-disabled",
                constraints: {min: 0, max: 15, places: 0, pattern: "#"},
                attribute: "LogLevelNodeRestart",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LogLevelConnection: {
                label: "LogLevelConnection",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-loglevelconnection", 
                tooltip: "Reporting level for events generated by " + 
                         "connections between nodes (15-stdout, 0-disabled)",
                constraints: {min: 0, max: 15, places: 0, pattern: "#"},
                attribute: "LogLevelConnection",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LogLevelError: {
                label: "LogLevelError",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-loglevelerror", 
                tooltip: "Events generated by errors and warnings not " + 
                         "causing node failure but worth reporting " +
                         "(15-stdout, 0-disabled).",
                constraints: {min: 0, max: 15, places: 0, pattern: "#"},
                attribute: "LogLevelError",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LogLevelCongestion: {
                label: "LogLevelCongestion",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-loglevelcongestion", 
                tooltip: "Reporting level for events generated by congestion " + 
                         "not causing node failure but worth reporting.",
                constraints: {min: 0, max: 15, places: 0, pattern: "#"},
                attribute: "LogLevelCongestion",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LogLevelInfo: {
                label: "LogLevelInfo",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-loglevelinfo", 
                tooltip: "Reporting level for events generated for " + 
                         "info about the general state of the cluster.",
                constraints: {min: 0, max: 15, places: 0, pattern: "#"},
                attribute: "LogLevelInfo",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            EventLogBufferSize: {
                label: "EventLogBufferSize(KB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-eventlogbuffersize", 
                tooltip: "Size of circular buffer used for log events in " +
                         "data nodes",
                constraints: {min: 0, max: 64, places: 0, pattern: "#"},
                attribute: "EventLogBufferSize",
                destination: "config.ini",
                suffix: "K",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 8,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            StartupStatusReportFrequency: {
                label: "StartupStatusRptFreq(s)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-startupstatusreportfrequency", 
                tooltip: "If 0, then reports are written to the cluster log " + 
                         "only at the beginning and at the completion of " + 
                         "the redo log file init. process.",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "StartupStatusReportFrequency",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
/********************************** Data memory *******************************/
            DataMemoryHeading: {
                label: "<br><b>Data and index memory</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            DataMemory: {
                label: "DataMemory(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-datamemory", 
                tooltip: "Number of Mbytes on each ndbd(DB) node allocated " +
                        "for storing data",
                constraints: {min: 1, max: 1048576, places: 0, pattern: "#"},
                attribute: "DataMemory",
                suffix: "M",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            IndexMemory: {
                label: "IndexMemory(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-indexmemory", 
                tooltip: "Number of Mbytes on each ndbd(DB) node allocated " +
                        "for storing indexes",
                constraints: {min: 1, max: 1048576, places: 0, pattern: "#"},
                attribute: "IndexMemory",
                suffix: "M",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MinFreePct: {
                label: "MinFreePct",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-minfreepct", 
                tooltip: "Pct of data node resources incl. DataMemory and " + 
                         "IndexMemory kept in reserve so that the data node " + 
                         "does not exhaust its memory while restarting.",
                constraints: {min: 0, max: 100, places: 0, pattern: "#"},
                attribute: "MinFreePct",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 5,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
/*********************************** Metadata *********************************/
            MetadataHeading: {
                label: "<br><b>Metadata storage</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxNoOfAttributes: {
                label: "MaxNoOfAttributes",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxnoofattributes", 
                tooltip: "Suggested MAX # of attributes that can be defined " + 
                         "in the cluster (like MaxNoOfTables etc).",
                constraints: {min: 32, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxNoOfAttributes",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 1000,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxNoOfTables: {
                label: "MaxNoOfTables",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxnooftables", 
                tooltip: "Total number of tables stored in the database",
                constraints: {min: 8, max: 20320, places: 0, pattern: "#"},
                attribute: "MaxNoOfTables",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 128,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxNoOfOrderedIndexes: {
                label: "MaxNoOfOrderedIndexes",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxnooforderedindexes", 
                tooltip: "Total number of ordered indexes in use in " +
                         "the system.",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxNoOfOrderedIndexes",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 128,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxNoOfTriggers: {
                label: "MaxNoOfTriggers",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxnooftriggers", 
                tooltip: "Total number of triggers that can be defined in " +
                        "the system",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxNoOfTriggers",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 768,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxNoOfSubscriptions: {
                label: "MaxNoOfSubscriptions",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxnoofsubscriptions", 
                tooltip: "Each NDB table in an NDB Cluster requires subscr " + 
                         "in the NDB kernel. 0 is treated as MaxNoOfTables.",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxNoOfSubscriptions",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            MaxNoOfSubscribers: {
                label: "MaxNoOfSubscribers",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxnoofsubscribers", 
                tooltip: "Only when using NDB Cluster Replication. The " + 
                         "default is 0, which is treated as 2*MaxNoOfTables",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxNoOfSubscribers",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            MaxNoOfConcurrentSubOperations: {
                label: "Max#ConcurrSubOps",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxnoofconcurrentsuboperations", 
                tooltip: "MAX # of operations that can be performed by all " + 
                         "API nodes in the cluster at one time.",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxNoOfConcurrentSubOperations",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 256,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            StringMemory: {
                label: "StringMemory",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-stringmemory", 
                tooltip: "Default size of string memory (1-100 -> %of max, " +
                        ">100 -> actual bytes)",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "StringMemory",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 25,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxAllocate: {
                label: "MaxAllocate(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxallocate", 
                tooltip: "MAX size of memory unit to use when allocating " +
                         "memory for tables",
                constraints: {min: 1, max: 1024, places: 0, pattern: "#"},
                attribute: "MaxAllocate",
                suffix: "M",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 32,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            // MaxNoOfOpenFiles and InitialNoOfOpenFiles not used, normally not
            // needed to change
            /* NO, weird, DEFAULT=0, MINVAL=20 MaxNoOfOpenFiles: {
                label: "MaxNoOfOpenFiles",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxnoofopenfiles", 
                tooltip: "# of internal threads to allocate for open files",
                constraints: {min: 20, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxNoOfOpenFiles",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            InitialNoOfOpenFiles: {
                label: "InitialNoOfOpenFiles",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-initialnoofopenfiles", 
                tooltip: "Initial # of intern. threads to allocate for open " + 
                         "files",
                constraints: {min: 20, max: 4294967039, places: 0, pattern: "#"},
                attribute: "InitialNoOfOpenFiles",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 27,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },*/
            MaxNoOfSavedMessages: {
                label: "MaxNoOfSavedMessages",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxnoofsavedmessages", 
                tooltip: "MAX # of errors written in the error log & MAX # " +
                         "of trace files kept before overwriting existing",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxNoOfSavedMessages",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 25,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            /*MaxLCPStartDelay: { removed as should not be needed
                label: "MaxLCPStartDelay (sec)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxlcpstartdelay", 
                tooltip: "# of seconds the cluster can wait to begin local " +
                         "checkpoint while data nodes are sync. metadata",
                constraints: {min: 0, max: 600, places: 0, pattern: "#"},
                attribute: "MaxLCPStartDelay",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },*/
            LcpScanProgressTimeout: {
                label: "LcpScanProgressTOut(s)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-lcpscanprogresstimeout", 
                tooltip: "MAX time for which the local checkpoint can be " +
                         "stalled before the LCP frag. scan watchdog shuts " +
                         "down the node (0-disabled)",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "LcpScanProgressTimeout",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 60,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            Diskless: {
                label: "Diskless",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-diskless", 
                tooltip: "If TRUE, causes entire cluster to operate only " +
                         "in main memory (no logging & checkpointing)",
                attribute: "Diskless",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: false,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LockPagesInMainMemory: {
                label: "LockPagesInMainMemory",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-lockpagesinmainmemory", 
                tooltip: "0: Disables locking(default)" +
                         "1: Lock after allocating memory for the process" +
                         "2: Lock before memory for the process is allocated",
                constraints: {min: 0, max: 2, places: 0, pattern: "#"},
                attribute: "LockPagesInMainMemory",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 1,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            ODirect: {
                label: "ODirect",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-odirect", 
                tooltip: "Attempt using O_DIRECT writes for LCP, backups, " +
                         "and redo logs; Linux kernel 2.6+",
                attribute: "ODirect",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            Arbitration: {
                label: "Arbitration",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-arbitration", 
                tooltip: "Choice of arbitration schemes. Use only in the [ndbd default] "+
                         "section of the cluster configuration file. The behavior of " +
                         "the cluster is unspecified when Arbitration is set to different " +
                         "values for individual data nodes.",
                attribute: "Arbitration",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.FilteringSelect,
                width: "75%",
                constraints: "Default, Disabled, WaitExternal",
                defaultValueType: "Default",
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
/*********************************** Disk data ********************************/
            DiskDataHeading: {
                label: "<br><b>Disk data storage</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            DiskPageBufferEntries: {
                label: "DiskPageBufferEntries",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-diskpagebufferentries", 
                tooltip: "# page buffer entries for each buffer page (32k)" +
                         "to allocate.",
                constraints: {min: 1, max: 100, places: 0, pattern: "#"},
                attribute: "DiskPageBufferEntries",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 10,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            DiskPageBufferMemory: {
                label: "DiskPageBufferMem(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-diskpagebuffermemory", 
                tooltip: "Number of Mbytes on each ndbd(DB) node allocated " +
                        "for disk page buffer cache",
                constraints: {min: 4, max: 1048576, places: 0, pattern: "#"},
                attribute: "DiskPageBufferMemory",
                suffix: "M",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 64,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            SharedGlobalMemory: {
                label: "SharedGlobalMemory(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-sharedglobalmemory", 
                tooltip: "Total number of Mbytes on each ndbd(DB) node " +
                        "allocated for any use",
                constraints: {min: 0, max: 67108864, places: 0, pattern: "#"},
                attribute: "SharedGlobalMemory",
                suffix: "M",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 128,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            DiskIOThreadPool: {
                label: "DiskIOThreadPool(thd)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-diskiothreadpool", 
                tooltip: "Number of unbound threads used for Disk Data file access.",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "DiskIOThreadPool",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 2,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            DiskSyncSize: {
                label: "DiskSyncSize(kB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-disksyncsize", 
                tooltip: "MAX #of bytes to store before flushing to LCP " +
                         "to prevent excessive write buffering. Ignored " +
                         "when ODIRECT is set.",
                constraints: {min: 32, max: 4194303, places: 0, pattern: "#"},
                attribute: "DiskSyncSize",
                destination: "config.ini",
                suffix: "K",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 4096,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            FileSystemPathDD: {
                label: "FileSystemPathDD",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-mgm-definition.html#ndbparam-ndbd-filesystempathdd",
                tooltip: "Place for Cluster Disk Data and undo log files. " +
                          "This can be overridden for data files, undo log " +
                          "files, or both, by specifying values for " +
                          "FileSystemPathDataFiles,FileSystemPathUndoFiles or " +
                          " both. If specified for data node (including in " +
                          "[ndbd default] section of the config.ini file),then " +
                          "starting data node with --initial causes all files " +
                          "in the directory to be deleted!",
                attribute: "FileSystemPathDD",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            FileSystemPathDataFiles: {
                label: "FileSystemPathDataFiles",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-mgm-definition.html#ndbparam-ndbd-filesystempathdatafiles",
                tooltip: "Place for Cluster Disk Data files. This overrides " +
                         "setting in FileSystemPathDD (which is the default)." +
                         " If specified for data node (including in " +
                         "[ndbd default] section of the config.ini file),then " +
                         "starting data node with --initial causes all files " +
                         "in the directory to be deleted!",
                attribute: "FileSystemPathDataFiles",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            FileSystemPathUndoFiles: {
                label: "FileSystemPathUndoFiles",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-mgm-definition.html#ndbparam-ndbd-FileSystemPathUndoFiles",
                tooltip: "Place for Cluster undo log files. This overrides " +
                         "setting in FileSystemPathDD (which is the default)." +
                         " If specified for data node (including in " +
                         "[ndbd default] section of the config.ini file),then " +
                         "starting data node with --initial causes all files " +
                         "in the directory to be deleted!",
                attribute: "FileSystemPathUndoFiles",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            InitialLogFileGroup: {
                label: "InitialLogFileGroup",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-mgm-definition.html#ndbparam-ndbd-InitialLogFileGroup",
                tooltip: "Specifies a log file group that is created when " +
                         "performing an initial start of the cluster. ",
                attribute: "InitialLogFileGroup",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            InitialTablespace: {
                label: "InitialTablespace",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-mgm-definition.html#ndbparam-ndbd-initialtablespace",
                tooltip: "Specifies tablespace created when performing " +
                         "initial start of the cluster.",
                attribute: "InitialTablespace",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
/********************************* Communication ******************************/
            CommunicationHeading: {
                label: "<br><b>Communication buffers</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            SendBufferMemory: {
                label: "SendBufferMemory(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-sendbuffermemory", 
                tooltip: "MBytes of buffer for signals sent from this node",
                constraints: {min: 1, max: 4096, places: 0, pattern: "#"},
                attribute: "SendBufferMemory",
                suffix: "M",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            ReceiveBufferMemory: {
                label: "ReceiveBufferMem(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-receivebuffermemory", 
                tooltip: "MBytes of buffer for signals received by this node",
                constraints: {min: 1, max: 4096, places: 0, pattern: "#"},
                attribute: "ReceiveBufferMemory",
                suffix: "M",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LongMessageBuffer: {
                label: "LongMessageBuffer(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-longmessagebuffer", 
                tooltip: "Number of Mbytes on each ndbd(DB) node allocated " +
                        "for internal long messages",
                constraints: {min: 1, max: 4096, places: 0, pattern: "#"},
                attribute: "LongMessageBuffer",
                suffix: "M",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 64, //32, actual min is 512kB...
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxParallelCopyInstances: {
                label: "MaxParallelCopyInstan",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxparallelcopyinstances", 
                tooltip: "Parallel. in copy phase of node or system restart",
                constraints: {min: 0, max: 64, places: 0, pattern: "#"},
                attribute: "MaxParallelCopyInstances",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
/******************************** Transactions ********************************/
            TransactionHeading: {
                label: "<br><b>Transaction handling</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxNoOfConcurrentTransactions: {
                label: "MaxNoOfConcTransact",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-maxnoofconcurrenttransactions", 
                tooltip: "Max number of transaction executing concurrently " +
                        "on the ndbd(DB) node",
                constraints: {min: 32, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxNoOfConcurrentTransactions",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 4096, //16384,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxNoOfConcurrentOperations: {
                label: "MaxNoOfConcOps",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-maxnoofconcurrentoperations", 
                tooltip: "Set min. to # of concurrently updated records in " + 
                         "transactions divided by # of cluster data nodes.",
                constraints: {min: 32, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxNoOfConcurrentOperations",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 131072,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxNoOfLocalOperations: {
                label: "MaxNoOfLocalOps",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-maxnooflocaloperations", 
                tooltip: "For many simultaneous transactions, none too big, " + 
                         "set to 1.1 × MaxNoOfConcurrentOperations.",
                constraints: {min: 32, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxNoOfLocalOperations",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxDMLOperationsPerTransaction: {
                label: "MaxDMLOpsPerTransact",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-maxdmloperationspertransaction", 
                tooltip: "Transaction is aborted if it requires more than " +
                         "this many DML operations. 0 means no-limit.",
                constraints: {min: 32, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxDMLOperationsPerTransaction",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 4294967295,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxNoOfConcurrentScans: {
                label: "MaxNoOfConcScans",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-maxnoofconcurrentscans", 
                tooltip: "# of parallel scans performed in the cluster",
                constraints: {min: 2, max: 500, places: 0, pattern: "#"},
                attribute: "MaxNoOfConcurrentScans",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 256,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxNoOfLocalScans: {
                label: "MaxNoOfLocalScans",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-maxnooflocalscans", 
                tooltip: "# of local scan recs if many scans are not fully " + 
                         "parallelized.4*MaxNoOfConcurrentScans*[#dnodes]+2",
                constraints: {min: 32, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxNoOfLocalScans",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxParallelScansPerFragment: {
                label: "MaxParallelScans/Frag",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-maxparallelscansperfragment", 
                tooltip: "# of TUP&TUX scans allowed before they begin " +
                         "queuing for serial handling",
                constraints: {min: 1, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxParallelScansPerFragment",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 256,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            BatchSizePerLocalScan: {
                label: "BatchSizePerLocalScan",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-batchsizeperlocalscan", 
                tooltip: "Used to calculate the number of lock records for " +
                        "scan with hold lock",
                constraints: {min: 1, max: 992, places: 0, pattern: "#"},
                attribute: "BatchSizePerLocalScan",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 256,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
/****************************** Transaction log *******************************/
            TransactionLogHeading: {
                label: "<br><b>Transaction logging</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            FragmentLogFileSize: {
                label: "FragmentLogFileSz(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-fragmentlogfilesize", 
                tooltip: "Size of each Redo log file",
                constraints: {min: 4, max: 1024, places: 0, pattern: "#"},
                attribute: "FragmentLogFileSize",
                suffix: "M",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 256,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            NoOfFragmentLogFiles: {
                label: "NoOfFragmentLogFiles",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-nooffragmentlogfiles", 
                tooltip: "No of Redo log files in each of the file group " +
                        "belonging to ndbd(DB) node",
                constraints: {min: 3, max: 4294967039, places: 0, pattern: "#"},
                attribute: "NoOfFragmentLogFiles",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 8,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            InitFragmentLogFiles: {
                label: "InitFragmentLogFiles",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-initfragmentlogfiles", 
                tooltip: "(not) all bytes are necessarily written to disk",
                constraints: "SPARSE,FULL",
                attribute: "InitFragmentLogFiles",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.FilteringSelect,
                width: "50%",
                defaultValueType: "SPARSE",
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            RedoBuffer: {
                label: "RedoBuffer(MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-redobuffer", 
                tooltip: "Number of MBytes on each LDM instance in each Data" +
                         " node allocated for writing REDO logs.",
                constraints: {min: 1, max: 4096, places: 0, pattern: "#"},
                attribute: "RedoBuffer",
                suffix: "M",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            RedoOverCommitCounter: {
                label: "RedoOverCommitCounter",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-redoovercommitcounter", 
                tooltip: "If exceeded when writing redo log to disk, any " +
                         "transactions that weren't committed are aborted " +
                         "and APInode where any of transactions originated " +
                         "handles transactions according to its " +
                         "DefaultOperationRedoProblemAction. 0-disabled.",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "RedoOverCommitCounter",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 3,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            RedoOverCommitLimit: {
                label: "RedoOverCommitLimit(s)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-redoovercommitlimit", 
                tooltip: "Upper limit for trying to write redo log to disk " +
                         "before timing out. 0-disabled.",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "RedoOverCommitLimit",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 20,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
/**************************** Realtime and timeouts ***************************/
            RealTimeoutsHeading: {
                label: "<br><b>Realtime behavior, timeouts</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            TwoPassInitialNodeRestartCopy: {
                label: "TwoPassInitNRCopy",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-twopassinitialnoderestartcopy", 
                tooltip: "Enables multi-thd building of ordered indexes, " +
                         "two-pass copy of data during init. node restarts." +
                         "Requires BuildIndexThreads > 0.",
                attribute: "TwoPassInitialNodeRestartCopy",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: false,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            BuildIndexThreads: {
                label: "BuildIndexThreads",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-" +
                        "buildindexthreads", 
                tooltip: "# of threads to create when rebuilding ordered " +
                         "indexes during a system or node start or when " +
                         "running ndb_restore --rebuild-indexes. Works only " +
                         "with more than 1 fragment for table per data node.",
                constraints: {min: 0, max: 128, places: 0, pattern: "#"},
                attribute: "BuildIndexThreads",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            Numa: {
                label: "Numa",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-" +
                        "numa", 
                tooltip: "Only on Linux systems with libnuma.so." +
                         "0-Data node process doesn't set policy for memory " +
                         "alloc. " +
                         "1-Data node process uses libnuma to request " +
                         "interleaved memory allocation (default)",
                constraints: {min: 0, max: 1, places: 0, pattern: "#"},
                attribute: "Numa",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            MaxNoOfExecutionThreads: {
                label: "MaxNoOfExecutionThreads",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbmtd-" +
                        "maxnoofexecutionthreads", 
                tooltip: "For ndbmtd, specify max no of execution threads",
                constraints: {min: 2, max: 72, places: 0, pattern: "#"},
                attribute: "MaxNoOfExecutionThreads",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            NoOfFragmentLogParts: {
                label: "NoOfFragmentLogParts",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbmtd-" +
                        "nooffragmentlogparts", 
                tooltip: "For ndbmtd, specify max no of execution threads",
                constraints: "4,6,8,10,12,16,20,24,32",
                attribute: "NoOfFragmentLogParts",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.FilteringSelect,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            ThreadConfig: {
                label: "ThreadConfig",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbmtd-" +
                        "threadconfig", 
                tooltip: "Thread configuration",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "ThreadConfig",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true,
                advancedLevel: true
            },
            LockExecuteThreadToCPU: {
                label: "LockExecThreadToCPU",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-" +
                        "lockexecutethreadtocpu", 
                tooltip: "ndbd (string): CPU ID assigned to execution thd." +
                         "ndbmtd (comma-sep list): CPU IDs assigned to " +
                         "execution threads[0,65535].",
                constraints: {min: 0, max: 64, places: 0, pattern: "#"},
                attribute: "LockExecuteThreadToCPU",
                destination: "config.ini",
                suffix: "K",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true,
                advancedLevel: true
            },
            LockMaintThreadsToCPU: {
                label: "LockMaintThreadsToCPU",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-" +
                        "lockmaintthreadstocpu", 
                tooltip: "Comma-sep list of CPU IDs assigned to " +
                         "maintenance threads[0,65535].",
                constraints: {min: 0, max: 64, places: 0, pattern: "#"},
                attribute: "LockMaintThreadsToCPU",
                destination: "config.ini",
                suffix: "K",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true,
                advancedLevel: true
            },
            RealtimeScheduler: {
                label: "RealtimeScheduler",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-realtimescheduler", 
                tooltip: "Dis/enables realtime scheduling of data node threads",
                attribute: "RealtimeScheduler",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: false,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true,
                advancedLevel: true
            },
            SchedulerResponsiveness: {
                label: "SchedulerResponsiveness",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-" +
                        "schedulerresponsiveness", 
                tooltip: "Balance in scheduler between speed and throughput." +
                         "HIGH- favor response time, LOW- favor throughput.",
                constraints: {min: 0, max: 10, places: 0, pattern: "#"},
                attribute: "SchedulerResponsiveness",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 5,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true,
                advancedLevel: true
            },
            SchedulerSpinTimer: {
                label: "SchedulerSpinTimer",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-" +
                        "schedulerspintimer", 
                tooltip: "Time in microsec for threads to be executed in " +
                         "scheduler before sleeping",
                constraints: {min: 0, max: 500, places: 0, pattern: "#"},
                attribute: "SchedulerSpinTimer",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true,
                advancedLevel: true
            },
            TimeBetweenLocalCheckpoints: {
                label: "TimeBtwLocalCP(log(2))",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-timebetweenlocalcheckpoints", 
                tooltip: "Value of 6 or less means local checkpoints will " +
                         "be executed continuously,independent of workload",
                constraints: {min: 0, max: 31, places: 0, pattern: "#"},
                attribute: "TimeBetweenLocalCheckpoints",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 20,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            TimeBetweenGlobalCheckpoints: {
                label: "TimeBetweenGCPs",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-timebetweenglobalcheckpoints", 
                tooltip: "As part of commit process,transactions are placed " +
                         "in global CP group. This group's log records are " +
                         "flushed to disk each TimeBetweenGlobalCP ms",
                constraints: {min: 20, max: 32000, places: 0, pattern: "#"},
                attribute: "TimeBetweenGlobalCheckpoints",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 2000,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            TimeBetweenGlobalCheckpointsTimeout: {
                label: "TimeBtwGCPTimeout",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-timebetweenglobalcheckpointstimeout", 
                tooltip: "Minimum timeout between global checkpoints",
                constraints: {min: 10, max: 4294967039, places: 0, pattern: "#"},
                attribute: "TimeBetweenGlobalCheckpointsTimeout",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 120000,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            TimeBetweenEpochs: {
                label: "TimeBetweenEpochs",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-timebetweenepochs", 
                tooltip: "Interval between synchronization epochs for NDB " +
                         "Cluster Replication",
                constraints: {min: 0, max: 32000, places: 0, pattern: "#"},
                attribute: "TimeBetweenEpochs",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 100,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            TimeBetweenEpochsTimeout: {
                label: "TimeBetweenEpochsTimeout",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-timebetweenepochstimeout", 
                tooltip: "Timeout for time between epochs. " +
                        "Exceeding will cause node shutdown.",
                constraints: {min: 0, max: 256000, places: 0, pattern: "#"},
                attribute: "TimeBetweenEpochsTimeout",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 32000,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            TimeBetweenWatchdogCheck: {
                label: "TimeBtwWatchDogCheck",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-timebetweenwatchdogcheck", 
                tooltip: "Time between watch-dog thread checks on " +
                        "main thread",
                constraints: {min: 70, max: 4294967039, places: 0, pattern: "#"},
                attribute: "TimeBetweenWatchdogCheck",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 6000,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            TimeBetweenWatchdogCheckInitial: {
                label: "TimeBtwWatchDogChkInit",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-timebetweenwatchdogcheckinitial", 
                tooltip: "Time between execution checks inside a database " +
                        "node in the early start phases when memory is " +
                        "allocated",
                constraints: {min: 70, max: 4294967039, places: 0, pattern: "#"},
                attribute: "TimeBetweenWatchdogCheckInitial",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 60000,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            TransactionInactiveTimeout: {
                label: "TransactInactiveTimeout",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-transactioninactivetimeout", 
                tooltip: "MAX time permitted to lapse between operations in " +
                         "the same transaction before the transaction is" +
                         "aborted. Timeout of 0 means we don't timeout at all.",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "TransactionInactiveTimeout",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 4294967039,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            TransactionDeadlockDetectionTimeout: {
                label: "TransactDeadlckDetectTOut",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-transactiondeadlockdetectiontimeout", 
                tooltip: "How long TC waits for query exec. by another node " +
                         "before aborting the transaction, and is important " +
                         "for both node failure handling and deadlock detect",
                constraints: {min: 50, max: 4294967039, places: 0, pattern: "#"},
                attribute: "TransactionDeadlockDetectionTimeout",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 1200,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            TimeBetweenInactiveTransactionAbortCheck: {
                label: "TimeBtwInactTransAbortChk",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-timebetweeninactivetransactionabortcheck", 
                tooltip: "Specifies interval (in ms) when each transaction " +
                         "is checked for timing out",
                constraints: {min: 1000, max: 4294967039, places: 0, pattern: "#"},
                attribute: "TimeBetweenInactiveTransactionAbortCheck",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 1000,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            HeartbeatIntervalDbDb: {
                label: "HeartbeatIntervalDbDb",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-heartbeatintervaldbdb", 
                tooltip: "Time between ndbd(DB)-ndbd(DB) heartbeats. " +
                        "ndbd(DB) considered dead after 3 missed HBs",
                constraints: {min: 10, max: 4294967039, places: 0, pattern: "#"},
                attribute: "HeartbeatIntervalDbDb",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            HeartbeatIntervalDbApi: {
                label: "HeartbeatIntervalDbApi",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-heartbeatintervaldbapi", 
                tooltip: "Time between mysqld(API)-ndbd(DB) heartbeats. " +
                        "mysqld(API) connection closed after 3 missed HBs",
                constraints: {min: 100, max: 4294967039, places: 0, pattern: "#"},
                attribute: "HeartbeatIntervalDbApi",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            ConnectCheckIntervalDelay: {
                label: "ConnChkIntervalDelay",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-connectcheckintervaldelay", 
                tooltip: "Enables connection checking between data nodes " +
                         "after one of them fails HB checks for 5 times " +
                         "up to HeartbeatIntervalDbDb ms",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "ConnectCheckIntervalDelay",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0, //disabled
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            StartPartialTimeout: {
                label: "StartPartialTimeout",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-startpartialtimeout", 
                tooltip: "How long Cluster waits for all data nodes to come " +
                         "up before the cluster initial. routine is invoked",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "StartPartialTimeout",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 30000,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            StartPartitionedTimeout: {
                label: "StartPartitionedTOut",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-startpartitionedtimeout", 
                tooltip: "If after waiting StartPartialTimeout ms Cluster " +
                         "is still possibly in a partitioned state it will " +
                         "continue waiting for StartPartitionedTimeout ms",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "StartPartitionedTimeout",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 4294967039,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            StartFailureTimeout: {
                label: "StartFailureTimeout",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-startfailuretimeout", 
                tooltip: "Time to wait for data node to start up or fail; " +
                         "0 means no data node startup timeout is applied",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "StartFailureTimeout",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            ArbitrationTimeout: {
                label: "ArbitrationTimeout",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-arbitrationtimeout", 
                tooltip: "How long data nodes wait for a response from the " +
                         "arbitrator to an arbitration message. If this is " +
                         "exceeded, the network is assumed to have split.",
                constraints: {min: 10, max: 4294967039, places: 0, pattern: "#"},
                attribute: "ArbitrationTimeout",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 7500,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            RestartSubscriberConnectTimeout: {
                label: "RestartSubscriberConnTOut",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#" +
                    "ndbparam-ndbd-restartsubscriberconnecttimeout", 
                tooltip: "How long data nodes wait for subscribing API nodes" +
                         "to connect. Once expired, any “missing” API nodes " +
                         "are disconnected from the cluster. 0 - disabled.",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "RestartSubscriberConnectTimeout",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 12000,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
/**************************** Error behaviour ***************************/
            ErrorHandlingHeading: {
                label: "<br><b>Error handling</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            StopOnError: {
                label: "StopOnError",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-stoponerror", 
                tooltip: "If set to 0, ndbd(DB) automatically " +
                        "restarts/recovers in case of node failure",
                attribute: "StopOnError",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                constraints: {min: 0, max: 1, places: 0, pattern: "#"},
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            CrashOnCorruptedTuple: {
                label: "CrashOnCorruptedTuple",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-crashoncorruptedtuple", 
                tooltip: "Default (true) forces data node to shut down " +
                         "whenever it encounters a corrupted tuple",
                attribute: "CrashOnCorruptedTuple",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: true,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            RestartOnErrorInsert: {
                label: "RestartOnErrorInsert",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-restartonerrorinsert", 
                tooltip: "Insert errors in execution of individual blocks " + 
                         "of code as part of testing; DEBUG build only",
                constraints: {min: 0, max: 4, places: 0, pattern: "#"},
                attribute: "RestartOnErrorInsert",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            StartFailRetryDelay: {
                label: "StartFailRetryDelay(s)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-startfailretrydelay", 
                tooltip: "Seconds between restart attempts of data node in " +
                         "case of failure on startup. Req. StopOnError=0",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "StartFailRetryDelay",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 0,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxStartFailRetries: {
                label: "MaxStartFailRetries",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-maxstartfailretries", 
                tooltip: "Limit of restart attempts by the data node in case" +
                         "of failed startup. Req. StopOnError=0",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "MaxStartFailRetries",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 3,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            }
/**************************** Statistics ***************************/
/*
            StatisticsHeading: {
                label: "<br><b>Index statistics</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            }
*/
        }
    },
    "sql": {
        parameters: {
/*********************************** General **********************************/
            NodeIdHeading: {
                label: "<br><b>Node identity and directories</b>",
                heading: true,
                visibleType: true,
                visibleInstance: true,
                advancedLevel: false
            },
            NodeId: {
                label: "NodeId",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-api-definition.html#ndbparam-api-nodeid", 
                tooltip: "Number identifying application node (mysqld(API))",
                constraints: {min: 1, max: 255, places: 0, pattern: "#"},
                attribute: "NodeId",
                destination: "config.ini",
                overridableType: false,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true
            },
            HostName: {
                label: "HostName",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-api-definition.html#ndbparam-api-hostname", 
                tooltip: "Name of computer for this node",
                attribute: "HostName",
                destination: "config.ini",
                overridableType: false,
                overridableInstance: false,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true
            },
            DataDir: {
                label: "DataDir",
                docurl: mcc.util.getDocUrlRoot() + "" +
                    "server-options.html#option_mysqld_datadir",
                tooltip: "Data directory for this node",
                attribute: "DataDir",
                destination: "my.cnf",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true
            },
/******************************* NDB/MYSQLDparams ********************************/
            NDBMYSQLDHeading: {
                label: "<br><b>Various NDB parameters for MYSQLD</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: false
            },
            /*
            https://dev.mysql.com/doc/refman/5.7/en/mysql-cluster-options-variables.html#option_mysqld_ndb-cluster-connection-pool
            Currently not possible to implement ndb-cluster-connection-pool since it requires changing the calculations part as well as modifying multiple config files. Same goes for ndb-cluster-connection-pool-nodeids.

            https://dev.mysql.com/doc/refman/5.7/en/mysql-cluster-options-variables.html#option_mysqld_ndb-allow-copying-alter-table
            ndb-allow-copying-alter-table - Not currently, requires, as do many options, drop-down list of choices (ON|OFF in this case).
            
            Same goes for ndb-default-column-format (FIXED|DYNAMIC, global) and ndb-distribution ([KEYHASH|LINHASH], global).
            */
            ndbwaitsetup: {
                label: "ndb-wait-setup",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-options-variables.html#option_mysqld_ndb-wait-setup",
                tooltip: "Size (in bytes) to use for NDB transaction batches",
                constraints: {min: 30, max: 31536000, places: 0, pattern: "#"},
                attribute: "ndb-wait-setup",
                destination: "my.cnf",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 120,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true
            },
            ndbbatchsize: {
                label: "ndb-batch-size",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-options-variables.html#option_mysqld_ndb-batch-size",
                tooltip: "Size (in bytes) to use for NDB transaction batches",
                constraints: {min: 0, max: 31536000, places: 0, pattern: "#"},
                attribute: "ndb-batch-size",
                destination: "my.cnf",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 32768,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true
            },
            ndbblobreadbatchbytes: {
                label: "ndb-blob-read-batch-bytes",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-options-variables.html#option_mysqld_ndb-blob-read-batch-bytes",
                tooltip: "Size for batching of BLOB data reads in Cluster " +
                         "apps. 0 disables BLOB read batching.",
                constraints: {min: 0, max: 4294967295, places: 0, pattern: "#"},
                attribute: "ndb-blob-read-batch-bytes",
                destination: "my.cnf",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 65536,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            ndbblobwritebatchbytes: {
                label: "ndb-blob-write-batch-bytes",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-options-variables.html#option_mysqld_ndb-blob-write-batch-bytes",
                tooltip: "Size for batching of BLOB data writes in Cluster " +
                         "apps. 0 disables BLOB write batching.",
                constraints: {min: 0, max: 4294967295, places: 0, pattern: "#"},
                attribute: "ndb-blob-write-batch-bytes",
                destination: "my.cnf",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 65536,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            /*ndb-connectstring is automatically added by deploy.js.*/
            ndbdeferredconstraints: { //testing only
                label: "ndb-deferred-constraints",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-options-variables.html#option_mysqld_ndb-deferred-constraints", 
                tooltip: "Defer constraint checks on unique indexes until " +
                         "commit time. For testing purposes only.",
                attribute: "ndb-deferred-constraints",
                destination: "my.cnf",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: false,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            ndblogapplystatus: {
                label: "ndb-log-apply-status",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-options-variables.html#option_mysqld_ndb-log-apply-status", 
                tooltip: "mysqld act as slave to log mysql.ndb_apply_status " +
                         "updates received from its immediate master in its " +
                         "own binary log, using its own server ID",
                attribute: "ndb-log-apply-status",
                destination: "my.cnf",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: false,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            ndblogemptyepochs: {
                label: "ndb-log-empty-epochs",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-options-variables.html#option_mysqld_ndb-log-empty-epochs", 
                tooltip: "Causes epochs in which there were no changes to be " +
                         "written to the ndb_apply_status and " +
                         "ndb_binlog_index tables",
                attribute: "ndb-log-empty-epochs",
                destination: "my.cnf",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: false,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            ndblogemptyupdate: {
                label: "ndb-log-empty-update",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-options-variables.html#option_mysqld_ndb-log-empty-update", 
                tooltip: "Causes updates in which there were no changes to " +
                         "be written to the ndb_apply_status and " +
                         "ndb_binlog_index tables",
                attribute: "ndb-log-empty-update",
                destination: "my.cnf",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: false,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            ndblogexclusivereads: {
                label: "ndb-log-exclusive-reads",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-options-variables.html#option_mysqld_ndb-log-exclusive-reads", 
                tooltip: "Log primary key reads with exclusive locks; " +
                         "allow conflict resol. based on read conflicts",
                attribute: "ndb-log-exclusive-reads",
                destination: "my.cnf",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: false,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
/******************************* MYSQLDparams ********************************/
            MYSQLDHeading: {
                label: "<br><b>Various MYSQLD parameters</b>",
                heading: true,
                visibleType: true,
                visibleInstance: true,
                advancedLevel: false
            },
            ArbitrationRank: {
                label: "ArbitrationRank",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-api-definition.html",
                attribute: "ArbitrationRank",
                tooltip: "0: The node is never used as arbitrator (DEFAULT) " +
                         "1: The node is high priority arbitrator " +
                         "2: Low-priority arbitrator node",
                constraints: {min: 0, max: 2, places: 0, pattern: "#"},
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true
            },
            /*ArbitrationDelay: {} SKIPPED*/
            AutoReconnect: {
                label: "AutoReconnect",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-api-definition.html#ndbparam-api-autoreconnect", 
                tooltip: "Forces disconnected APInodes to use new connection" +
                         "rather than attempting to re-use existing one",
                attribute: "AutoReconnect",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: false,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false
            },
            BatchByteSize: {
                label: "BatchByteSize (kB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-api-definition.html#ndbparam-api-batchbytesize", 
                tooltip: "Batch size (in kB) for fetching records",
                constraints: {min: 1, max: 1024, places: 0, pattern: "#"},
                attribute: "BatchByteSize",
                destination: "config.ini",
                suffix: "K",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 16,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            BatchSize: {
                label: "BatchSize(records)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-api-definition.html#ndbparam-api-batchsize", 
                tooltip: "Batch size (in # of records) for fetching",
                constraints: {min: 1, max: 992, places: 0, pattern: "#"},
                attribute: "BatchSize",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 256,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true,
                advancedLevel: true
            },
            MaxScanBatchSize: {
                label: "MaxScanBatchSize(kB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-api-definition.html#ndbparam-api-maxscanbatchsize", 
                tooltip: "Batch size of each batch sent from each data node." +
                         "Parameter limits total batch size over all nodes.",
                constraints: {min: 32, max: 16384, places: 0, pattern: "#"},
                attribute: "MaxScanBatchSize",
                destination: "config.ini",
                suffix: "K",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 256,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            TotalSendBufferMemory: {
                label: "TotalSendBufferMemory(kB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-api-definition.html#ndbparam-api-totalsendbuffermemory",
                attribute: "TotalSendBufferMemory",
                tooltip: "Total amount of memory to allocate on this node " +
                         "for shared send buffer memory. 0-disabled.",
                constraints: {min: 256, max: 4194303, places: 0, pattern: "#"},
                suffix: "K",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true,
                advancedLevel: true
            },
            SendBufferMemory: {
                label: "SendBufferMemory(KB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-tcp-definition.html#ndbparam-tcp-sendbuffermemory", 
                tooltip: "MBytes of buffer for signals sent from this node",
                constraints: {min: 256, max: 4194303, places: 0, pattern: "#"},
                attribute: "SendBufferMemory",
                suffix: "K",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 2048,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true,
                advancedLevel: true
            },
            ExtraSendBufferMemory: {
                label: "ExtraSendBufferMemory",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-api-definition.html#ndbparam-api-extrasendbuffermemory", 
                tooltip: "Amount of transporter send buffer memory to " +
                         "allocate on top of any that has been set using " +
                         "TotalSendBufferMemory, SendBufferMemory or both",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "ExtraSendBufferMemory",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: false,
                advancedLevel: true
            },
            /*HeartbeatThreadPriority: {Skipped, seems too fine-grained for MCC}*/
            /*DefaultOperationRedoProblemAction: {
              https://dev.mysql.com/doc/refman/5.7/en/mysql-cluster-api-definition.html#ndbparam-api-defaultoperationredoproblemaction}
             Skipped, need to implement list with choices first...*/
            /*DefaultHashMapSize: (https://dev.mysql.com/doc/refman/5.7/en/mysql-cluster-api-definition.html#ndbparam-api-defaulthashmapsize)
             Skipped, need to implement list with choices first...*/
            /*WAN: { //we will add another parameter that'll make this parameter deprecated
                label: "WAN",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-api-definition.html#ndbparam-api-wan", 
                tooltip: "Use WAN TCP setting as default",
                attribute: "WAN",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.CheckBox,
                width: "15px",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },*/
            ConnectBackoffMaxTime: {
                label: "ConnectBackoffMaxTime",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-api-definition.html#ndbparam-api-ConnectBackoffMaxTime", 
                tooltip: "In an NDB Cluster with many unstarted data nodes, " +
                         "the value of this parameter can be raised to " +
                         "circumvent connection attempts to data nodes which " +
                         "have not yet begun to function in the cluster. API " +
                         "node is not connected to any new data nodes.",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "ConnectBackoffMaxTime",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            StartConnectBackoffMaxTime: {
                label: "StartConnBackoffMaxTime",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-api-definition.html#ndbparam-api-startconnectbackoffmaxtime", 
                tooltip: "In an NDB Cluster with many unstarted data nodes, " +
                         "the value of this parameter can be raised to " +
                         "circumvent connection attempts to data nodes which " +
                         "have not yet begun to function in the cluster. API " +
                         "node is connected to any new data nodes.",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "StartConnectBackoffMaxTime",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
/******************************* Communication ********************************/
            CommunicationHeading: {
                label: "<br><b>Communication</b>",
                heading: true,
                visibleType: true,
                visibleInstance: true,
                advancedLevel: false
            },
            Portbase: {
                label: "Portbase",
                attribute: "Portbase",
                tooltip: "Portbase is not a MySQL Cluster configuration " +
                        "parameter, but is used for convenience to allow " +
                        "changing the base for allocating port numbers for " +
                        "individual mysqld processes",
                constraints: {min: 0, max: 65535, places: 0, pattern: "#"},
                destination: "mcc",
                overridableType: true,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 3306,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false
            },
            Port: {
                label: "Port",
                docurl: mcc.util.getDocUrlRoot() + "" +
                    "server-options.html#option_mysqld_port",
                tooltip: "The port number to use when listening for TCP/IP " +
                        "connections",
                constraints: {min: 0, max: 65535, places: 0, pattern: "#"},
                attribute: "Port",
                destination: "my.cnf",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true
            },
            Socket: {
                label: "Socket",
                docurl: mcc.util.getDocUrlRoot() + "" +
                    "server-options.html#option_mysqld_socket",
                tooltip: "On Unix, this option specifies the Unix socket " +
                        "file to use when listening for local connections. " +
                        "On Windows, the option specifies the pipe name to " +
                        "use when listening for local connections that use " +
                        "a named pipe",
                attribute: "Socket",
                destination: "my.cnf",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: true
            }
        }
    },
    "api": {
        parameters: {
/*********************************** General **********************************/
            NodeIdHeading: {
                label: "<br><b>Node identity and directories</b>",
                heading: true,
                visibleType: false,
                visibleInstance: true,
                advancedLevel: false
            },
            NodeId: {
                label: "NodeId",
                attribute: "NodeId",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-api-definition.html#ndbparam-api-nodeid", 
                tooltip: "Number identifying application node (mysqld(API))",
                constraints: {min: 1, max: 255, places: 0, pattern: "#"},
                destination: "config.ini",
                overridableType: false,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true
            },
            HostName: {
                label: "HostName",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-api-definition.html#ndbparam-api-hostname", 
                tooltip: "Name of computer for this node",
                attribute: "HostName",
                destination: "config.ini",
                overridableType: false,
                overridableInstance: false,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true
            }/*,
            ConnectionMap: { //To be added later with support for Shared Memory transporters
                label: "ConnectionMap",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-api-definition.html",
                tooltip: "Specifies which data nodes to connect.",
                attribute: "ConnectionMap",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: false,
                visibleInstance: true
            }*/
        }
    }
};

/****************************** Implementation  *******************************/

// Retrieve the array of parameters for the given process family name
function getAllPara(family) {
    return processParameterDefaults[family].parameters;
}

// Retrieve an attribute for the given family, instance and parameter name
function getPara(family, instanceId, parameter, attribute) {
    if (attribute != "defaultValueInstance") {
        return processParameterDefaults[family].
                parameters[parameter][attribute];
    } else if (instanceId && processParameterDefaults[family].
                parameters[parameter][attribute]) {
        return processParameterDefaults[family].
                parameters[parameter][attribute][instanceId];
    }
}

// Assign to an attribute for the given family, instance and parameter name
function setPara(family, instanceId, parameter, attribute, value) {
    if (attribute != "defaultValueInstance") {
        processParameterDefaults[family].parameters[parameter][attribute] = 
                value;
    } else if (instanceId && processParameterDefaults[family].
                parameters[parameter][attribute]) {
        processParameterDefaults[family].
                parameters[parameter][attribute][instanceId] = value;
    }
}

// Check if the attribute is visible at the chosen configuration level
function visiblePara(dest, appArea, family, param) {
    if (dest == "cfg") {
        return true;
    } else if (dest == "gui") {
        var advanced = getPara(family, null, param, "advancedLevel");
        return !(advanced && mcc.util.getCookie("configLevel") == "simple");
    }
}

// Check if the attribute is a heading
function isHeading(family, parameter) {
    return (getPara(family, null, parameter, "heading") == true);
}

// Reset all predefined instance values for the given id or for all ids
function resetDefaultValueInstance(id) {
    for (var f in processParameterDefaults) {
        for (var p in processParameterDefaults[f].parameters) {
            var para = processParameterDefaults[f].parameters[p];
            if (!para.defaultValueInstance) continue;
            if (id !== undefined && 
                    para.defaultValueInstance[id] !== undefined) {
                mcc.util.dbg("Reset " + p + " for item id " + id);
                para.defaultValueInstance[id] = undefined;
            } else {
                for (var i in para.defaultValueInstance) {
                    if (para.defaultValueInstance[i] !== undefined) {
                        mcc.util.dbg("Reset " + p + " for item id " + i);
                        para.defaultValueInstance[i] = undefined;
                    }
                }
            }
        }
    }
}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    mcc.util.dbg("Configuration parameter definition module initialized");
});
