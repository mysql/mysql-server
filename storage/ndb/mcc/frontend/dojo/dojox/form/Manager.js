//>>built
define("dojox/form/Manager",["dijit/_Widget","dijit/_AttachMixin","dijit/_WidgetsInTemplateMixin","./manager/_Mixin","./manager/_NodeMixin","./manager/_FormMixin","./manager/_ValueMixin","./manager/_EnableMixin","./manager/_DisplayMixin","./manager/_ClassMixin","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _b("dojox.form.Manager",[_1,_3,_2,_4,_5,_6,_7,_8,_9,_a],{searchContainerNode:true,buildRendering:function(){
if(!this.containerNode){
this.containerNode=this.srcNodeRef;
}
this.inherited(arguments);
}});
});
