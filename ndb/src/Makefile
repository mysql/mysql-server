include .defs.mk

DIRS := \
        client \
        common  \
        kernel  \
        ndbapi  \
	mgmsrv  \
        mgmapi  \
	rep \
        mgmclient \
        cw \
        ndbbaseclient
ifneq ($(NDB_ODBC),N)
        DIRS += ndbclient
endif
ifeq ($(findstring OSE, $(NDB_OS)),)
	DIRS += scripts 
endif
include $(NDB_TOP)/Epilogue.mk

_bins_mgmsrv: _libs_ndbapi
_bins_mgmsrv: _libs_mgmapi
_bins_mgmclient: _libs_mgmapi
_bins_mgmclient: _libs_common
_bins_client: _bins_ndbapi
_bins_common: _bins_mgmapi
_bins_kernel: _bins_ndbapi
_bins_newtonapi: _bins_ndbapi
_bins_mgmapi : _libs_common
_bins_rep: _libs_common
_bins_rep: _libs_ndbapi
