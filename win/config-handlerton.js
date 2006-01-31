// Configure.js

ForReading = 1;
ForWriting = 2;
ForAppending = 8;

try 
{
	// first we attempt to open the main configure.in file
    var fso = new ActiveXObject("Scripting.FileSystemObject");

	var args = WScript.Arguments

    var datafile = fso.OpenTextFile(args.Item(0), ForReading);
	var extern_line = '';
	var address_line = '';
	while (! datafile.AtEndOfStream)
	{
		var line = datafile.ReadLine();
		if (line == "WITH_INNODB")
		{
			extern_line += ",innobase_hton";
			address_line += ",&innobase_hton";
		}
		else if (line == "WITH_PARTITION")
		{
			extern_line += ",partition_hton";
			address_line += ",&partition_hton";
		}
	}
	datafile.Close();
	
    var infile = fso.OpenTextFile("..\\sql\\handlerton.cc.in", ForReading);
    var infile_contents = infile.ReadAll();
    infile.Close();
    infile_contents = infile_contents.replace("@mysql_se_decls@", extern_line);
    infile_contents = infile_contents.replace("@mysql_se_htons@", address_line);
    
    var outfile = fso.CreateTextFile("..\\sql\\handlerton.cc", true)
    outfile.Write(infile_contents);
    outfile.Close();
	
	fso = null;

    WScript.Echo("done!");
}
catch (e)
{
    WScript.Echo("Error: " + e.description);
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

function ConfigureMySqlVersion()
{
    // read in the Unix configure.in file
    var configureInTS = fso.OpenTextFile("..\\configure.in", ForReading);
    var configureIn = configureInTS.ReadAll();
    configureInTS.Close();
    
    // read in the mysql_version.h.in file
    var mysqlTS = fso.OpenTextFile("..\\include\\mysql_version.h.in", ForReading);
    var mysqlin = mysqlTS.ReadAll();
    mysqlTS.Close();
    
    mysqlin = mysqlin.replace("@PROTOCOL_VERSION@", GetValue(configureIn, "PROTOCOL_VERSION"));
    mysqlin = mysqlin.replace("@DOT_FRM_VERSION@", GetValue(configureIn, "DOT_FRM_VERSION"));
    mysqlin = mysqlin.replace("@MYSQL_TCP_PORT@", GetValue(configureIn, "MYSQL_TCP_PORT_DEFAULT"));
    mysqlin = mysqlin.replace("@MYSQL_UNIX_ADDR@", GetValue(configureIn, "MYSQL_UNIX_ADDR_DEFAULT"));
    mysqlin = mysqlin.replace("@MYSQL_SERVER_SUFFIX@", '');
    mysqlin = mysqlin.replace("@COMPILATION_COMMENT@", 'Source distribution');


    var version = GetVersion(configureIn);
    mysqlin = mysqlin.replace("@VERSION@", version);
    mysqlin = mysqlin.replace("@MYSQL_BASE_VERSION@", GetBaseVersion(version));
    mysqlin = mysqlin.replace("@MYSQL_VERSION_ID@", GetVersionId(version));


    var mysqlfile = fso.CreateTextFile("..\\include\\mysql_version.h", true);
    mysqlfile.Write(mysqlin);
    mysqlfile.Close();

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

