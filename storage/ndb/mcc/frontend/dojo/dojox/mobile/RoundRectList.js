//>>built
define("dojox/mobile/RoundRectList",["dojo/_base/array","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/window","dojo/dom-construct","dojo/dom-attr","dijit/_Contained","dijit/_Container","dijit/_WidgetBase"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dojox.mobile.RoundRectList",[_a,_9,_8],{transition:"slide",iconBase:"",iconPos:"",select:"",stateful:false,syncWithViews:false,editable:false,tag:"ul",editableMixinClass:"dojox/mobile/_EditableListMixin",baseClass:"mblRoundRectList",filterBoxClass:"mblFilteredRoundRectListSearchBox",buildRendering:function(){
this.domNode=this.srcNodeRef||_6.create(this.tag);
if(this.select){
_7.set(this.domNode,"role","listbox");
if(this.select==="multiple"){
_7.set(this.domNode,"aria-multiselectable","true");
}
}
this.inherited(arguments);
},postCreate:function(){
if(this.editable){
require([this.editableMixinClass],_4.hitch(this,function(_b){
_2.safeMixin(this,new _b());
}));
}
this.connect(this.domNode,"onselectstart",_3.stop);
if(this.syncWithViews){
var f=function(_c,_d,_e,_f,_10,_11){
var _12=_1.filter(this.getChildren(),function(w){
return w.moveTo==="#"+_c.id||w.moveTo===_c.id;
})[0];
if(_12){
_12.set("selected",true);
}
};
this.subscribe("/dojox/mobile/afterTransitionIn",f);
this.subscribe("/dojox/mobile/startView",f);
}
},resize:function(){
_1.forEach(this.getChildren(),function(_13){
if(_13.resize){
_13.resize();
}
});
},onCheckStateChanged:function(){
},_setStatefulAttr:function(_14){
this._set("stateful",_14);
this.selectOne=_14;
_1.forEach(this.getChildren(),function(_15){
_15.setArrow&&_15.setArrow();
});
},deselectItem:function(_16){
_16.set("selected",false);
},deselectAll:function(){
_1.forEach(this.getChildren(),function(_17){
_17.set("selected",false);
});
},selectItem:function(_18){
_18.set("selected",true);
}});
});
