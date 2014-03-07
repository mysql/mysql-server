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
require({cache:{"dojox/main":function(){define(["dojo/_base/kernel"],function(_1){return _1.dojox;});},"dojox/mobile/compat":function(){define(["dojo/_base/lang","dojo/_base/sniff"],function(_2,_3){var dm=_2.getObject("dojox.mobile",true);if(!_3("webkit")){var s="dojox/mobile/_compat";require([s]);}return dm;});},"dijit/main":function(){define("dijit/main",["dojo/_base/kernel"],function(_4){return _4.dijit;});}}});define("dojox/mobile/app/compat",["dijit","dojo","dojox","dojo/require!dojox/mobile/compat"],function(_5,_6,_7){_6.provide("dojox.mobile.app.compat");_6.require("dojox.mobile.compat");_6.extend(_7.mobile.app.AlertDialog,{_doTransition:function(_8){var h=_6.marginBox(this.domNode.firstChild).h;var _9=this.controller.getWindowSize().h;var _a=_9-h;var _b=_9;var _c=_6.fx.slideTo({node:this.domNode,duration:400,top:{start:_8<0?_a:_b,end:_8<0?_b:_a}});var _d=_6[_8<0?"fadeOut":"fadeIn"]({node:this.mask,duration:400});var _e=_6.fx.combine([_c,_d]);var _f=this;_6.connect(_e,"onEnd",this,function(){if(_8<0){_f.domNode.style.display="none";_6.destroy(_f.domNode);_6.destroy(_f.mask);}});_e.play();}});_6.extend(_7.mobile.app.List,{deleteRow:function(){var row=this._selectedRow;_6.style(row,{visibility:"hidden",minHeight:"0px"});_6.removeClass(row,"hold");var _10=_6.contentBox(row).h;_6.animateProperty({node:row,duration:800,properties:{height:{start:_10,end:1},paddingTop:{end:0},paddingBottom:{end:0}},onEnd:this._postDeleteAnim}).play();}});if(_7.mobile.app.ImageView&&!_6.create("canvas").getContext){_6.extend(_7.mobile.app.ImageView,{buildRendering:function(){this.domNode.innerHTML="ImageView widget is not supported on this browser."+"Please try again with a modern browser, e.g. "+"Safari, Chrome or Firefox";this.canvas={};},postCreate:function(){}});}if(_7.mobile.app.ImageThumbView){_6.extend(_7.mobile.app.ImageThumbView,{place:function(_11,x,y){_6.style(_11,{top:y+"px",left:x+"px",visibility:"visible"});}});}});