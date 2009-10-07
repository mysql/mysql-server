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
    var with_maria_tmp_tables = -1;

    var configfile = fso.CreateTextFile("win\\configure.data", true);
    for (i=0; i < args.Count(); i++)
    {
        var parts = args.Item(i).split('=');
        switch (parts[0])
        {
            case "CYBOZU":
            case "EMBED_MANIFESTS":
            case "EXTRA_DEBUG":
            case "WITH_EMBEDDED_SERVER":
            case "WITHOUT_MARIA_TEMP_TABLES":
                    configfile.WriteLine("SET (" + args.Item(i) + " TRUE)");
                    break;
            case "WITH_MARIA_STORAGE_ENGINE":
                    configfile.WriteLine("SET (" + args.Item(i) + " TRUE)");
                    if(with_maria_tmp_tables == -1)
                    {
                      with_maria_tmp_tables = 1;
                    }
                    break;
            case "WITH_MARIA_TMP_TABLES":
                    with_maria_tmp_tables = ( parts.length == 1 ||
                           parts[1] == "YES" || parts[1] == "TRUE");
                    break;
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
    if (with_maria_tmp_tables == 1)
    {
      configfile.WriteLine("SET (WITH_MARIA_TMP_TABLES TRUE)");
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
    var engineOptions = ParsePlugins();
    for (option in engineOptions)
    {
       configfile.WriteLine("SET(" + engineOptions[option] + " TRUE)");
    }
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
    var key2 = "AM_INIT_AUTOMAKE(mariadb, ";
    var key_len = key.length;
    var pos = str.indexOf(key); //5.0.6-beta)
    if (pos == -1)
    {
      pos = str.indexOf(key2);
      key_len= key2.length;
    }
    if (pos == -1) return null;
    pos += key_len;
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

function PluginConfig(isGroup, include)
{
    this.isGroup = isGroup;
    this.include = include;
}


// Parse command line arguments specific to plugins (aka storage engines).
//
// --with-plugin-PLUGIN, --with-plugins=group,  --with-plugins=PLUGIN[,PLUGIN...]
// --without-plugin-PLUGIN is supported.
//
// Legacy option WITH_<PLUGIN>_STORAGE_ENGINE is supported as well.
// The function returns string array with elements like WITH_SOME_STORAGE_ENGINE 
// or WITHOUT_SOME_STORAGE_ENGINE.
//
// This function handles groups, for example effect of specifying --with-plugins=max 
// is the same as --with-plugins==archive,federated,falcon,innobase...

function ParsePlugins()
{

    var config = new Array();  

    config["DEFAULT"] = new PluginConfig(true,true);
    
    // Parse command line parameters
    for (i=0; i< WScript.Arguments.length;i++)
    {
        var option = WScript.Arguments.Item(i);
        var match = /WITH_(\w+)_STORAGE_ENGINE/.exec(option);
        if (match == null)
            match = /--with-plugin-(\w+)/.exec(option);
        if (match != null)
        {
            config[match[1].toUpperCase()] =  new PluginConfig(false,true);
            continue;
        }

        match = /WITHOUT_(\w+)_STORAGE_ENGINE/.exec(option);
        if (match == null)
            match = /--without-plugin-(\w+)/.exec(option);
    
        if (match != null)
        {
            config[match[1].toUpperCase()] =  
                new PluginConfig(false,false);
            continue;
        }
        
        match = /--with-plugins=([\w,\-_]+)/.exec(option);
        if(match != null)
        {
        
            var plugins  = match[1].split(",");
            for(var key in plugins)
            {
                config[plugins[key].toUpperCase()] = 
                    new PluginConfig(null,true);
            }
            continue;
        }
        match = /--without-plugins=([\w,\-_]+)/.exec(option);
        if(match != null)
        {
            var plugins = match[1].split(",");
            for(var key in plugins)
                config[plugins[key].toUpperCase()] =
                    new PluginConfig(null, false);
            continue;
        }
    }
    
    // Read plugin definitions, find out groups plugins belong to.
    var fc = new Enumerator(fso.GetFolder("storage").SubFolders);
    for (;!fc.atEnd(); fc.moveNext())
    {
        var subfolder = fc.item();
        var name =  subfolder.name.toUpperCase();
        
        // Handle case where storage engine  was already specified by name in 
        // --with-plugins or --without-plugins.
        if (config[name] != undefined)
        {
            config[name].isGroup = false;
            continue;
        }
        config[name] = new PluginConfig(false,null);
        
        // Handle groups. For each plugin, find out which group it belongs to
        // If this group was specified on command line for inclusion/exclusion,
        // then include/exclude the plugin.
        filename  = subfolder +"\\plug.in";
        if (fso.FileExists(filename))
        {
            var content = fso.OpenTextFile(filename, ForReading).ReadAll();
            var match = 
              /MYSQL_STORAGE_ENGINE([ ]*)[\(]([^\)]+)[\)]/.exec(content);
            if (match== null)
                continue;
            match = /\[[\w,\-_]+\][\s]?\)/.exec(match[0]);
            if (match == null)
                continue;
            groups = match[0].split(/[\,\(\)\[\] ]/);
            for (var key in groups)
            {
                var group = groups[key].toUpperCase();
                if (config[group] != undefined)
                {
                    config[group].isGroup = true;
                    if (config[group].include != null)
                    {
                        config[name].include = config[group].include;
                        break;
                    }
                }
            }
        }
    }
    
    var arr = new Array();
    for(key in config)
    {
        var eng = config[key];
        if(eng.isGroup != undefined && !eng.isGroup	&& eng.include != undefined)
        {
            if (fso.FolderExists("storage\\"+key) || key=="PARTITION")
            {
                arr[arr.length] = eng.include? 
                    "WITH_"+key+"_STORAGE_ENGINE":"WITHOUT_"+key+"_STORAGE_ENGINE";
            }
        }
    }
    return arr;
}
