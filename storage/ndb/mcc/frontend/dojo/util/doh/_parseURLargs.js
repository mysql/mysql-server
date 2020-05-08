(function(){
	var
		boot =
			// zero to many scripts to load a configuration and/or loader.
			// i.e. path-to-util/doh/runner.html?boots=path-to/config.js,path-to/require.js
			["../../dojo/dojo.js"],

		standardDojoBoot = boot,

		test =
			// zero to many AMD modules and/or URLs to load; provided by csv URL query parameter="test"
			// For example, the URL...
			//
			//		 path-to-util/doh/runner.html?test=doh/selfTest,my/path/test.js
			//
			// ...will load...
			//
			//	 * the AMD module doh/selfTest
			//	 * the plain old Javascript resource my/path/test.js
			//
			["dojo/tests/module"],

		paths =
			// zero to many path items to pass to the AMD loader; provided by semicolon separated values
			// for URL query parameter="paths"; each path item has the form <from-path>,<to-path>
			// i.e. path-to-util/doh/runner.html?paths=my/from/path,my/to/path;my/from/path2,my/to/path2
			{},

		dohPlugins =
			// Semicolon separated list of files to load before the tests.
			// Idea is to override aspects of DOH for reporting purposes.
			"",

		breakOnError =
			// boolean; instructs doh to call the debugger upon a test failures; this can be helpful when
			// trying to isolate exactly where the test failed
			false,

		async =
			// boolean; config require.async==true before loading boot; this will have the effect of making
			// version 1.7+ dojo bootstrap/loader operating in async mode
			false,

		sandbox =
			// boolean; use a loader configuration that sandboxes the dojo and dojox objects used by doh
			false,

		trim = function(text){
			if(text instanceof Array){
				for (var result= [], i= 0; i<text.length; i++) {
					result.push(trim(text[i]));
				}
				return result;
			}else{
				return text.match(/[^\s]*/)[0];
			}
		},
		packages= [];

		qstr = window.location.search.substr(1);

	if(qstr.length){
		for(var qparts = qstr.split("&"), x = 0; x < qparts.length; x++){
			var tp = qparts[x].split("="), name=tp[0], value=(tp[1]||"").replace(/[<>"':\(\)]/g, ""); // replace() to avoid XSS attack
			//Avoid URLs that use the same protocol but on other domains, for security reasons.
			if (value.indexOf("//") === 0 || value.indexOf("\\\\") === 0) {
				throw "Insupported URL";
			}
			switch(name){
				// Note:
				//	 * dojoUrl is deprecated, and is a synonym for boot
				//	 * testUrl is deprecated, and is a synonym for test
				//	 * testModule is deprecated, and is a synonym for test (dots are automatically replaced with slashes)
				//	 * registerModulePath is deprecated, and is a synonym for paths
				case "boot":
				case "dojoUrl":
					boot= trim(value.split(","));
					break;

				case "test":
				case "testUrl":
					test= trim(value.split(","));
					break;

				case "testModule":
					test= trim(value.replace(/\./g, "/").split(","));
					break;

				// registerModulePath is deprecated; use "paths"
				case "registerModulePath":
				case "paths":
					for(var path, modules = value.split(";"), i= 0; i<modules.length; i++){
						path= modules[i].split(",");
						paths[trim(path[0])]= trim(path[1]);
					}
					break;

				case "breakOnError":
					breakOnError= true;
					break;

				case "sandbox":
					sandbox= true;
					break;

				case "async":
					async= true;
					break;
				case "dohPlugins":
					dohPlugins=value.split(";");
					break;
				case 'mapPackage':
					var packagesRaw=value.split(';');
					for (var i = 0; i < packagesRaw.length; i++) {
						var parts = packagesRaw[i].split(',');
						packages.push({
							name: parts[0],
							location: parts[1]
						});
					}

					break;
			}
		}
	}

	var config;
	if(sandbox){
		// configure the loader assuming the dojo loader; of course the injected boot(s) can override this config
		config= {
			paths: paths,
			// this config uses the dojo loader's scoping features to sandbox the version of dojo used by doh
			packages: [{
				name: 'doh',
				location: '../util/doh',
				// here's the magic...every time doh asks for a "dojo" module, it gets mapped to a "dohDojo"
				// module; same goes for dojox/dohDojox since doh uses dojox
				packageMap: {dojo:"dohDojo", dojox:"dohDojox"}
			},{
				// now define the dohDojo package...
				name: 'dohDojo',
				location: '../dojo',
				packageMap: {dojo: "dohDojo", dojox: "dohDojox"}
			},{
				// and the dohDojox package...
				name: 'dohDojox',
				location: '../dojox',
				// and dojox uses dojo...that is, dohDojox...which must be mapped to dohDojo in the context of dohDojox
				packageMap: {dojo: "dohDojo", dojox: "dohDojox"}
			}],

			// next, we need to preposition a special configuration for dohDojo
			cache: {
				"dohDojo*_base/config": function(){
					define([], {
						// this configuration keeps dojo, dijit, and dojox out of the global space
						scopeMap: [["dojo", "dohDojo"], ["dijit", "dohDijit"], ["dojox", "dohDojox"]],
						isDebug: true,
						noGlobals: true
					});
				}
			},

			// control the loader; don't boot global dojo, doh will ask for dojo itself
			has: {
				"dojo-sniff": 0,
				"dojo-loader": 1,
				"dojo-boot": 0,
				"dojo-test-sniff": 1
			},

			// no sniffing; therefore, set the baseUrl
			baseUrl: "../../dojo",

			deps: ["dohDojo/domReady", "doh"],

			callback: callback,

			async: async
		};
	}else{
		config= {
			// override non-standard behavior of V1 dojo.js, which sets baseUrl to dojo dir
			baseUrl: "../..",
			tlmSiblingOfDojo: false,

			paths: paths,
			packages: [
				{ name: 'doh', location: 'util/doh' },

				// override non-standard behavior of V1 dojo.js, which remaps these packages
				'dojo',
				'dijit',
				'dojox'
			],
			deps: ["dojo/domReady", "doh/main"],
			callback: callback,
			async: async,
			isDebug: 1
		};
	}

	for (var i = 0; i < packages.length; i++) {
		var isFound = false;
		for (var j = 0; j < config.packages.length; j++) {
			if (packages[i].name == config.packages[j].name) {
				isFound = true;
				config.packages[j].location = packages[i].location;
				break;
			}
		}
		if (!isFound) {
			config.packages.push(packages[i]);
		}
	}

	function callback(domReady, doh){
		domReady(function(){
			var amdTests = [], module;
			doh._fixHeight();
			doh.breakOnError= breakOnError;
			for (var i = 0, l = test.length; i < l; i++) {
				module = test[i];
				if(/\.html$/.test(module)){
					doh.register(module, require.toUrl(module), 999999);
				}else{
					amdTests.push(module);
				}
			}
			require(amdTests, function(){
				doh.run();
			});
		});
	}

	// load all of the dohPlugins
	if(dohPlugins){
		var i = 0;
		for(i = 0; i < dohPlugins.length; i++){
			config.deps.push(dohPlugins[i]);
		}
	}

	require = config;

	// now script inject any boots
	for(var e, i = 0; i < boot.length; i++) {
		if(boot[i]){
			e = document.createElement("script");
			e.type = "text/javascript";
			e.src = boot[i];
			e.charset = "utf-8";
			document.getElementsByTagName("head")[0].appendChild(e);
		}
	}
})();
