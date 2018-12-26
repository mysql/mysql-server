//>>built
define("dojox/app/scene",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/array","dojo/_base/Deferred","dojo/_base/lang","dojo/_base/sniff","dojo/dom-style","dojo/dom-geometry","dojo/dom-class","dojo/dom-construct","dojo/dom-attr","dojo/query","dijit","dojox","dijit/_WidgetBase","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dojox/css3/transit","./animation","./model","./view","./bind"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17){
var _18=function(_19,mb){
var cs=_8.getComputedStyle(_19);
var me=_9.getMarginExtents(_19,cs);
var pb=_9.getPadBorderExtents(_19,cs);
return {l:_8.toPixelValue(_19,cs.paddingLeft),t:_8.toPixelValue(_19,cs.paddingTop),w:mb.w-(me.w+pb.w),h:mb.h-(me.h+pb.h)};
};
var _1a=function(_1b){
return _1b.substring(0,1).toUpperCase()+_1b.substring(1);
};
var _1c=function(_1d,dim){
var _1e=_1d.resize?_1d.resize(dim):_9.setMarginBox(_1d.domNode,dim);
if(_1e){
_1.mixin(_1d,_1e);
}else{
_1.mixin(_1d,_9.getMarginBox(_1d.domNode));
_1.mixin(_1d,dim);
}
};
return _2("dojox.app.scene",[_e._WidgetBase,_e._TemplatedMixin,_e._WidgetsInTemplateMixin],{isContainer:true,widgetsInTemplate:true,defaultView:"default",selectedChild:null,baseClass:"scene mblView",isFullScreen:false,defaultViewType:_16,getParent:function(){
return null;
},constructor:function(_1f,_20){
this.children={};
if(_1f.parent){
this.parent=_1f.parent;
}
if(_1f.app){
this.app=_1f.app;
}
},buildRendering:function(){
this.inherited(arguments);
_8.set(this.domNode,{width:"100%","height":"100%"});
_a.add(this.domNode,"dijitContainer");
},splitChildRef:function(_21){
var id=_21.split(",");
if(id.length>0){
var to=id.shift();
}else{
console.warn("invalid child id passed to splitChildRef(): ",_21);
}
return {id:to||this.defaultView,next:id.join(",")};
},loadChild:function(_22,_23){
if(!_22){
var _24=this.defaultView?this.defaultView.split(","):"default";
_22=_24.shift();
_23=_24.join(",");
}
var cid=this.id+"_"+_22;
if(this.children[cid]){
return this.children[cid];
}
if(this.views&&this.views[_22]){
var _25=this.views[_22];
if(!_25.dependencies){
_25.dependencies=[];
}
var _26=_25.template?_25.dependencies.concat(["dojo/text!app/"+_25.template]):_25.dependencies.concat([]);
var def=new _5();
if(_26.length>0){
require(_26,function(){
def.resolve.call(def,arguments);
});
}else{
def.resolve(true);
}
var _27=new _5();
var _28=this;
_5.when(def,function(){
var _29;
if(_25.type){
_29=_1.getObject(_25.type);
}else{
if(_28.defaultViewType){
_29=_28.defaultViewType;
}else{
throw Error("Unable to find appropriate ctor for the base child class");
}
}
var _2a=_1.mixin({},_25,{id:_28.id+"_"+_22,templateString:_25.template?arguments[0][arguments[0].length-1]:"<div></div>",parent:_28,app:_28.app});
if(_23){
_2a.defaultView=_23;
}
var _2b=new _29(_2a);
if(!_2b.loadedModels){
_2b.loadedModels=_15(_25.models,_28.loadedModels);
_17([_2b],_2b.loadedModels);
}
var _2c=_28.addChild(_2b);
_3.publish("/app/loadchild",[_2b]);
var _2d;
_23=_23.split(",");
if((_23[0].length>0)&&(_23.length>1)){
_2d=_2b.loadChild(_23[0],_23[1]);
}else{
if(_23[0].length>0){
_2d=_2b.loadChild(_23[0],"");
}
}
_1.when(_2d,function(){
_27.resolve(_2c);
});
});
return _27;
}
throw Error("Child '"+_22+"' not found.");
},resize:function(_2e,_2f){
var _30=this.domNode;
if(_2e){
_9.setMarginBox(_30,_2e);
if(_2e.t){
_30.style.top=_2e.t+"px";
}
if(_2e.l){
_30.style.left=_2e.l+"px";
}
}
var mb=_2f||{};
_1.mixin(mb,_2e||{});
if(!("h" in mb)||!("w" in mb)){
mb=_1.mixin(_9.getMarginBox(_30),mb);
}
var cs=_8.getComputedStyle(_30);
var me=_9.getMarginExtents(_30,cs);
var be=_9.getBorderExtents(_30,cs);
var bb=(this._borderBox={w:mb.w-(me.w+be.w),h:mb.h-(me.h+be.h)});
var pe=_9.getPadExtents(_30,cs);
this._contentBox={l:_8.toPixelValue(_30,cs.paddingLeft),t:_8.toPixelValue(_30,cs.paddingTop),w:bb.w-pe.w,h:bb.h-pe.h};
this.layout();
},layout:function(){
var _31,_32,_33;
if(this.selectedChild&&this.selectedChild.isFullScreen){
console.warn("fullscreen sceen layout");
}else{
_32=_d("> [region]",this.domNode).map(function(_34){
var w=_e.getEnclosingWidget(_34);
if(w){
return w;
}
return {domNode:_34,region:_c.get(_34,"region")};
});
if(this.selectedChild){
_32=_4.filter(_32,function(c){
if(c.region=="center"&&this.selectedChild&&this.selectedChild.domNode!==c.domNode){
_8.set(c.domNode,"zIndex",25);
_8.set(c.domNode,"display","none");
return false;
}else{
if(c.region!="center"){
_8.set(c.domNode,"display","");
_8.set(c.domNode,"zIndex",100);
}
}
return c.domNode&&c.region;
},this);
}else{
_4.forEach(_32,function(c){
if(c&&c.domNode&&c.region=="center"){
_8.set(c.domNode,"zIndex",25);
_8.set(c.domNode,"display","none");
}
});
}
}
if(this._contentBox){
this.layoutChildren(this.domNode,this._contentBox,_32);
}
_4.forEach(this.getChildren(),function(_35){
if(!_35._started&&_35.startup){
_35.startup();
}
});
},layoutChildren:function(_36,dim,_37,_38,_39){
dim=_1.mixin({},dim);
_a.add(_36,"dijitLayoutContainer");
_37=_4.filter(_37,function(_3a){
return _3a.region!="center"&&_3a.layoutAlign!="client";
}).concat(_4.filter(_37,function(_3b){
return _3b.region=="center"||_3b.layoutAlign=="client";
}));
_4.forEach(_37,function(_3c){
var elm=_3c.domNode,pos=(_3c.region||_3c.layoutAlign);
var _3d=elm.style;
_3d.left=dim.l+"px";
_3d.top=dim.t+"px";
_3d.position="absolute";
_a.add(elm,"dijitAlign"+_1a(pos));
var _3e={};
if(_38&&_38==_3c.id){
_3e[_3c.region=="top"||_3c.region=="bottom"?"h":"w"]=_39;
}
if(pos=="top"||pos=="bottom"){
_3e.w=dim.w;
_1c(_3c,_3e);
dim.h-=_3c.h;
if(pos=="top"){
dim.t+=_3c.h;
}else{
_3d.top=dim.t+dim.h+"px";
}
}else{
if(pos=="left"||pos=="right"){
_3e.h=dim.h;
_1c(_3c,_3e);
dim.w-=_3c.w;
if(pos=="left"){
dim.l+=_3c.w;
}else{
_3d.left=dim.l+dim.w+"px";
}
}else{
if(pos=="client"||pos=="center"){
_1c(_3c,dim);
}
}
}
});
},getChildren:function(){
return this._supportingWidgets;
},startup:function(){
if(this._started){
return;
}
this._started=true;
var _3f=this.defaultView?this.defaultView.split(","):"default";
var _40,_41;
_40=_3f.shift();
_41=_3f.join(",");
if(this.views[this.defaultView]&&this.views[this.defaultView]["defaultView"]){
_41=this.views[this.defaultView]["defaultView"];
}
if(this.models&&!this.loadedModels){
this.loadedModels=_15(this.models);
_17(this.getChildren(),this.loadedModels);
}
var cid=this.id+"_"+_40;
if(this.children[cid]){
var _42=this.children[cid];
this.set("selectedChild",_42);
var _43=this.getParent&&this.getParent();
if(!(_43&&_43.isLayoutContainer)){
this.resize();
this.connect(_7("ie")?this.domNode:_1.global,"onresize",function(){
this.resize();
});
}
_4.forEach(this.getChildren(),function(_44){
_44.startup();
});
if(this._startView&&(this._startView!=this.defaultView)){
this.transition(this._startView,{});
}
}
},addChild:function(_45){
_a.add(_45.domNode,this.baseClass+"_child");
_45.region="center";
_c.set(_45.domNode,"region","center");
this._supportingWidgets.push(_45);
_b.place(_45.domNode,this.domNode);
this.children[_45.id]=_45;
return _45;
},removeChild:function(_46){
if(_46){
var _47=_46.domNode;
if(_47&&_47.parentNode){
_47.parentNode.removeChild(_47);
}
return _46;
}
},_setSelectedChildAttr:function(_48,_49){
if(_48!==this.selectedChild){
return _5.when(_48,_6.hitch(this,function(_4a){
if(this.selectedChild){
if(this.selectedChild.deactivate){
this.selectedChild.deactivate();
}
_8.set(this.selectedChild.domNode,"zIndex",25);
}
this.selectedChild=_4a;
_8.set(_4a.domNode,"display","");
_8.set(_4a.domNode,"zIndex",50);
this.selectedChild=_4a;
if(this._started){
if(_4a.startup&&!_4a._started){
_4a.startup();
}else{
if(_4a.activate){
_4a.activate();
}
}
}
this.layout();
}));
}
},transition:function(_4b,_4c){
var _4d,_4e,_4f,_50=this.selectedChild;
if(_4b){
var _51=_4b.split(",");
_4d=_51.shift();
_4e=_51.join(",");
}else{
_4d=this.defaultView;
if(this.views[this.defaultView]&&this.views[this.defaultView]["defaultView"]){
_4e=this.views[this.defaultView]["defaultView"];
}
}
_4f=this.loadChild(_4d,_4e);
if(!_50){
return this.set("selectedChild",_4f);
}
var _52=new _5();
_5.when(_4f,_6.hitch(this,function(_53){
var _54;
if(_53!==_50){
var _55=_14.getWaitingList([_53.domNode,_50.domNode]);
var _56={};
_56[_50.domNode.id]=_14.playing[_50.domNode.id]=new _5();
_56[_53.domNode.id]=_14.playing[_50.domNode.id]=new _5();
_5.when(_55,_1.hitch(this,function(){
this.set("selectedChild",_53);
_3.publish("/app/transition",[_53,_4d]);
_13(_50.domNode,_53.domNode,_1.mixin({},_4c,{transition:this.defaultTransition||"none",transitionDefs:_56})).then(_6.hitch(this,function(){
if(_4e&&_53.transition){
_54=_53.transition(_4e,_4c);
}
_5.when(_54,function(){
_52.resolve();
});
}));
}));
return;
}
if(_4e&&_53.transition){
_54=_53.transition(_4e,_4c);
}
_5.when(_54,function(){
_52.resolve();
});
}));
return _52;
},toString:function(){
return this.id;
},activate:function(){
},deactive:function(){
}});
});
