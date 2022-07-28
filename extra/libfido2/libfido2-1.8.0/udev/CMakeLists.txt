# Copyright (c) 2018 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

if(UDEV_RULES_DIR)
	install(FILES 70-u2f.rules DESTINATION ${UDEV_RULES_DIR})
endif()
