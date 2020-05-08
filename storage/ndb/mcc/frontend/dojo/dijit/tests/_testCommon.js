// module:
//		dijit/tests/_testCommon.js
// description:
//		Deprecated, remove in 2.0.   New test files should use boilerplate.html rather than this file,
//		and non-test files shouldn't be using either.

require([
	"require",
	"dojo/_base/array",
	"dojo/_base/config",
	"dojo/dom",
	"dojo/dom-attr",
	"dojo/dom-class",
	"dojo/dom-construct",
	"dojo/_base/kernel",
	"dojo/_base/lang",
	"dojo/query",
	"dojo/ready",
	"dojo/_base/window"
], function(require, array, config, dom, domAttr, domClass, domConstruct, kernel, lang, query, ready, win){

	var dir = "",
		theme = false,
		themeModule = "dijit",
		testMode = null,
		defTheme = "claro",
		vars={};

	if(window.location.href.indexOf("?") > -1){
		var str = window.location.href.substr(window.location.href.indexOf("?")+1).split(/#/);
		var ary  = str[0].split(/&/);
		for(var i=0; i<ary.length; i++){
			var split = ary[i].split("="),
				key = split[0],
				value = (split[1]||'').replace(/[^\w]/g, "");	// replace() to prevent XSS attack
			switch(key){
				case "locale":
					// locale string | null
					kernel.locale = config.locale = locale = value;
					break;
				case "dir":
					// rtl | null
					document.getElementsByTagName("html")[0].dir = value;
					dir = value;
					break;
				case "theme":
					// tundra | soria | nihilo | claro | null
					theme = value;
					break;
				case "a11y":
					if(value){ testMode = "dj_a11y"; }
					break;
				case "themeModule":
					// moduleName | null
					if(value){ themeModule = value; }
			}
			vars[key] = value;
		}
	}
	kernel._getVar = function(k, def){	// TODO: not sure what this is
		return vars[k] || def;
	};

	// BIDI
	if(dir == "rtl"){
		ready(0, function(){
			// pretend all the labels are in an RTL language, because
			// that affects how they lay out relative to inline form widgets
			query("label").attr("dir", "rtl");
		});
	}

	// a11y
	if(testMode){
		ready(0, function(){
			var b = win.body();
			if(testMode){
				domClass.add(b, testMode);
			}
		});
	}

	// If URL specifies a non-claro theme then pull in those theme CSS files and modify
	// <body> to point to that new theme instead of claro.
	//
	// Also defer parsing and any dojo.ready() calls that the test file makes
	// until the CSS has finished loading.
	if(theme){
		// Wait until JS modules have finished loading so this doesn't confuse
		// AMD loader.
		ready(1, function(){
			// Reset <body> to point to the specified theme
			var b = win.body();
			domClass.replace(b, theme, defTheme);

			// Remove claro CSS
			query('link[href$="claro.css"]').orphan();
			query('link[href$="claro/document.css"]').orphan();

			// Load theme CSS.
			// Eventually would like to use [something like]
			// https://github.com/unscriptable/curl/blob/master/src/curl/plugin/css.js
			// to load the CSS and then know exactly when it finishes loading.
			var modules = [
				require.toUrl(themeModule+"/themes/"+theme+"/"+theme+".css"),
				require.toUrl(themeModule+"/themes/"+theme+"/"+theme+"_rtl.css"),
				require.toUrl("dojo/resources/dojo.css")
			];
			var head = query("head")[0];
			array.forEach(modules, function(css){
				if(document.createStyleSheet){
					// For IE
					document.createStyleSheet(css);
				}else{
					// For other browsers
					domConstruct.place('<link rel="stylesheet" type="text/css" href="'+css+'"/>',
						head);
				}
			});
		});
		ready(2, function(){
			// Delay parsing and other dojo.ready() callbacks (except ones in this file)
			// until the injected <link>'s above have finished loading.
			require(["dijit/tests/delay!320"]);
		});
	}
});
