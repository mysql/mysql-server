/*
Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

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
/*********************************** Backup ***********************************/
            BackupHeading: {
                label: "<br><b>Backup parameters</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            BackupMaxWriteSize: {
                label: "BackupMaxWriteSize (MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-backupmaxwritesize", 
                tooltip: "Max size of filesystem writes made by backup",
                constraints: {min: 1, max: 4096, places: 0, pattern: "#"},
                attribute: "BackupMaxWriteSize",
                destination: "config.ini",
                suffix: "M",
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
            BackupDataBufferSize: {
                label: "BackupDataBufferSz (MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-backupdatabuffersize", 
                tooltip: "Default size of databuffer for a backup",
                constraints: {min: 16, max: 4096, places: 0, pattern: "#"},
                attribute: "BackupDataBufferSize",
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
            BackupLogBufferSize: {
                label: "BackupLogBufferSz (MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-backuplogbuffersize", 
                tooltip: "Default size of logbuffer for a backup",
                constraints: {min: 0, max: 4096, places: 0, pattern: "#"},
                attribute: "BackupLogBufferSize",
                destination: "config.ini",
                suffix: "M",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 4,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            BackupMemory: {
                label: "BackupMemory (MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-backupmemory", 
                tooltip: "Total memory allocated for backups per node",
                constraints: {min: 0, max: 4096, places: 0, pattern: "#"},
                attribute: "BackupMemory",
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
                defaultValueType: 30,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LogLevelStartup: {
                label: "LogLevelStartup",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-loglevelstartup", 
                tooltip: "Node startup info printed on stdout",
                constraints: {min: 0, max: 15, places: 0, pattern: "#"},
                attribute: "LogLevelStartup",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 15,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LogLevelShutdown: {
                label: "LogLevelShutdown",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-loglevelshutdown", 
                tooltip: "Node shutdown info printed on stdout",
                constraints: {min: 0, max: 15, places: 0, pattern: "#"},
                attribute: "LogLevelShutdown",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 15,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            LogLevelCheckpoint: {
                label: "LogLevelCheckpoint",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-loglevelcheckpoint", 
                tooltip: "Local and Global checkpoint info printed on stdout",
                constraints: {min: 0, max: 15, places: 0, pattern: "#"},
                attribute: "LogLevelCheckpoint",
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
            LogLevelNodeRestart: {
                label: "LogLevelNodeRestart",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-loglevelnoderestart", 
                tooltip: "Node restart, node failure info printed on stdout",
                constraints: {min: 0, max: 15, places: 0, pattern: "#"},
                attribute: "LogLevelNodeRestart",
                destination: "config.ini",
                overridableType: true,
                overridableInstance: true,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 15,
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
                label: "DataMemory (MB)",
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
                label: "IndexMemory (MB)",
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
/*********************************** Metadata *********************************/
            MetadataHeading: {
                label: "<br><b>Metadata storage</b>",
                heading: true,
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
                defaultValueType: 4096,
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
                defaultValueType: 3500,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            NoOfReplicas: {
                label: "NoOfReplicas",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-noofreplicas", 
                tooltip: "Number of copies of all data in the database (1-4)",
                constraints: {min: 1, max: 2, places: 0, pattern: "#"},
                attribute: "NoOfReplicas",
                destination: "config.ini",
                overridableType: false,
                overridableInstance: false,
                widget: dijit.form.NumberSpinner,
                width: "50%",
                defaultValueType: 2,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
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
/*********************************** Disk data ********************************/
            DiskDataHeading: {
                label: "<br><b>Data storage</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            DiskPageBufferMemory: {
                label: "DiskPageBufferMem (MB)",
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
                label: "SharedGlobalMemory (MB)",
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
                label: "SendBufferMemory (MB)",
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
                label: "ReceiveBufferMem (MB)",
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
                label: "LongMessageBuffer (MB)",
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
                defaultValueType: 32,
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
                label: "MaxNoOfConcTransactions",
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
                defaultValueType: 16384,
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
                defaultValueType: 512,
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
                label: "FragmentLogFileSz (MB)",
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
                defaultValueType: 175,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            RedoBuffer: {
                label: "RedoBuffer (MB)",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                    "-ndbd-definition.html#ndbparam-ndbd-redobuffer", 
                tooltip: "Number of Mbytes on each ndbd(DB) node allocated " +
                        "for writing REDO logs",
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
/**************************** Realtime and timeouts ***************************/
            RealTimeoutsHeading: {
                label: "<br><b>Realtime behavior, timeouts</b>",
                heading: true,
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            MaxNoOfExecutionThreads: {
                label: "MaxNoOfExecutionThreads",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbmtd-" +
                        "maxnoofexecutionthreads", 
                tooltip: "For ndbmtd, specify max no of execution threads",
                constraints: {min: 2, max: 36, places: 0, pattern: "#"},
                attribute: "MaxNoOfExecutionThreads",
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
                overridableInstance: false,
                widget: dijit.form.TextBox,
                width: "95%",
                defaultValueType: undefined,
                defaultValueInstance: [],
                visibleType: true,
                visibleInstance: false,
                advancedLevel: true
            },
            StopOnError: {
                label: "StopOnError",
                docurl: mcc.util.getDocUrlRoot() + "mysql-cluster" +
                        "-ndbd-definition.html#ndbparam-ndbd-stoponerror", 
                tooltip: "If set to N, ndbd(DB) automatically " +
                        "restarts/recovers in case of node failure",
                attribute: "StopOnError",
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
                tooltip: "If set to yes, then NDB Cluster data will not be " +
                        "swapped out to disk",
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
            TimeBetweenEpochsTimeout: {
                label: "TimeBtweenEpochsTimeout",
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
            TimeBetweenWatchdogCheckInitial: {
                label: "TimeBtwWatchdCheckIntl",
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
                tooltip: "Time application can wait before executing another " +
                    "transaction part (ms). This is the time the transaction " +
                    "coordinator waits for the application to execute or " +
                    "send another part (query, statement) of the " +
                    "transaction. If the application takes too long time, " +
                    "the transaction gets aborted. Timeout set to 0 means " +
                    "that we don't timeout at all on application wait.",
                constraints: {min: 0, max: 4294967039, places: 0, pattern: "#"},
                attribute: "TransactionInactiveTimeout",
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
            }
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
            }
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




