/* 
  manifest.js - Writes a custom XML manifest for each executable/library
  5 command line options must be supplied: 
  name      - Name of the executable/library into which the mainfest will be 
              embedded.
  version   - Version of the executable 
  arch      - Architecture intended.
  exe_level - Application execution level. 
              [asInvoker|highestAvailable|requireAdministrator]
  outfile   - Final destination where mainfest will be written.

  Example:  
  cscript manifest.js name=mysql version=5.0.32 arch=X86 type=win32 
                      exe_level=asInvoker outfile=out.xml
*/

try 
{
  var args = WScript.Arguments
  for (i=0; i < args.Count(); i++)
  {
    var parts = args.Item(i).split('=');
    switch (parts[0])
    {
      case "name":
           var app_name= parts[1];
           break;
      case "version":
           var supp_version= parts[1];
           // Clean up the supplied version string.
           var end= supp_version.indexOf("-");
           if (end == -1) end= supp_version.length;
           var app_version= supp_version.substring(0, end);
           var fourth_element= 0;
           if(app_version.match(/[a-z]$/)) {
               fourth_element+= (1 + app_version.charCodeAt(end-1) - "a".charCodeAt(0));
               app_version= app_version.substring(0,--end);
           }
           if(app_version.match(/sp[1-9]$/)) {
               fourth_element+= 100*(app_version.charCodeAt(end-1) - "0".charCodeAt(0));
               app_version= app_version.substring(0, end-3);
               end-= 3;
           }
           app_version+= "." + fourth_element;
           break;
      case "arch":
           var app_arch= parts[1];
           break;
      case "exe_level":
		   var app_exe_level= parts[1];
           break;
      case "outfile":
		   var manifest_file= parts[1];
           break;
	  default:
	       WScript.echo("Invalid argument supplied.");
    }
  }
  if (i != 5)
    throw new Error(1, "Incorrect number of arguments.");

  var manifest_xml= "<?xml version=\'1.0\' encoding=\'UTF-8\' standalone=\'yes\'?>\r\n";
  manifest_xml+= "<assembly xmlns=\'urn:schemas-microsoft-com:asm.v1\'";
  manifest_xml+= " manifestVersion=\'1.0\'>\r\n";
  // Application Information 
  manifest_xml+= "\t<assemblyIdentity name=\'" + app_name + "\'";
  manifest_xml+= " version=\'" + app_version + "\'"; 
  manifest_xml+= " processorArchitecture=\'" + app_arch + "\'";
  manifest_xml+= " publicKeyToken=\'02ad33b422233ae3\'";
  manifest_xml+= " type=\'win32\' />\r\n";
  // Identify the application security requirements.
  manifest_xml+= "\t<trustInfo xmlns=\'urn:schemas-microsoft-com:asm.v2\'>\r\n"; 
  manifest_xml+= "\t\t<security>\r\n\t\t\t<requestedPrivileges>\r\n\t\t\t\t";
  manifest_xml+= "<requestedExecutionLevel level=\'" + app_exe_level + "\'";
  manifest_xml+= " uiAccess=\'false\'/>\r\n";
  manifest_xml+= "\t\t\t</requestedPrivileges>\r\n\t\t</security>\r\n";
  manifest_xml+= "\t</trustInfo>\r\n</assembly>\r\n";

  // Write the valid XML to it's final destination.
  var outfileXML = WScript.CreateObject("Msxml2.DOMDocument.3.0");
  outfileXML.async = false;
  if (!outfileXML.loadXML(manifest_xml))
  {
     WScript.Echo(manifest_xml);
     throw new Error(2, "Invalid XML");
  }
  outfileXML.save(manifest_file);
  
  WScript.Echo("Success, created custom manifest!");
  WScript.Quit(0);
}
catch (e)
{
    WScript.Echo("Error: " + e.description);
	WScript.Quit(1);
}
