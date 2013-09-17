//>>built
define("dojox/form/Manager",["dijit/_Widget","dijit/_TemplatedMixin","./manager/_Mixin","./manager/_NodeMixin","./manager/_FormMixin","./manager/_ValueMixin","./manager/_EnableMixin","./manager/_DisplayMixin","./manager/_ClassMixin","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _a("dojox.form.Manager",[_1,_3,_4,_5,_6,_7,_8,_9],{buildRendering:function(){
var _b=(this.domNode=this.srcNodeRef);
if(!this.containerNode){
this.containerNode=_b;
}
this.inherited(arguments);
this._attachPoints=[];
this._attachEvents=[];
_2.prototype._attachTemplateNodes.call(this,_b,function(n,p){
return n.getAttribute(p);
});
},destroyRendering:function(_c){
if(!this.__ctm){
this.__ctm=true;
_2.prototype.destroyRendering.apply(this,arguments);
delete this.__ctm;
this.inherited(arguments);
}
}});
});
