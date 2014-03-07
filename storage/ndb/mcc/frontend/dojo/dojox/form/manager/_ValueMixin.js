//>>built
define("dojox/form/manager/_ValueMixin",["dojo/_base/lang","dojo/_base/kernel","dojo/_base/declare"],function(_1,_2,_3){
return _3("dojox.form.manager._ValueMixin",null,{elementValue:function(_4,_5){
if(_4 in this.formWidgets){
return this.formWidgetValue(_4,_5);
}
if(this.formNodes&&_4 in this.formNodes){
return this.formNodeValue(_4,_5);
}
return this.formPointValue(_4,_5);
},gatherFormValues:function(_6){
var _7=this.inspectFormWidgets(function(_8){
return this.formWidgetValue(_8);
},_6);
if(this.inspectFormNodes){
_1.mixin(_7,this.inspectFormNodes(function(_9){
return this.formNodeValue(_9);
},_6));
}
_1.mixin(_7,this.inspectAttachedPoints(function(_a){
return this.formPointValue(_a);
},_6));
return _7;
},setFormValues:function(_b){
if(_b){
this.inspectFormWidgets(function(_c,_d,_e){
this.formWidgetValue(_c,_e);
},_b);
if(this.inspectFormNodes){
this.inspectFormNodes(function(_f,_10,_11){
this.formNodeValue(_f,_11);
},_b);
}
this.inspectAttachedPoints(function(_12,_13,_14){
this.formPointValue(_12,_14);
},_b);
}
return this;
}});
});
