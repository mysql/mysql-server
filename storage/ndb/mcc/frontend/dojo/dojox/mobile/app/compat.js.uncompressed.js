/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

/*
	This is an optimized version of Dojo, built for deployment and not for
	development. To get sources and documentation, please visit:

		http://dojotoolkit.org
*/

//>>built
require({cache:{
'dojox/main':function(){
define(["dojo/_base/kernel"], function(dojo) {
	// module:
	//		dojox/main
	// summary:
	//		The dojox package main module; dojox package is somewhat unusual in that the main module currently just provides an empty object.

	return dojo.dojox;
});
},
'dojox/mobile/compat':function(){
define([
	"dojo/_base/lang",
	"dojo/_base/sniff"
], function(lang, has){
	var dm = lang.getObject("dojox.mobile", true);
	if(!has("webkit")){
		var s = "dojox/mobile/_compat"; // assign to a variable so as not to be picked up by the build tool
		require([s]);
	}
	return dm;
});

},
'dijit/main':function(){
define("dijit/main", [
	"dojo/_base/kernel"
], function(dojo){
	// module:
	//		dijit
	// summary:
	//		The dijit package main module

	return dojo.dijit;
});

}}});

// wrapped by build app
define("dojox/mobile/app/compat", ["dijit","dojo","dojox","dojo/require!dojox/mobile/compat"], function(dijit,dojo,dojox){
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
