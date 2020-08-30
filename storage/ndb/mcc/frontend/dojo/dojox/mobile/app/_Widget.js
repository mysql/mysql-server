//>>built
define("dojox/mobile/app/_Widget",["dojo","dijit","dojox","dojo/require!dijit/_WidgetBase"],function(_1,_2,_3){
_1.provide("dojox.mobile.app._Widget");
_1.experimental("dojox.mobile.app._Widget");
_1.require("dijit._WidgetBase");
_1.declare("dojox.mobile.app._Widget",_2._WidgetBase,{getScroll:function(){
return {x:_1.global.scrollX,y:_1.global.scrollY};
},connect:function(_4,_5,fn){
if(_5.toLowerCase()=="dblclick"||_5.toLowerCase()=="ondblclick"){
if(_1.global["Mojo"]){
return this.connect(_4,Mojo.Event.tap,fn);
}
}
return this.inherited(arguments);
}});
});
