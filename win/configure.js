// Configure.js
//
// Copyright (C) 2006 MySQL AB
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; version 2 of the License.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

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
    var actual_port = 0;

    var configfile = fso.CreateTextFile("win\\configure.data", true);
    for (i=0; i < args.Count(); i++)
    {
        var parts = args.Item(i).split('=');
        switch (parts[0])
        {
            case "WITH_COMMUNITY_FEATURES":
            case "WITH_ARCHIVE_STORAGE_ENGINE":
            case "WITH_BLACKHOLE_STORAGE_ENGINE":
            case "WITH_EXAMPLE_STORAGE_ENGINE":
            case "WITH_FEDERATED_STORAGE_ENGINE":
            case "WITH_INNOBASE_STORAGE_ENGINE":
            case "__NT__":
            case "DISABLE_GRANT_OPTIONS":
            case "EMBED_MANIFESTS":
                    configfile.WriteLine("SET (" + args.Item(i) + " TRUE)");
                    break;
            // BDB includes auto-generated files which are not handled by
            // cmake, so only allow this option if building from a source
            // distribution
            case "WITH_BERKELEY_STORAGE_ENGINE":
                if (!fso.FileExists("bdb\\btree\\btree_auto.c"))
                {
                    BDBSourceError = new Error("BDB cannot be built directly" +
                      " from a bzr tree, see comment in bdb\\CMakeLists.txt");
                    throw BDBSourceError;
                }
                else
                {
                    configfile.WriteLine("SET (" + args.Item(i) + " TRUE)");
                    break;
                }
            case "MYSQL_SERVER_SUFFIX":
            case "MYSQLD_EXE_SUFFIX":
                    configfile.WriteLine("SET (" + parts[0] + " \""
                                         + parts[1] + "\")");
                    break;
            case "COMPILATION_COMMENT":
                    default_comment = parts[1];
                    break;
            case "MYSQL_TCP_PORT":
                    actual_port = parts[1];
                    break;
        }
    }
    if (actual_port == 0)
	{
       // if we actually defaulted (as opposed to the pathological case of
       // --with-tcp-port=<MYSQL_TCP_PORT_DEFAULT> which might in theory
       // happen if whole batch of servers was built from a script), set
       // the default to zero to indicate that; we don't lose information
       // that way, because 0 obviously indicates that we can get the
       // default value from MYSQL_TCP_PORT. this seems really evil, but
       // testing for MYSQL_TCP_PORT==MYSQL_TCP_PORT_DEFAULT would make a
       // a port of MYSQL_TCP_PORT_DEFAULT magic even if the builder did not
       // intend it to mean "use the default, in fact, look up a good default
       // from /etc/services if you can", but really, really meant 3306 when
       // they passed in 3306. When they pass in a specific value, let them
       // have it; don't second guess user and think we know better, this will
       // just make people cross.  this makes the the logic work like this
       // (which is complicated enough):
       // 
       // - if a port was set during build, use that as a default.
       // 
       // - otherwise, try to look up a port in /etc/services; if that fails,
       //   use MYSQL_TCP_PORT_DEFAULT (at the time of this writing 3306)
       // 
       // - allow the MYSQL_TCP_PORT environment variable to override that.
       // 
       // - allow command-line parameters to override all of the above.
       // 
       // the top-most MYSQL_TCP_PORT_DEFAULT is read from win/configure.js,
       // so don't mess with that.
	   actual_port = default_port;
	   default_port = 0;
	}

    configfile.WriteLine("SET (COMPILATION_COMMENT \"" +
                         default_comment + "\")");

    configfile.WriteLine("SET (PROTOCOL_VERSION \"" +
                         GetValue(configureIn, "PROTOCOL_VERSION") + "\")");
    configfile.WriteLine("SET (DOT_FRM_VERSION \"" +
                         GetValue(configureIn, "DOT_FRM_VERSION") + "\")");
    configfile.WriteLine("SET (MYSQL_TCP_PORT_DEFAULT \"" + default_port + "\")");
    configfile.WriteLine("SET (MYSQL_TCP_PORT \"" + actual_port + "\")");
    configfile.WriteLine("SET (MYSQL_UNIX_ADDR \"" +
                         GetValue(configureIn, "MYSQL_UNIX_ADDR_DEFAULT") + "\")");
    var version = GetVersion(configureIn);
    configfile.WriteLine("SET (VERSION \"" + version + "\")");
    configfile.WriteLine("SET (MYSQL_BASE_VERSION \"" +
                         GetBaseVersion(version) + "\")");
    configfile.WriteLine("SET (MYSQL_VERSION_ID \"" +
                         GetVersionId(version) + "\")");

    configfile.Close();
    
    //ConfigureBDB();

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
    if (str.charAt(end-1) == "\r")
        end--;
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

function ConfigureBDB() 
{
    // read in the Unix configure.in file
    var dbIncTS = fso.OpenTextFile("..\\bdb\\dbinc\\db.in", ForReading);
    var dbIn = dbIncTS.ReadAll();
    dbIncTS.Close();

    dbIn = dbIn.replace("@DB_VERSION_MAJOR@", "$DB_VERSION_MAJOR");
    dbIn = dbIn.replace("@DB_VERSION_MINOR@", "$DB_VERSION_MINOR");
    dbIn = dbIn.replace("@DB_VERSION_PATCH@", "$DB_VERSION_PATCH");
    dbIn = dbIn.replace("@DB_VERSION_STRING@", "$DB_VERSION_STRING");

    dbIn = dbIn.replace("@u_int8_decl@", "typedef unsigned char u_int8_t;");
    dbIn = dbIn.replace("@int16_decl@", "typedef short int16_t;");
    dbIn = dbIn.replace("@u_int16_decl@", "typedef unsigned short u_int16_t;");
    dbIn = dbIn.replace("@int32_decl@", "typedef int int32_t;");
    dbIn = dbIn.replace("@u_int32_decl@", "typedef unsigned int u_int32_t;");

    dbIn = dbIn.replace("@u_char_decl@", "{\r\n#if !defined(_WINSOCKAPI_)\r\n" +
        "typedef unsigned char u_char;");
    dbIn = dbIn.replace("@u_short_decl@", "typedef unsigned short u_short;");
    dbIn = dbIn.replace("@u_int_decl@", "typedef unsigned int u_int;");
    dbIn = dbIn.replace("@u_long_decl@", "typedef unsigned long u_long;");
    
    dbIn = dbIn.replace("@ssize_t_decl@", "#endif\r\n#if defined(_WIN64)\r\n" +
        "typedef __int64 ssize_t;\r\n#else\r\n" +
        "typedef int ssize_t;\r\n#endif");
}
