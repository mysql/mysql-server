
# - MYSQL_EMBED_MANIFEST(target_name required_privs)
# Create a manifest for target_name.  Set the execution level to require_privs
#
# NOTE. PROCESSOR_ARCH must be defined before this MACRO is called.

MACRO(MYSQL_EMBED_MANIFEST _target_name _required_privs)
  ADD_CUSTOM_COMMAND(
    TARGET ${_target_name}
    PRE_LINK
    COMMAND cscript.exe 
    ARGS "${PROJECT_SOURCE_DIR}/win/create_manifest.js" name=$(ProjectName) version=${VERSION} arch=${PROCESSOR_ARCH} exe_level=${_required_privs} outfile=$(IntDir)\\$(TargetFileName).intermediate.manifest
    COMMENT "Generates the contents of the manifest contents.")
  ADD_CUSTOM_COMMAND(
    TARGET ${_target_name}
    POST_BUILD
    COMMAND mt.exe 
    ARGS -nologo -manifest $(IntDir)\\$(TargetFileName).intermediate.manifest -outputresource:$(TargetPath) 
    COMMENT "Embeds the manifest contents.")
ENDMACRO(MYSQL_EMBED_MANIFEST)
