//>>built
define("dojox/form/manager/_DisplayMixin",["dojo/_base/kernel","dojo/dom-style","dojo/_base/declare"],function(_1,_2,_3){
return _3("dojox.form.manager._DisplayMixin",null,{gatherDisplayState:function(_4){
var _5=this.inspectAttachedPoints(function(_6,_7){
return _2.get(_7,"display")!="none";
},_4);
return _5;
},show:function(_8,_9){
if(arguments.length<2){
_9=true;
}
this.inspectAttachedPoints(function(_a,_b,_c){
_2.set(_b,"display",_c?"":"none");
},_8,_9);
return this;
},hide:function(_d){
return this.show(_d,false);
}});
});
