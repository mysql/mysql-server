# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
# 
MYSQL_ADD_PLUGIN(test_framework test_framework.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_framework")

MYSQL_ADD_PLUGIN(test_services test_services.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_services")

MYSQL_ADD_PLUGIN(test_services_thread test_services_threaded.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_services_threaded")

