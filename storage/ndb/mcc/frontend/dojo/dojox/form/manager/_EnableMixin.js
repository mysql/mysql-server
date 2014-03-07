//>>built
define("dojox/form/manager/_EnableMixin",["dojo/_base/lang","dojo/_base/kernel","dojo/dom-attr","./_Mixin","dojo/_base/declare"],function(_1,_2,_3,_4,_5){
var fm=_1.getObject("dojox.form.manager",true),aa=fm.actionAdapter,ia=fm.inspectorAdapter;
return _5("dojox.form.manager._EnableMixin",null,{gatherEnableState:function(_6){
var _7=this.inspectFormWidgets(ia(function(_8,_9){
return !_9.get("disabled");
}),_6);
if(this.inspectFormNodes){
_1.mixin(_7,this.inspectFormNodes(ia(function(_a,_b){
return !_3.get(_b,"disabled");
}),_6));
}
return _7;
},enable:function(_c,_d){
if(arguments.length<2||_d===undefined){
_d=true;
}
this.inspectFormWidgets(aa(function(_e,_f,_10){
_f.set("disabled",!_10);
}),_c,_d);
if(this.inspectFormNodes){
this.inspectFormNodes(aa(function(_11,_12,_13){
_3.set(_12,"disabled",!_13);
}),_c,_d);
}
return this;
},disable:function(_14){
var _15=this.gatherEnableState();
this.enable(_14,false);
return _15;
}});
});
