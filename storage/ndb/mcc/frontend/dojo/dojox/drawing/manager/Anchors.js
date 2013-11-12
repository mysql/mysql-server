//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.manager.Anchors");
_3.drawing.manager.Anchors=_3.drawing.util.oo.declare(function(_4){
this.mouse=_4.mouse;
this.undo=_4.undo;
this.util=_4.util;
this.drawing=_4.drawing;
this.items={};
},{onAddAnchor:function(_5){
},onReset:function(_6){
var st=this.util.byId("drawing").stencils;
st.onDeselect(_6);
st.onSelect(_6);
},onRenderStencil:function(){
for(var nm in this.items){
_2.forEach(this.items[nm].anchors,function(a){
a.shape.moveToFront();
});
}
},onTransformPoint:function(_7){
var _8=this.items[_7.stencil.id].anchors;
var _9=this.items[_7.stencil.id].item;
var _a=[];
_2.forEach(_8,function(a,i){
if(_7.id==a.id||_7.stencil.anchorType!="group"){
}else{
if(_7.org.y==a.org.y){
a.setPoint({dx:0,dy:_7.shape.getTransform().dy-a.shape.getTransform().dy});
}else{
if(_7.org.x==a.org.x){
a.setPoint({dx:_7.shape.getTransform().dx-a.shape.getTransform().dx,dy:0});
}
}
a.shape.moveToFront();
}
var mx=a.shape.getTransform();
_a.push({x:mx.dx+a.org.x,y:mx.dy+a.org.y});
if(a.point.t){
_a[_a.length-1].t=a.point.t;
}
},this);
_9.setPoints(_a);
_9.onTransform(_7);
this.onRenderStencil();
},onAnchorUp:function(_b){
},onAnchorDown:function(_c){
},onAnchorDrag:function(_d){
},onChangeStyle:function(_e){
for(var nm in this.items){
_2.forEach(this.items[nm].anchors,function(a){
a.shape.moveToFront();
});
}
},add:function(_f){
this.items[_f.id]={item:_f,anchors:[]};
if(_f.anchorType=="none"){
return;
}
var pts=_f.points;
_2.forEach(pts,function(p,i){
if(p.noAnchor){
return;
}
if(i==0||i==_f.points.length-1){
}
var a=new _3.drawing.manager.Anchor({stencil:_f,point:p,pointIdx:i,mouse:this.mouse,util:this.util});
this.items[_f.id]._cons=[_2.connect(a,"onRenderStencil",this,"onRenderStencil"),_2.connect(a,"reset",this,"onReset"),_2.connect(a,"onAnchorUp",this,"onAnchorUp"),_2.connect(a,"onAnchorDown",this,"onAnchorDown"),_2.connect(a,"onAnchorDrag",this,"onAnchorDrag"),_2.connect(a,"onTransformPoint",this,"onTransformPoint"),_2.connect(_f,"onChangeStyle",this,"onChangeStyle")];
this.items[_f.id].anchors.push(a);
this.onAddAnchor(a);
},this);
if(_f.shortType=="path"){
var f=pts[0],l=pts[pts.length-1],a=this.items[_f.id].anchors;
if(f.x==l.x&&f.y==l.y){
console.warn("LINK ANVHROS",a[0],a[a.length-1]);
a[0].linkedAnchor=a[a.length-1];
a[a.length-1].linkedAnchor=a[0];
}
}
if(_f.anchorType=="group"){
_2.forEach(this.items[_f.id].anchors,function(_10){
_2.forEach(this.items[_f.id].anchors,function(a){
if(_10.id!=a.id){
if(_10.org.y==a.org.y){
_10.x_anchor=a;
}else{
if(_10.org.x==a.org.x){
_10.y_anchor=a;
}
}
}
},this);
},this);
}
},remove:function(_11){
if(!this.items[_11.id]){
return;
}
_2.forEach(this.items[_11.id].anchors,function(a){
a.destroy();
});
_2.forEach(this.items[_11.id]._cons,_2.disconnect,_2);
this.items[_11.id].anchors=null;
delete this.items[_11.id];
}});
_3.drawing.manager.Anchor=_3.drawing.util.oo.declare(function(_12){
this.defaults=_3.drawing.defaults.copy();
this.mouse=_12.mouse;
this.point=_12.point;
this.pointIdx=_12.pointIdx;
this.util=_12.util;
this.id=_12.id||this.util.uid("anchor");
this.org=_2.mixin({},this.point);
this.stencil=_12.stencil;
if(this.stencil.anchorPositionCheck){
this.anchorPositionCheck=_2.hitch(this.stencil,this.stencil.anchorPositionCheck);
}
if(this.stencil.anchorConstrain){
this.anchorConstrain=_2.hitch(this.stencil,this.stencil.anchorConstrain);
}
this._zCon=_2.connect(this.mouse,"setZoom",this,"render");
this.render();
this.connectMouse();
},{y_anchor:null,x_anchor:null,render:function(){
this.shape&&this.shape.removeShape();
var d=this.defaults.anchors,z=this.mouse.zoom,b=d.width*z,s=d.size*z,p=s/2,_13={width:b,style:d.style,color:d.color,cap:d.cap};
var _14={x:this.point.x-p,y:this.point.y-p,width:s,height:s};
this.shape=this.stencil.container.createRect(_14).setStroke(_13).setFill(d.fill);
this.shape.setTransform({dx:0,dy:0});
this.util.attr(this,"drawingType","anchor");
this.util.attr(this,"id",this.id);
},onRenderStencil:function(_15){
},onTransformPoint:function(_16){
},onAnchorDown:function(obj){
this.selected=obj.id==this.id;
},onAnchorUp:function(obj){
this.selected=false;
this.stencil.onTransformEnd(this);
},onAnchorDrag:function(obj){
if(this.selected){
var mx=this.shape.getTransform();
var pmx=this.shape.getParent().getParent().getTransform();
var _17=this.defaults.anchors.marginZero;
var _18=pmx.dx+this.org.x,_19=pmx.dy+this.org.y,x=obj.x-_18,y=obj.y-_19,s=this.defaults.anchors.minSize;
var _1a,_1b,_1c,_1d;
var chk=this.anchorPositionCheck(x,y,this);
if(chk.x<0){
console.warn("X<0 Shift");
while(this.anchorPositionCheck(x,y,this).x<0){
this.shape.getParent().getParent().applyTransform({dx:2,dy:0});
}
}
if(chk.y<0){
console.warn("Y<0 Shift");
while(this.anchorPositionCheck(x,y,this).y<0){
this.shape.getParent().getParent().applyTransform({dx:0,dy:2});
}
}
if(this.y_anchor){
if(this.org.y>this.y_anchor.org.y){
_1c=this.y_anchor.point.y+s-this.org.y;
_1d=Infinity;
if(y<_1c){
y=_1c;
}
}else{
_1c=-_19+_17;
_1d=this.y_anchor.point.y-s-this.org.y;
if(y<_1c){
y=_1c;
}else{
if(y>_1d){
y=_1d;
}
}
}
}else{
_1c=-_19+_17;
if(y<_1c){
y=_1c;
}
}
if(this.x_anchor){
if(this.org.x>this.x_anchor.org.x){
_1a=this.x_anchor.point.x+s-this.org.x;
_1b=Infinity;
if(x<_1a){
x=_1a;
}
}else{
_1a=-_18+_17;
_1b=this.x_anchor.point.x-s-this.org.x;
if(x<_1a){
x=_1a;
}else{
if(x>_1b){
x=_1b;
}
}
}
}else{
_1a=-_18+_17;
if(x<_1a){
x=_1a;
}
}
var _1e=this.anchorConstrain(x,y);
if(_1e!=null){
x=_1e.x;
y=_1e.y;
}
this.shape.setTransform({dx:x,dy:y});
if(this.linkedAnchor){
this.linkedAnchor.shape.setTransform({dx:x,dy:y});
}
this.onTransformPoint(this);
}
},anchorConstrain:function(x,y){
return null;
},anchorPositionCheck:function(x,y,_1f){
return {x:1,y:1};
},setPoint:function(mx){
this.shape.applyTransform(mx);
},connectMouse:function(){
this._mouseHandle=this.mouse.register(this);
},disconnectMouse:function(){
this.mouse.unregister(this._mouseHandle);
},reset:function(_20){
},destroy:function(){
_2.disconnect(this._zCon);
this.disconnectMouse();
this.shape.removeShape();
}});
});
