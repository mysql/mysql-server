
# - MYSQL_EMBED_MANIFEST(target_name required_privs)
# Create a manifest for target_name.  Set the execution level to require_privs
#
# NOTE. PROCESSOR_ARCH must be defined before this MACRO is called.

MACRO(MYSQL_EMBED_MANIFEST _target_name _required_privs)
  ADD_CUSTOM_COMMAND(
    TARGET ${_target_name}
    PRE_LINK
    COMMAND cscript.exe 
    ARGS "${PROJECT_SOURCE_DIR}/win/create_manifest.js" name=$(TargetName) version=${VERSION} arch=${PROCESSOR_ARCH} exe_level=${_required_privs} outfile=$(IntDir)\\$(TargetFileName).intermediate.manifest
    COMMENT "Generates the contents of the manifest contents.")
  ADD_CUSTOM_COMMAND(
    TARGET ${_target_name}
    POST_BUILD
    COMMAND mt.exe       ARGS -nologo -hashupdate -makecdfs -manifest $(IntDir)\\$(TargetFileName).intermediate.manifest -outputresource:$(TargetPath) 
    COMMAND makecat.exe  ARGS $(IntDir)\\$(TargetFileName).intermediate.manifest.cdf
    COMMAND signtool.exe ARGS sign /a /t http://timestamp.verisign.com/scripts/timstamp.dll $(TargetPath)
    COMMENT "Embeds the manifest contents, creates a cryptographic catalog, signs the target with Authenticode certificate.")
ENDMACRO(MYSQL_EMBED_MANIFEST)
