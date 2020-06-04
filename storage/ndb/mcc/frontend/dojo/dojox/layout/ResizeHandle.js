//>>built
define("dojox/layout/ResizeHandle",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/connect","dojo/_base/array","dojo/_base/event","dojo/_base/fx","dojo/_base/window","dojo/fx","dojo/dom","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/declare","dojo/touch","dijit/_base/manager","dijit/_Widget","dijit/_TemplatedMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11){
_1.experimental("dojox.layout.ResizeHandle");
var _12=_d("dojox.layout._ResizeHelper",_10,{show:function(){
_c.set(this.domNode,"display","");
},hide:function(){
_c.set(this.domNode,"display","none");
},resize:function(dim){
_b.setMarginBox(this.domNode,dim);
}});
var _13=_d("dojox.layout.ResizeHandle",[_10,_11],{targetId:"",targetContainer:null,resizeAxis:"xy",activeResize:false,activeResizeClass:"dojoxResizeHandleClone",animateSizing:true,animateMethod:"chain",animateDuration:225,minHeight:100,minWidth:100,constrainMax:false,maxHeight:0,maxWidth:0,fixedAspect:false,intermediateChanges:false,startTopic:"/dojo/resize/start",endTopic:"/dojo/resize/stop",templateString:"<div dojoAttachPoint=\"resizeHandle\" class=\"dojoxResizeHandle\"><div></div></div>",postCreate:function(){
this.connect(this.resizeHandle,_e.press,"_beginSizing");
if(!this.activeResize){
this._resizeHelper=_f.byId("dojoxGlobalResizeHelper");
if(!this._resizeHelper){
this._resizeHelper=new _12({id:"dojoxGlobalResizeHelper"}).placeAt(_7.body());
_a.add(this._resizeHelper.domNode,this.activeResizeClass);
}
}else{
this.animateSizing=false;
}
if(!this.minSize){
this.minSize={w:this.minWidth,h:this.minHeight};
}
if(this.constrainMax){
this.maxSize={w:this.maxWidth,h:this.maxHeight};
}
this._resizeX=this._resizeY=false;
var _14=_2.partial(_a.add,this.resizeHandle);
switch(this.resizeAxis.toLowerCase()){
case "xy":
this._resizeX=this._resizeY=true;
_14("dojoxResizeNW");
break;
case "x":
this._resizeX=true;
_14("dojoxResizeW");
break;
case "y":
this._resizeY=true;
_14("dojoxResizeN");
break;
}
},_beginSizing:function(e){
if(this._isSizing){
return;
}
_3.publish(this.startTopic,[this]);
this.targetWidget=_f.byId(this.targetId);
this.targetDomNode=this.targetWidget?this.targetWidget.domNode:_9.byId(this.targetId);
if(this.targetContainer){
this.targetDomNode=this.targetContainer;
}
if(!this.targetDomNode){
return;
}
if(!this.activeResize){
var c=_b.position(this.targetDomNode,true);
this._resizeHelper.resize({l:c.x,t:c.y,w:c.w,h:c.h});
this._resizeHelper.show();
if(!this.isLeftToRight()){
this._resizeHelper.startPosition={l:c.x,t:c.y};
}
}
this._isSizing=true;
this.startPoint={x:e.clientX,y:e.clientY};
var _15=_c.getComputedStyle(this.targetDomNode),_16=_b.boxModel==="border-model",_17=_16?{w:0,h:0}:_b.getPadBorderExtents(this.targetDomNode,_15),_18=_b.getMarginExtents(this.targetDomNode,_15);
this.startSize={w:_c.get(this.targetDomNode,"width",_15),h:_c.get(this.targetDomNode,"height",_15),pbw:_17.w,pbh:_17.h,mw:_18.w,mh:_18.h};
if(!this.isLeftToRight()&&_c.get(this.targetDomNode,"position")=="absolute"){
var p=_b.position(this.targetDomNode,true);
this.startPosition={l:p.x,t:p.y};
}
this._pconnects=[_3.connect(_7.doc,_e.move,this,"_updateSizing"),_3.connect(_7.doc,_e.release,this,"_endSizing")];
_5.stop(e);
},_updateSizing:function(e){
if(this.activeResize){
this._changeSizing(e);
}else{
var tmp=this._getNewCoords(e,"border",this._resizeHelper.startPosition);
if(tmp===false){
return;
}
this._resizeHelper.resize(tmp);
}
e.preventDefault();
},_getNewCoords:function(e,box,_19){
try{
if(!e.clientX||!e.clientY){
return false;
}
}
catch(err){
return false;
}
this._activeResizeLastEvent=e;
var dx=(this.isLeftToRight()?1:-1)*(this.startPoint.x-e.clientX),dy=this.startPoint.y-e.clientY,_1a=this.startSize.w-(this._resizeX?dx:0),_1b=this.startSize.h-(this._resizeY?dy:0),r=this._checkConstraints(_1a,_1b);
_19=(_19||this.startPosition);
if(_19&&this._resizeX){
r.l=_19.l+dx;
if(r.w!=_1a){
r.l+=(_1a-r.w);
}
r.t=_19.t;
}
switch(box){
case "margin":
r.w+=this.startSize.mw;
r.h+=this.startSize.mh;
case "border":
r.w+=this.startSize.pbw;
r.h+=this.startSize.pbh;
break;
}
return r;
},_checkConstraints:function(_1c,_1d){
if(this.minSize){
var tm=this.minSize;
if(_1c<tm.w){
_1c=tm.w;
}
if(_1d<tm.h){
_1d=tm.h;
}
}
if(this.constrainMax&&this.maxSize){
var ms=this.maxSize;
if(_1c>ms.w){
_1c=ms.w;
}
if(_1d>ms.h){
_1d=ms.h;
}
}
if(this.fixedAspect){
var w=this.startSize.w,h=this.startSize.h,_1e=w*_1d-h*_1c;
if(_1e<0){
_1c=_1d*w/h;
}else{
if(_1e>0){
_1d=_1c*h/w;
}
}
}
return {w:_1c,h:_1d};
},_changeSizing:function(e){
var _1f=this.targetWidget&&_2.isFunction(this.targetWidget.resize),tmp=this._getNewCoords(e,_1f&&"margin");
if(tmp===false){
return;
}
if(_1f){
this.targetWidget.resize(tmp);
}else{
if(this.animateSizing){
var _20=_8[this.animateMethod]([_6.animateProperty({node:this.targetDomNode,properties:{width:{start:this.startSize.w,end:tmp.w}},duration:this.animateDuration}),_6.animateProperty({node:this.targetDomNode,properties:{height:{start:this.startSize.h,end:tmp.h}},duration:this.animateDuration})]);
_20.play();
}else{
_c.set(this.targetDomNode,{width:tmp.w+"px",height:tmp.h+"px"});
}
}
if(this.intermediateChanges){
this.onResize(e);
}
},_endSizing:function(e){
_4.forEach(this._pconnects,_3.disconnect);
var pub=_2.partial(_3.publish,this.endTopic,[this]);
if(!this.activeResize){
this._resizeHelper.hide();
this._changeSizing(e);
setTimeout(pub,this.animateDuration+15);
}else{
pub();
}
this._isSizing=false;
this.onResize(e);
},onResize:function(e){
}});
return _13;
});
