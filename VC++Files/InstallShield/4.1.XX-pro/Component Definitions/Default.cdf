[Development]
required0=Servers
SELECTED=Yes
FILENEED=STANDARD
required1=Grant Tables
HTTPLOCATION=
STATUS=Examples, Libraries, Includes and Script files
UNINSTALLABLE=Yes
TARGET=<TARGETDIR>
FTPLOCATION=
VISIBLE=Yes
DESCRIPTION=Examples, Libraries, Includes and Script files
DISPLAYTEXT=Examples, Libraries, Includes and Script files
IMAGE=
DEFSELECTION=Yes
filegroup0=Development
COMMENT=
INCLUDEINBUILD=Yes
INSTALLATION=ALWAYSOVERWRITE
COMPRESSIFSEPARATE=No
MISC=
ENCRYPT=No
DISK=ANYDISK
TARGETDIRCDROM=
PASSWORD=
TARGETHIDDEN=General Application Destination

[Grant Tables]
required0=Servers
SELECTED=Yes
FILENEED=CRITICAL
HTTPLOCATION=
STATUS=The Grant Tables and Core Files
UNINSTALLABLE=Yes
TARGET=<TARGETDIR>
FTPLOCATION=
VISIBLE=Yes
DESCRIPTION=The Grant Tables and Core Files
DISPLAYTEXT=The Grant Tables and Core Files
IMAGE=
DEFSELECTION=Yes
filegroup0=Grant Tables
requiredby0=Development
COMMENT=
INCLUDEINBUILD=Yes
requiredby1=Clients and Tools
INSTALLATION=NEVEROVERWRITE
requiredby2=Documentation
COMPRESSIFSEPARATE=No
MISC=
ENCRYPT=No
DISK=ANYDISK
TARGETDIRCDROM=
PASSWORD=
TARGETHIDDEN=General Application Destination

[Components]
component0=Development
component1=Grant Tables
component2=Servers
component3=Clients and Tools
component4=Documentation

[TopComponents]
component0=Servers
component1=Clients and Tools
component2=Documentation
component3=Development
component4=Grant Tables

[SetupType]
setuptype0=Compact
setuptype1=Typical
setuptype2=Custom

[Clients and Tools]
required0=Servers
SELECTED=Yes
FILENEED=HIGHLYRECOMMENDED
required1=Grant Tables
HTTPLOCATION=
STATUS=The MySQL clients and Maintenance Tools
UNINSTALLABLE=Yes
TARGET=<TARGETDIR>
FTPLOCATION=
VISIBLE=Yes
DESCRIPTION=The MySQL clients and Maintenance Tools
DISPLAYTEXT=The MySQL clients and Maintenance Tools
IMAGE=
DEFSELECTION=Yes
filegroup0=Clients and Tools
COMMENT=
INCLUDEINBUILD=Yes
INSTALLATION=NEWERDATE
COMPRESSIFSEPARATE=No
MISC=
ENCRYPT=No
DISK=ANYDISK
TARGETDIRCDROM=
PASSWORD=
TARGETHIDDEN=General Application Destination

[Servers]
SELECTED=Yes
FILENEED=CRITICAL
HTTPLOCATION=
STATUS=The MySQL Servers
UNINSTALLABLE=Yes
TARGET=<TARGETDIR>
FTPLOCATION=
VISIBLE=Yes
DESCRIPTION=The MySQL Servers
DISPLAYTEXT=The MySQL Servers
IMAGE=
DEFSELECTION=Yes
filegroup0=Servers
requiredby0=Development
COMMENT=
INCLUDEINBUILD=Yes
requiredby1=Grant Tables
INSTALLATION=ALWAYSOVERWRITE
requiredby2=Clients and Tools
requiredby3=Documentation
COMPRESSIFSEPARATE=No
MISC=
ENCRYPT=No
DISK=ANYDISK
TARGETDIRCDROM=
PASSWORD=
TARGETHIDDEN=General Application Destination

[SetupTypeItem-Compact]
Comment=
item0=Grant Tables
item1=Servers
item2=Clients and Tools
item3=Documentation
Descrip=
DisplayText=

[SetupTypeItem-Custom]
Comment=
item0=Development
item1=Grant Tables
item2=Servers
item3=Clients and Tools
Descrip=
item4=Documentation
DisplayText=

[Info]
Type=CompDef
Version=1.00.000
Name=

[SetupTypeItem-Typical]
Comment=
item0=Development
item1=Grant Tables
item2=Servers
item3=Clients and Tools
Descrip=
item4=Documentation
DisplayText=

[Documentation]
required0=Servers
SELECTED=Yes
FILENEED=HIGHLYRECOMMENDED
required1=Grant Tables
HTTPLOCATION=
STATUS=The MySQL Documentation with different formats
UNINSTALLABLE=Yes
TARGET=<TARGETDIR>
FTPLOCATION=
VISIBLE=Yes
DESCRIPTION=The MySQL Documentation with different formats
DISPLAYTEXT=The MySQL Documentation with different formats
IMAGE=
DEFSELECTION=Yes
filegroup0=Documentation
COMMENT=
INCLUDEINBUILD=Yes
INSTALLATION=ALWAYSOVERWRITE
COMPRESSIFSEPARATE=No
MISC=
ENCRYPT=No
DISK=ANYDISK
TARGETDIRCDROM=
PASSWORD=
TARGETHIDDEN=General Application Destination

