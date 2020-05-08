// module:
//		dijit/tests/boilerplate
// description:
//		A <script src="boilerplate.js"> on your test page will:
//
//		- load claro or a specified theme
//		- load the loader (i.e. define the require() method)
//
//		By URL flags you can specify the theme,
//		and optionally enable RTL (right to left) mode, and/or dj_a11y (high-
//		contrast/image off emulation) ... probably not a genuine test for a11y.
//
//		You should NOT be using this in a production environment.  Include
//		your css and set your classes manually:
//
//		<style type="text/css">
//			@import "dijit/themes/claro/document.css";
//		</style>
//		<link id="themeStyles" rel="stylesheet" href="dijit/themes/claro/claro.css"/>
//		<script type="text/javascript" src="dojo/dojo.js"></script>
//		...
//		<body class="claro">

var dir = "",
	theme = "claro",
	testMode = null;

dojoConfig = {
	async: true,
	isDebug: true,
	locale: "en-us"
};

// Parse the URL, get parameters
if(window.location.href.indexOf("?") > -1){
	var str = window.location.href.substr(window.location.href.indexOf("?")+1).split(/#/);
	var ary  = str[0].split(/&/);
	for(var i = 0; i < ary.length; i++){
		var split = ary[i].split("="),
			key = split[0],
			value = (split[1]||'').replace(/[^\w]/g, "");	// replace() to prevent XSS attack
		switch(key){
			case "locale":
				// locale string | null
				dojoConfig.locale = value;
				break;
			case "dir":
				// rtl | null
				dir = value;
				break;
			case "theme":
				// tundra | soria | nihilo | claro | null
				theme = /null|none/.test(value) ? null : value;
				break;
			case "a11y":
				if(value){ testMode = "dj_a11y"; }
				break;
		}
	}
}

// Find the <script src="boilerplate.js"> tag, to get test directory and data-dojo-config argument
var scripts = document.getElementsByTagName("script"), script, testDir;
for(i = 0; script = scripts[i]; i++){
	var src = script.getAttribute("src"),
		match = src && src.match(/(.*|^)boilerplate\.js/i);
	if(match){
		// Sniff location of dijit/tests directory relative to this test file.   testDir will be an empty string if it's
		// the same directory, or a string including a slash, ex: "../", if the test is in a subdirectory.
		testDir = match[1];

		// Sniff configuration on attribute in script element.
		// Allows syntax like <script src="boilerplate.js data-dojo-config="parseOnLoad: true">, where the settings
		// specified override the default settings.
		var attr = script.getAttribute("data-dojo-config");
		if(attr){
			var overrides = eval("({ " + attr + " })");
			for(var key in overrides){
				dojoConfig[key] = overrides[key];
			}
		}
		break;
	}
}

// Output the boilerplate text to load the theme CSS
if(theme){
	var themeDir = testDir + "../themes/" + theme + "/";
	document.write([
		'<style type="text/css">',
			theme == "claro" ? '@import "' + themeDir + 'document.css";' : "",
			'@import "' + testDir + 'css/dijitTests.css";',
		'</style>',
		'<link id="themeStyles" rel="stylesheet" href="' + themeDir + theme + '.css"/>'
	].join("\n"));
}

// Output the boilerplate text to load the loader, and to do some initial manipulation when the page finishes loading
// For 2.0 this should be changed to require the loader (ex: requirejs) directly, rather than dojo.js.
document.write('<script type="text/javascript" src="' + testDir + '../../dojo/dojo.js"></script>');

// On IE9 the following inlined script will run before dojo has finished loading, leading to an error because require()
// isn't defined yet.  Workaround it by putting the code in a separate file.
//document.write('<script type="text/javascript">require(["dojo/domReady!"], boilerplateOnLoad);</script>');
document.write('<script type="text/javascript" src="' + testDir + 'boilerplateOnload.js"></script>');

function boilerplateOnLoad(){
	// This function is the first registered domReady() callback, allowing us to setup
	// theme stuff etc. before the widgets start instantiating.

	// theme (claro, tundra, etc.)
	if(theme){
		// Set <body> to point to the specified theme
		document.body.className = theme;
	}

	// a11y (flag for faux high-contrast testing)
	if(testMode){
		document.body.className += " " + testMode;
	}

	// BIDI
	if(dir == "rtl"){
		// set dir=rtl on <html> node
		document.body.parentNode.setAttribute("dir", "rtl");

		require(["dojo/query!css2", "dojo/NodeList-dom"], function(query){
			// pretend all the labels are in an RTL language, because
			// that affects how they lay out relative to inline form widgets
			query("label").attr("dir", "rtl");
		});
	}

	// parseOnLoad: true requires that the parser itself be loaded.
	if(dojoConfig.parseOnLoad){
		require(["dojo/parser"]);
	}
}
