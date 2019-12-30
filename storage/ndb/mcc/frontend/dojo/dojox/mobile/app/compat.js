/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

/*
	This is an optimized version of Dojo, built for deployment and not for
	development. To get sources and documentation, please visit:

		http://dojotoolkit.org
*/

//>>built
require({cache:{"dijit/main":function(){define("dijit/main",["dojo/_base/kernel"],function(_1){return _1.dijit;});},"dojox/main":function(){define("dojox/main",["dojo/_base/kernel"],function(_2){return _2.dojox;});},"dojo/require":function(){define(["./_base/loader"],function(_3){return {dynamic:0,normalize:function(id){return id;},load:_3.require};});},"dojox/mobile/compat":function(){define("dojox/mobile/compat",["dojo/_base/lang","dojo/_base/sniff"],function(_4,_5){var dm=_4.getObject("dojox.mobile",true);if(!_5("webkit")){var s="dojox/mobile/_compat";require([s]);}return dm;});}}});define("dojox/mobile/app/compat",["dojo","dijit","dojox","dojo/require!dojox/mobile/compat"],function(_6,_7,_8){_6.provide("dojox.mobile.app.compat");_6.require("dojox.mobile.compat");_6.extend(_8.mobile.app.AlertDialog,{_doTransition:function(_9){var h=_6.marginBox(this.domNode.firstChild).h;var _a=this.controller.getWindowSize().h;var _b=_a-h;var _c=_a;var _d=_6.fx.slideTo({node:this.domNode,duration:400,top:{start:_9<0?_b:_c,end:_9<0?_c:_b}});var _e=_6[_9<0?"fadeOut":"fadeIn"]({node:this.mask,duration:400});var _f=_6.fx.combine([_d,_e]);var _10=this;_6.connect(_f,"onEnd",this,function(){if(_9<0){_10.domNode.style.display="none";_6.destroy(_10.domNode);_6.destroy(_10.mask);}});_f.play();}});_6.extend(_8.mobile.app.List,{deleteRow:function(){var row=this._selectedRow;_6.style(row,{visibility:"hidden",minHeight:"0px"});_6.removeClass(row,"hold");var _11=_6.contentBox(row).h;_6.animateProperty({node:row,duration:800,properties:{height:{start:_11,end:1},paddingTop:{end:0},paddingBottom:{end:0}},onEnd:this._postDeleteAnim}).play();}});if(_8.mobile.app.ImageView&&!_6.create("canvas").getContext){_6.extend(_8.mobile.app.ImageView,{buildRendering:function(){this.domNode.innerHTML="ImageView widget is not supported on this browser."+"Please try again with a modern browser, e.g. "+"Safari, Chrome or Firefox";this.canvas={};},postCreate:function(){}});}if(_8.mobile.app.ImageThumbView){_6.extend(_8.mobile.app.ImageThumbView,{place:function(_12,x,y){_6.style(_12,{top:y+"px",left:x+"px",visibility:"visible"});}});}});