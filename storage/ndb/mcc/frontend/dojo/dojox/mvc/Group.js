//>>built
define("dojox/mvc/Group",["dojo/_base/declare","dijit/_WidgetBase","dojo/_base/lang"],function(_1,_2,_3){
return _1("dojox.mvc.Group",_2,{target:null,startup:function(){
var _4=null;
if(_3.isFunction(this.getParent)){
if(this.getParent()&&this.getParent().removeRepeatNode){
this.select=this.getParent().select;
this.onCheckStateChanged=this.getParent().onCheckStateChanged;
}
}
this.inherited(arguments);
},_setTargetAttr:function(_5){
this._set("target",_5);
if(this.binding!=_5){
this.set("ref",_5);
}
}});
});
