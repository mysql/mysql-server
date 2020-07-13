//>>built
define("dojox/mobile/IconContainer",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-construct","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./IconItem","./Heading","./View"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _2("dojox.mobile.IconContainer",[_8,_7,_6],{defaultIcon:"",transition:"below",pressedIconOpacity:0.4,iconBase:"",iconPos:"",back:"Home",label:"My Application",single:false,editable:false,tag:"ul",baseClass:"mblIconContainer",editableMixinClass:"dojox/mobile/_EditableIconMixin",iconItemPaneContainerClass:"dojox/mobile/Container",iconItemPaneContainerProps:null,iconItemPaneClass:"dojox/mobile/_IconItemPane",iconItemPaneProps:null,buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_5.create(this.tag);
this._terminator=_5.create(this.tag==="ul"?"li":"div",{className:"mblIconItemTerminator"},this.domNode);
this.inherited(arguments);
},postCreate:function(){
if(this.editable&&!this.startEdit){
require([this.editableMixinClass],_3.hitch(this,function(_c){
_2.safeMixin(this,new _c());
this.set("editable",this.editable);
}));
}
},startup:function(){
if(this._started){
return;
}
require([this.iconItemPaneContainerClass],_3.hitch(this,function(_d){
this.paneContainerWidget=new _d(this.iconItemPaneContainerProps);
if(this.transition==="below"){
_5.place(this.paneContainerWidget.domNode,this.domNode,"after");
}else{
var _e=this.appView=new _b({id:this.id+"_mblApplView"});
var _f=this;
_e.onAfterTransitionIn=function(_10,dir,_11,_12,_13){
_f._opening._open_1();
};
_e.domNode.style.visibility="hidden";
var _14=_e._heading=new _a({back:this._cv?this._cv(this.back):this.back,label:this._cv?this._cv(this.label):this.label,moveTo:this.domNode.parentNode.id,transition:this.transition=="zoomIn"?"zoomOut":this.transition});
_e.addChild(_14);
_e.addChild(this.paneContainerWidget);
var _15;
for(var w=this.getParent();w;w=w.getParent()){
if(w instanceof _b){
_15=w.domNode.parentNode;
break;
}
}
if(!_15){
_15=_4.body();
}
_15.appendChild(_e.domNode);
_e.startup();
}
}));
this.inherited(arguments);
},closeAll:function(){
_1.forEach(this.getChildren(),function(w){
w.close(true);
},this);
},addChild:function(_16,_17){
this.inherited(arguments);
if(this._started&&_16.paneWidget&&!_16.paneWidget.getParent()){
this.paneContainerWidget.addChild(_16.paneWidget,_17);
}
this.domNode.appendChild(this._terminator);
},removeChild:function(_18){
var _19=(typeof _18=="number")?_18:_18.getIndexInParent();
this.paneContainerWidget.removeChild(_19);
this.inherited(arguments);
},_setLabelAttr:function(_1a){
if(!this.appView){
return;
}
this.label=_1a;
var s=this._cv?this._cv(_1a):_1a;
this.appView._heading.set("label",s);
}});
});
