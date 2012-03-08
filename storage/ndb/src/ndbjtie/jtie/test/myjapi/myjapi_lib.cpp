/*
 Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * myjapi_lib.cpp
 */

// libraries
#include "helpers.hpp"

// global jtie library definitions
#include "jtie.hpp"
#include "jtie_lib.hpp"

// global myjapi library definitions
#include "myjapi_MyJapiCtypes.hpp"
#include "myjapi_MyJapi.hpp"
#include "myjapi_classes.hpp"

// ---------------------------------------------------------------------------
// API Global Symbol Definitions & Template Instantiations
// ---------------------------------------------------------------------------

// static class definitions and template instantiations for classes
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(myjapi_A, "myjapi/A")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(myjapi_B0, "myjapi/B0")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(myjapi_B1, "myjapi/B1")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(myjapi_CI_C0, "myjapi/CI$C0")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(myjapi_CI_C1, "myjapi/CI$C1")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(myjapi_CI_C0Array, "myjapi/CI$C0Array")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(myjapi_CI_C1Array, "myjapi/CI$C1Array")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(myjapi_D0, "myjapi/D0")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(myjapi_D1, "myjapi/D1")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(myjapi_D2, "myjapi/D2")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(myjapi_E, "myjapi/E")

// template instantiations for enums
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(E::EE)

// ---------------------------------------------------------------------------
// Library Load and Unload Handlers
// ---------------------------------------------------------------------------

// Initializes the JTie resources; called when the native library is loaded;
// returns the JNI version needed by the native library or JNI_ERR.
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM * jvm, void * reserved)
{
    TRACE("jint JNI_OnLoad(JavaVM *, void *)");
    VERBOSE("loading the MyJAPI JTie library ...");

    const jint required_jni_version = JTie_OnLoad(jvm, reserved);
    if (required_jni_version == JNI_ERR) {
        PRINT_ERROR("JTie_OnLoad() returned: JNI_ERR");
        return JNI_ERR;
    }

    VERBOSE("initializing the myapi resources ...");
    myapi_init();
    VERBOSE("... initialized the myapi resources");

    VERBOSE("... loaded the MyJAPI JTie library");
    return required_jni_version;
}

// Called when the class loader containing the native library is garbage
// collected; called in an unknown context (such as from a finalizer):
// be conservative, and refrain from arbitrary Java call-backs.
JNIEXPORT void JNICALL
JNI_OnUnload(JavaVM * jvm, void * reserved)
{
    TRACE("void JNI_OnUnload(JavaVM *, void *)");
    VERBOSE("unloading the MyJAPI JTie library...");

    VERBOSE("releasing the myapi resources ...");
    myapi_finit();
    VERBOSE("... released the myapi resources");

    JTie_OnUnload(jvm, reserved);

    VERBOSE("... unloaded the MyJAPI JTie library");
}

// ---------------------------------------------------------------------------
