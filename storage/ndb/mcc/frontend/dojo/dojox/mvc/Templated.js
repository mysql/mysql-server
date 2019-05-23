//>>built
define("dojox/mvc/Templated",["dojo/_base/declare","dojo/_base/lang","dijit/_WidgetBase","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dojox/mvc/at"],function(_1,_2,_3,_4,_5,at){
return _1("dojox.mvc.Templated",[_3,_4,_5],{bindings:null,startup:function(){
this.inherited(arguments);
for(var s in this.bindings){
var w=this[s],_6=this.bindings[s];
if(w){
for(var _7 in _6){
w.set(_7,_6[_7]);
}
}
}
}});
});
