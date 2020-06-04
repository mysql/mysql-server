require({cache:{
'dijit/main':function(){
define([
	"dojo/_base/kernel"
], function(dojo){
	// module:
	//		dijit/main

/*=====
return {
	// summary:
	//		The dijit package main module.
	//		Deprecated.   Users should access individual modules (ex: dijit/registry) directly.
};
=====*/

	return dojo.dijit;
});

},
'dojox/main':function(){
define(["dojo/_base/kernel"], function(dojo) {
	// module:
	//		dojox/main

	/*=====
	return {
		// summary:
		//		The dojox package main module; dojox package is somewhat unusual in that the main module currently just provides an empty object.
		//		Apps should require modules from the dojox packages directly, rather than loading this module.
	};
	=====*/

	return dojo.dojox;
});
},
'dojo/require':function(){
define(["./_base/loader"], function(loader){
	return {
		dynamic:0,
		normalize:function(id){return id;},
		load:loader.require
	};
});

},
'dojox/mobile/compat':function(){
define([
	"dojo/_base/lang",
	"dojo/sniff"
], function(lang, has){
	// module:
	//		dojox/mobile/compat

	var dm = lang.getObject("dojox.mobile", true);
	// TODO: Use feature detection instead, but this would require a major rewrite of _compat
	// to detect each feature and plug the corresponding compat code if needed.
	// Currently the compat code is a workaround for too many different things to be able to
	// decide based on feature detection. So for now we just disable _compat on the mobile browsers
	// that are known to support enough CSS3: all webkit-based browsers, IE10 (Windows [Phone] 8) and IE11+.
	if(!(has("webkit") || has("ie") === 10) || (!has("ie") && has("trident") > 6)){
		var s = "dojox/mobile/_compat"; // assign to a variable so as not to be picked up by the build tool
		require([s]);
	}
	
	/*=====
	return {
		// summary:
		//		CSS3 compatibility module.
		// description:
		//		This module provides to dojox/mobile support for some of the CSS3 features 
		//		in non-CSS3 browsers, such as IE or Firefox.
		//		If you require this module, when running in a non-CSS3 browser it directly 
		//		replaces some of the methods of	dojox/mobile classes, without any subclassing. 
		//		This way, HTML pages remain the same regardless of whether this compatibility 
		//		module is used or not.
		//
		//		Example of usage: 
		//		|	require([
		//		|		"dojox/mobile",
		//		|		"dojox/mobile/compat",
		//		|		...
		//		|	], function(...){
		//		|		...
		//		|	});
		//
		//		This module also loads compatibility CSS files, which have a -compat.css
		//		suffix. You can use either the `<link>` tag or `@import` to load theme
		//		CSS files. Then, this module searches for the loaded CSS files and loads
		//		compatibility CSS files. For example, if you load dojox/mobile/themes/iphone/iphone.css
		//		in a page, this module automatically loads dojox/mobile/themes/iphone/iphone-compat.css.
		//		If you explicitly load iphone-compat.css with `<link>` or `@import`,
		//		this module will not load again the already loaded file.
		//
		//		Note that, by default, compatibility CSS files are only loaded for CSS files located
		//		in a directory containing a "mobile/themes" path. For that, a matching is done using 
		//		the default pattern	"/\/mobile\/themes\/.*\.css$/". If a custom theme is not located 
		//		in a directory containing this path, the data-dojo-config needs to specify a custom 
		//		pattern using the "mblLoadCompatPattern" configuration parameter, for instance:
		// |	data-dojo-config="mblLoadCompatPattern: /\/mycustomtheme\/.*\.css$/"
	};
	=====*/
	return dm;
});

}}});
// wrapped by build app
define("dojox/mobile/app/compat", ["dojo","dijit","dojox","dojo/require!dojox/mobile/compat"], function(dojo,dijit,dojox){
dojo.provide("dojox.mobile.app.compat");
dojo.require("dojox.mobile.compat");

// summary:
//		CSS3 compatibility module for apps
// description:
//		This module provides support for some of the CSS3 features to djMobile
//		for non-CSS3 browsers, such as IE or Firefox.
//		If you load this module, it directly replaces some of the methods of
//		djMobile instead of subclassing. This way, html pages remains the same
//		regardless of whether this compatibility module is used or not.
//		Recommended usage is as follows. the code below loads dojox.mobile.compat
//		only when isWebKit is true.
//
//		dojo.require("dojox.mobile");
//		dojo.requireIf(!dojo.isWebKit, "dojox.mobile.appCompat");

dojo.extend(dojox.mobile.app.AlertDialog, {
	_doTransition: function(dir){
		console.log("in _doTransition and this = ", this);

		var h = dojo.marginBox(this.domNode.firstChild).h;

		var bodyHeight = this.controller.getWindowSize().h;
	
		var high = bodyHeight - h;
		var low = bodyHeight;

		var anim1 = dojo.fx.slideTo({
			node: this.domNode,
			duration: 400,
			top: {start: dir < 0 ? high : low, end: dir < 0 ? low: high}
		});

		var anim2 = dojo[dir < 0 ? "fadeOut" : "fadeIn"]({
			node: this.mask,
			duration: 400
		});
	
		var anim = dojo.fx.combine([anim1, anim2]);
	
		var _this = this;

		dojo.connect(anim, "onEnd", this, function(){
			if(dir < 0){
				_this.domNode.style.display = "none";
				dojo.destroy(_this.domNode);
				dojo.destroy(_this.mask);
			}
		});
		anim.play();
	}
});

dojo.extend(dojox.mobile.app.List, {
	deleteRow: function(){
		console.log("deleteRow in compat mode", row);
	
		var row = this._selectedRow;
		// First make the row invisible
		// Put it back where it came from
		dojo.style(row, {
			visibility: "hidden",
			minHeight: "0px"
		});
		dojo.removeClass(row, "hold");
	
	
		// Animate reducing it's height to zero, then delete the data from the
		// array
		var height = dojo.contentBox(row).h;
		dojo.animateProperty({
				node: row,
				duration: 800,
				properties: {
				height: {start: height, end: 1},
				paddingTop: {end: 0},
				paddingBottom: {end: 0}
			},
			onEnd: this._postDeleteAnim
		}).play();
	}
});

if(dojox.mobile.app.ImageView && !dojo.create("canvas").getContext){
	dojo.extend(dojox.mobile.app.ImageView, {
		buildRendering: function(){
			this.domNode.innerHTML =
				"ImageView widget is not supported on this browser."
				+ "Please try again with a modern browser, e.g. "
				+ "Safari, Chrome or Firefox";
			this.canvas = {};
		},
		
		postCreate: function(){}
	});
}

if(dojox.mobile.app.ImageThumbView){
	dojo.extend(dojox.mobile.app.ImageThumbView, {
		place: function(node, x, y){
			dojo.style(node, {
				top: y + "px",
				left: x + "px",
				visibility: "visible"
			});
		}
	})
}

});
