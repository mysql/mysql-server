# Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

MYSQL_ADD_PLUGIN(connection_control
		 connection_control_coordinator.cc
		 connection_control.cc
		 security_context_wrapper.cc
		 connection_delay.cc
                 MODULE_ONLY)

