//>>built
define("dojox/mvc/Templated",["dojo/_base/declare","dojo/_base/lang","dijit/_WidgetBase","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","./at"],function(_1,_2,_3,_4,_5){
return _1("dojox.mvc.Templated",[_3,_4,_5],{bindings:null,startup:function(){
var _6=_2.isFunction(this.bindings)&&this.bindings.call(this)||this.bindings;
for(var s in _6){
var w=this[s],_7=_6[s];
if(w){
for(var _8 in _7){
w.set(_8,_7[_8]);
}
}else{
console.warn("Widget with the following attach point was not found: "+s);
}
}
this.inherited(arguments);
}});
});
