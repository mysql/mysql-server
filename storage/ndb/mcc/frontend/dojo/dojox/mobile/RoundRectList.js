//>>built
define("dojox/mobile/RoundRectList",["dojo/_base/array","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/window","dojo/dom-construct","dijit/_Contained","dijit/_Container","dijit/_WidgetBase"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.mobile.RoundRectList",[_9,_8,_7],{transition:"slide",iconBase:"",iconPos:"",select:"",stateful:false,syncWithViews:false,editable:false,tag:"ul",editableMixinClass:"dojox/mobile/_EditableListMixin",baseClass:"mblRoundRectList",buildRendering:function(){
this.domNode=this.srcNodeRef||_6.create(this.tag);
this.inherited(arguments);
},postCreate:function(){
if(this.editable){
require([this.editableMixinClass],_4.hitch(this,function(_a){
_2.safeMixin(this,new _a());
}));
}
this.connect(this.domNode,"onselectstart",_3.stop);
if(this.syncWithViews){
var f=function(_b,_c,_d,_e,_f,_10){
var _11=_1.filter(this.getChildren(),function(w){
return w.moveTo==="#"+_b.id||w.moveTo===_b.id;
})[0];
if(_11){
_11.set("selected",true);
}
};
this.subscribe("/dojox/mobile/afterTransitionIn",f);
this.subscribe("/dojox/mobile/startView",f);
}
},resize:function(){
_1.forEach(this.getChildren(),function(_12){
if(_12.resize){
_12.resize();
}
});
},onCheckStateChanged:function(){
},_setStatefulAttr:function(_13){
this._set("stateful",_13);
this.selectOne=_13;
_1.forEach(this.getChildren(),function(_14){
_14.setArrow&&_14.setArrow();
});
},deselectItem:function(_15){
_15.set("selected",false);
},deselectAll:function(){
_1.forEach(this.getChildren(),function(_16){
_16.set("selected",false);
});
},selectItem:function(_17){
_17.set("selected",true);
}});
});
