//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/dnd/Source,dojo/dnd/Manager,dojox/layout/dnd/Avatar"],function(_1,_2,_3){
_2.provide("dojox.layout.dnd.PlottedDnd");
_2.require("dojo.dnd.Source");
_2.require("dojo.dnd.Manager");
_2.require("dojox.layout.dnd.Avatar");
_2.declare("dojox.layout.dnd.PlottedDnd",[_2.dnd.Source],{GC_OFFSET_X:_2.dnd.manager().OFFSET_X,GC_OFFSET_Y:_2.dnd.manager().OFFSET_Y,constructor:function(_4,_5){
this.childBoxes=null;
this.dropIndicator=new _3.layout.dnd.DropIndicator("dndDropIndicator","div");
this.withHandles=_5.withHandles;
this.handleClasses=_5.handleClasses;
this.opacity=_5.opacity;
this.allowAutoScroll=_5.allowAutoScroll;
this.dom=_5.dom;
this.singular=true;
this.skipForm=true;
this._over=false;
this.defaultHandleClass="GcDndHandle";
this.isDropped=false;
this._timer=null;
this.isOffset=(_5.isOffset)?true:false;
this.offsetDrag=(_5.offsetDrag)?_5.offsetDrag:{x:0,y:0};
this.hideSource=_5.hideSource?_5.hideSource:true;
this._drop=this.dropIndicator.create();
},_calculateCoords:function(_6){
_2.forEach(this.node.childNodes,function(_7){
var c=_2.coords(_7,true);
_7.coords={xy:c,w:_7.offsetWidth/2,h:_7.offsetHeight/2,mw:c.w};
if(_6){
_7.coords.mh=c.h;
}
},this);
},_legalMouseDown:function(e){
if(!this.withHandles){
return true;
}
for(var _8=(e.target);_8&&_8!=this.node;_8=_8.parentNode){
if(_2.hasClass(_8,this.defaultHandleClass)){
return true;
}
}
return false;
},setDndItemSelectable:function(_9,_a){
for(var _b=_9;_b&&_9!=this.node;_b=_b.parentNode){
if(_2.hasClass(_b,"dojoDndItem")){
_2.setSelectable(_b,_a);
return;
}
}
},getDraggedWidget:function(_c){
var _d=_c;
while(_d&&_d.nodeName.toLowerCase()!="body"&&!_2.hasClass(_d,"dojoDndItem")){
_d=_d.parentNode;
}
return (_d)?_1.byNode(_d):null;
},isAccepted:function(_e){
var _f=(_e)?_e.getAttribute("dndtype"):null;
return (_f&&_f in this.accept);
},onDndStart:function(_10,_11,_12){
this.firstIndicator=(_10==this);
this._calculateCoords(true);
var m=_2.dnd.manager();
if(_11[0].coords){
this._drop.style.height=_11[0].coords.mh+"px";
_2.style(m.avatar.node,"width",_11[0].coords.mw+"px");
}else{
this._drop.style.height=m.avatar.node.clientHeight+"px";
}
this.dndNodes=_11;
_3.layout.dnd.PlottedDnd.superclass.onDndStart.call(this,_10,_11,_12);
if(_10==this&&this.hideSource){
_2.forEach(_11,function(n){
_2.style(n,"display","none");
});
}
},onDndCancel:function(){
var m=_2.dnd.manager();
if(m.source==this&&this.hideSource){
var _13=this.getSelectedNodes();
_2.forEach(_13,function(n){
_2.style(n,"display","");
});
}
_3.layout.dnd.PlottedDnd.superclass.onDndCancel.call(this);
this.deleteDashedZone();
},onDndDrop:function(_14,_15,_16,_17){
try{
if(!this.isAccepted(_15[0])){
this.onDndCancel();
}else{
if(_14==this&&this._over&&this.dropObject){
this.current=this.dropObject.c;
}
_3.layout.dnd.PlottedDnd.superclass.onDndDrop.call(this,_14,_15,_16,_17);
this._calculateCoords(true);
}
}
catch(e){
console.warn(e);
}
},onMouseDown:function(e){
if(this.current==null){
this.selection={};
}else{
if(this.current==this.anchor){
this.anchor=null;
}
}
if(this.current!==null){
var c=_2.coords(this.current,true);
this.current.coords={xy:c,w:this.current.offsetWidth/2,h:this.current.offsetHeight/2,mh:c.h,mw:c.w};
this._drop.style.height=this.current.coords.mh+"px";
if(this.isOffset){
if(this.offsetDrag.x==0&&this.offsetDrag.y==0){
var _18=true;
var _19=_2.coords(this._getChildByEvent(e));
this.offsetDrag.x=_19.x-e.pageX;
this.offsetDrag.y=_19.y-e.clientY;
}
if(this.offsetDrag.y<16&&this.current!=null){
this.offsetDrag.y=this.GC_OFFSET_Y;
}
var m=_2.dnd.manager();
m.OFFSET_X=this.offsetDrag.x;
m.OFFSET_Y=this.offsetDrag.y;
if(_18){
this.offsetDrag.x=0;
this.offsetDrag.y=0;
}
}
}
if(_2.dnd.isFormElement(e)){
this.setDndItemSelectable(e.target,true);
}else{
this.containerSource=true;
var _1a=this.getDraggedWidget(e.target);
if(_1a&&_1a.dragRestriction){
}else{
_3.layout.dnd.PlottedDnd.superclass.onMouseDown.call(this,e);
}
}
},onMouseUp:function(e){
_3.layout.dnd.PlottedDnd.superclass.onMouseUp.call(this,e);
this.containerSource=false;
if(!_2.isIE&&this.mouseDown){
this.setDndItemSelectable(e.target,true);
}
var m=_2.dnd.manager();
m.OFFSET_X=this.GC_OFFSET_X;
m.OFFSET_Y=this.GC_OFFSET_Y;
},onMouseMove:function(e){
var m=_2.dnd.manager();
if(this.isDragging){
var _1b=false;
if(this.current!=null||(this.current==null&&!this.dropObject)){
if(this.isAccepted(m.nodes[0])||this.containerSource){
_1b=this.setIndicatorPosition(e);
}
}
if(this.current!=this.targetAnchor||_1b!=this.before){
this._markTargetAnchor(_1b);
m.canDrop(!this.current||m.source!=this||!(this.current.id in this.selection));
}
if(this.allowAutoScroll){
this._checkAutoScroll(e);
}
}else{
if(this.mouseDown&&this.isSource){
var _1c=this.getSelectedNodes();
if(_1c.length){
m.startDrag(this,_1c,this.copyState(_2.isCopyKey(e)));
}
}
if(this.allowAutoScroll){
this._stopAutoScroll();
}
}
},_markTargetAnchor:function(_1d){
if(this.current==this.targetAnchor&&this.before==_1d){
return;
}
this.targetAnchor=this.current;
this.targetBox=null;
this.before=_1d;
},_unmarkTargetAnchor:function(){
if(!this.targetAnchor){
return;
}
this.targetAnchor=null;
this.targetBox=null;
this.before=true;
},setIndicatorPosition:function(e){
var _1e=false;
if(this.current){
if(!this.current.coords||this.allowAutoScroll){
this.current.coords={xy:_2.coords(this.current,true),w:this.current.offsetWidth/2,h:this.current.offsetHeight/2};
}
_1e=this.horizontal?(e.pageX-this.current.coords.xy.x)<this.current.coords.w:(e.pageY-this.current.coords.xy.y)<this.current.coords.h;
this.insertDashedZone(_1e);
}else{
if(!this.dropObject){
this.insertDashedZone(false);
}
}
return _1e;
},onOverEvent:function(){
this._over=true;
_3.layout.dnd.PlottedDnd.superclass.onOverEvent.call(this);
if(this.isDragging){
var m=_2.dnd.manager();
if(!this.current&&!this.dropObject&&this.getSelectedNodes()[0]&&this.isAccepted(m.nodes[0])){
this.insertDashedZone(false);
}
}
},onOutEvent:function(){
this._over=false;
this.containerSource=false;
_3.layout.dnd.PlottedDnd.superclass.onOutEvent.call(this);
if(this.dropObject){
this.deleteDashedZone();
}
},deleteDashedZone:function(){
this._drop.style.display="none";
var _1f=this._drop.nextSibling;
while(_1f!=null){
_1f.coords.xy.y-=parseInt(this._drop.style.height);
_1f=_1f.nextSibling;
}
delete this.dropObject;
},insertDashedZone:function(_20){
if(this.dropObject){
if(_20==this.dropObject.b&&((this.current&&this.dropObject.c==this.current.id)||(!this.current&&!this.dropObject.c))){
return;
}else{
this.deleteDashedZone();
}
}
this.dropObject={n:this._drop,c:this.current?this.current.id:null,b:_20};
if(this.current){
_2.place(this._drop,this.current,_20?"before":"after");
if(!this.firstIndicator){
var _21=this._drop.nextSibling;
while(_21!=null){
_21.coords.xy.y+=parseInt(this._drop.style.height);
_21=_21.nextSibling;
}
}else{
this.firstIndicator=false;
}
}else{
this.node.appendChild(this._drop);
}
this._drop.style.display="";
},insertNodes:function(_22,_23,_24,_25){
if(this.dropObject){
_2.style(this.dropObject.n,"display","none");
_3.layout.dnd.PlottedDnd.superclass.insertNodes.call(this,true,_23,true,this.dropObject.n);
this.deleteDashedZone();
}else{
return _3.layout.dnd.PlottedDnd.superclass.insertNodes.call(this,_22,_23,_24,_25);
}
var _26=_1.byId(_23[0].getAttribute("widgetId"));
if(_26){
_3.layout.dnd._setGcDndHandle(_26,this.withHandles,this.handleClasses);
if(this.hideSource){
_2.style(_26.domNode,"display","");
}
}
},_checkAutoScroll:function(e){
if(this._timer){
clearTimeout(this._timer);
}
this._stopAutoScroll();
var _27=this.dom,y=this._sumAncestorProperties(_27,"offsetTop");
if((e.pageY-_27.offsetTop+30)>_27.clientHeight){
this.autoScrollActive=true;
this._autoScrollDown(_27);
}else{
if((_27.scrollTop>0)&&(e.pageY-y)<30){
this.autoScrollActive=true;
this._autoScrollUp(_27);
}
}
},_autoScrollUp:function(_28){
if(this.autoScrollActive&&_28.scrollTop>0){
_28.scrollTop-=30;
this._timer=setTimeout(_2.hitch(this,"_autoScrollUp",_28),100);
}
},_autoScrollDown:function(_29){
if(this.autoScrollActive&&(_29.scrollTop<(_29.scrollHeight-_29.clientHeight))){
_29.scrollTop+=30;
this._timer=setTimeout(_2.hitch(this,"_autoScrollDown",_29),100);
}
},_stopAutoScroll:function(){
this.autoScrollActive=false;
},_sumAncestorProperties:function(_2a,_2b){
_2a=_2.byId(_2a);
if(!_2a){
return 0;
}
var _2c=0;
while(_2a){
var val=_2a[_2b];
if(val){
_2c+=val-0;
if(_2a==_2.body()){
break;
}
}
_2a=_2a.parentNode;
}
return _2c;
}});
_3.layout.dnd._setGcDndHandle=function(_2d,_2e,_2f,_30){
var cls="GcDndHandle";
if(!_30){
_2.query(".GcDndHandle",_2d.domNode).removeClass(cls);
}
if(!_2e){
_2.addClass(_2d.domNode,cls);
}else{
var _31=false;
for(var i=_2f.length-1;i>=0;i--){
var _32=_2.query("."+_2f[i],_2d.domNode)[0];
if(_32){
_31=true;
if(_2f[i]!=cls){
var _33=_2.query("."+cls,_2d.domNode);
if(_33.length==0){
_2.removeClass(_2d.domNode,cls);
}else{
_33.removeClass(cls);
}
_2.addClass(_32,cls);
}
}
}
if(!_31){
_2.addClass(_2d.domNode,cls);
}
}
};
_2.declare("dojox.layout.dnd.DropIndicator",null,{constructor:function(cn,tag){
this.tag=tag||"div";
this.style=cn||null;
},isInserted:function(){
return (this.node.parentNode&&this.node.parentNode.nodeType==1);
},create:function(){
if(this.node&&this.isInserted()){
return this.node;
}
var h="90px",el=_2.doc.createElement(this.tag);
if(this.style){
el.className=this.style;
el.style.height=h;
}else{
_2.style(el,{position:"relative",border:"1px dashed #F60",margin:"2px",height:h});
}
this.node=el;
return el;
},destroy:function(){
if(!this.node||!this.isInserted()){
return;
}
this.node.parentNode.removeChild(this.node);
this.node=null;
}});
_2.extend(_2.dnd.Manager,{canDrop:function(_34){
var _35=this.target&&_34;
if(this.canDropFlag!=_35){
this.canDropFlag=_35;
if(this.avatar){
this.avatar.update();
}
}
},makeAvatar:function(){
return (this.source.declaredClass=="dojox.layout.dnd.PlottedDnd")?new _3.layout.dnd.Avatar(this,this.source.opacity):new _2.dnd.Avatar(this);
}});
if(_2.isIE){
_3.layout.dnd.handdleIE=[_2.subscribe("/dnd/start",null,function(){
IEonselectstart=document.body.onselectstart;
document.body.onselectstart=function(){
return false;
};
}),_2.subscribe("/dnd/cancel",null,function(){
document.body.onselectstart=IEonselectstart;
}),_2.subscribe("/dnd/drop",null,function(){
document.body.onselectstart=IEonselectstart;
})];
_2.addOnWindowUnload(function(){
_2.forEach(_3.layout.dnd.handdleIE,_2.unsubscribe);
});
}
});
