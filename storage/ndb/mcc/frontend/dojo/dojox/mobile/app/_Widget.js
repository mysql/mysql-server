//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/_WidgetBase"],function(_1,_2,_3){
_2.provide("dojox.mobile.app._Widget");
_2.experimental("dojox.mobile.app._Widget");
_2.require("dijit._WidgetBase");
_2.declare("dojox.mobile.app._Widget",_1._WidgetBase,{getScroll:function(){
return {x:_2.global.scrollX,y:_2.global.scrollY};
},connect:function(_4,_5,fn){
if(_5.toLowerCase()=="dblclick"||_5.toLowerCase()=="ondblclick"){
if(_2.global["Mojo"]){
return this.connect(_4,Mojo.Event.tap,fn);
}
}
return this.inherited(arguments);
}});
});
