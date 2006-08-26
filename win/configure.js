// Configure.js

ForReading = 1;
ForWriting = 2;
ForAppending = 8;

try 
{
    var fso = new ActiveXObject("Scripting.FileSystemObject");

    var args = WScript.Arguments
    
    // read in the Unix configure.in file
    var configureInTS = fso.OpenTextFile("configure.in", ForReading);
    var configureIn = configureInTS.ReadAll();
    configureInTS.Close();
    var default_comment = "Source distribution";
    var default_port = GetValue(configureIn, "MYSQL_TCP_PORT_DEFAULT");

    var configfile = fso.CreateTextFile("win\\configure.data", true);
    for (i=0; i < args.Count(); i++)
    {
        var parts = args.Item(i).split('=');
        switch (parts[0])
        {
            case "WITH_ARCHIVE_STORAGE_ENGINE":
            case "WITH_BLACKHOLE_STORAGE_ENGINE":
            case "WITH_EXAMPLE_STORAGE_ENGINE":
            case "WITH_FEDERATED_STORAGE_ENGINE":
            case "WITH_INNOBASE_STORAGE_ENGINE":
            case "WITH_PARTITION_STORAGE_ENGINE":
            case "__NT__":
            case "CYBOZU":
                    configfile.WriteLine("SET (" + args.Item(i) + " TRUE)");
                    break;
            case "MYSQL_SERVER_SUFFIX":
                    configfile.WriteLine("SET (" + parts[0] + " \""
                                         + parts[1] + "\")");
                    break;
            case "COMPILATION_COMMENT":
                    default_comment = parts[1];
                    break;
            case "MYSQL_TCP_PORT":
                    default_port = parts[1];
                    break;
        }
    }

    configfile.WriteLine("SET (COMPILATION_COMMENT \"" +
                         default_comment + "\")");

    configfile.WriteLine("SET (PROTOCOL_VERSION \"" +
                         GetValue(configureIn, "PROTOCOL_VERSION") + "\")");
    configfile.WriteLine("SET (DOT_FRM_VERSION \"" +
                         GetValue(configureIn, "DOT_FRM_VERSION") + "\")");
    configfile.WriteLine("SET (MYSQL_TCP_PORT \"" + default_port + "\")");
    configfile.WriteLine("SET (MYSQL_UNIX_ADDR \"" +
                         GetValue(configureIn, "MYSQL_UNIX_ADDR_DEFAULT") + "\")");
    var version = GetVersion(configureIn);
    configfile.WriteLine("SET (VERSION \"" + version + "\")");
    configfile.WriteLine("SET (MYSQL_BASE_VERSION \"" +
                         GetBaseVersion(version) + "\")");
    configfile.WriteLine("SET (MYSQL_VERSION_ID \"" +
                         GetVersionId(version) + "\")");

    configfile.Close();
    
    fso = null;

    WScript.Echo("done!");
}
catch (e)
{
    WScript.Echo("Error: " + e.description);
}

function GetValue(str, key)
{
    var pos = str.indexOf(key+'=');
    if (pos == -1) return null;
    pos += key.length + 1;
    var end = str.indexOf("\n", pos);
    if (str.charAt(pos) == "\"")
        pos++;
    if (str.charAt(end-1) == "\"")
        end--;
    return str.substring(pos, end);    
}

function GetVersion(str)
{
    var key = "AM_INIT_AUTOMAKE(mysql, ";
    var pos = str.indexOf(key); //5.0.6-beta)
    if (pos == -1) return null;
    pos += key.length;
    var end = str.indexOf(")", pos);
    if (end == -1) return null;
    return str.substring(pos, end);
}

function GetBaseVersion(version)
{
    var dot = version.indexOf(".");
    if (dot == -1) return null;
    dot = version.indexOf(".", dot+1);
    if (dot == -1) dot = version.length;
    return version.substring(0, dot);
}

function GetVersionId(version)
{
    var dot = version.indexOf(".");
    if (dot == -1) return null;
    var major = parseInt(version.substring(0, dot), 10);
    
    dot++;
    var nextdot = version.indexOf(".", dot);
    if (nextdot == -1) return null;
    var minor = parseInt(version.substring(dot, nextdot), 10);
    dot = nextdot+1;
    
    var stop = version.indexOf("-", dot);
    if (stop == -1) stop = version.length;
    var build = parseInt(version.substring(dot, stop), 10);
    
    var id = major;
    if (minor < 10)
        id += '0';
    id += minor;
    if (build < 10)
        id += '0';
    id += build;
    return id;
}
