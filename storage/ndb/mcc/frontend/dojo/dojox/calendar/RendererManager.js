//>>built
define("dojox/calendar/RendererManager",["dojo/_base/declare","dojo/_base/array","dojo/_base/html","dojo/_base/lang","dojo/dom-class","dojo/dom-style","dojo/Stateful","dojo/Evented"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _1("dojox.calendar.RendererManager",[_7,_8],{owner:null,rendererPool:null,rendererList:null,itemToRenderer:null,constructor:function(_a){
_a=_a||{};
this.rendererPool=[];
this.rendererList=[];
this.itemToRenderer={};
},destroy:function(){
while(this.rendererList.length>0){
this.destroyRenderer(this.rendererList.pop());
}
for(var _b in this._rendererPool){
var _c=this._rendererPool[_b];
if(_c){
while(_c.length>0){
this.destroyRenderer(_c.pop());
}
}
}
},recycleItemRenderers:function(_d){
while(this.rendererList.length>0){
var ir=this.rendererList.pop();
this.recycleRenderer(ir,_d);
}
this.itemToRenderer={};
},getRenderers:function(_e){
if(_e==null||_e.id==null){
return null;
}
var _f=this.itemToRenderer[_e.id];
return _f==null?null:_f.concat();
},createRenderer:function(_10,_11,_12,_13){
if(_10!=null&&_11!=null&&_12!=null){
var res=null,_14=null;
var _15=this.rendererPool[_11];
if(_15!=null){
res=_15.shift();
}
if(res==null){
_14=new _12;
res={renderer:_14,container:_14.domNode,kind:_11};
this.emit("rendererCreated",{renderer:res,source:this.owner,item:_10});
}else{
_14=res.renderer;
this.emit("rendererReused",{renderer:_14,source:this.owner,item:_10});
}
_14.owner=this.owner;
_14.set("rendererKind",_11);
_14.set("item",_10);
var _16=this.itemToRenderer[_10.id];
if(_16==null){
this.itemToRenderer[_10.id]=_16=[];
}
_16.push(res);
this.rendererList.push(res);
return res;
}
return null;
},recycleRenderer:function(_17,_18){
this.emit("rendererRecycled",{renderer:_17,source:this.owner});
var _19=this.rendererPool[_17.kind];
if(_19==null){
this.rendererPool[_17.kind]=[_17];
}else{
_19.push(_17);
}
if(_18){
_17.container.parentNode.removeChild(_17.container);
}
_6.set(_17.container,"display","none");
_17.renderer.owner=null;
_17.renderer.set("item",null);
},destroyRenderer:function(_1a){
this.emit("rendererDestroyed",{renderer:_1a,source:this.owner});
var ir=_1a.renderer;
if(ir["destroy"]){
ir.destroy();
}
_3.destroy(_1a.container);
},destroyRenderersByKind:function(_1b){
var _1c=[];
for(var i=0;i<this.rendererList.length;i++){
var ir=this.rendererList[i];
if(ir.kind==_1b){
this.destroyRenderer(ir);
}else{
_1c.push(ir);
}
}
this.rendererList=_1c;
var _1d=this.rendererPool[_1b];
if(_1d){
while(_1d.length>0){
this.destroyRenderer(_1d.pop());
}
}
}});
});
