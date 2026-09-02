# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# MySQL provides RapidYAML from extra/rapidyaml.
if(NOT TARGET ryml::ryml)
  message(FATAL_ERROR "Missing bundled RapidYAML target ryml::ryml")
endif()

set(ryml_PROVIDER "bundled")
