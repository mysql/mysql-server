//>>built
define("dojox/drawing/manager/Anchors",["dojo","../util/oo","../defaults"],function(_1,oo,_2){
var _3=oo.declare(function(_4){
this.defaults=_2.copy();
this.mouse=_4.mouse;
this.point=_4.point;
this.pointIdx=_4.pointIdx;
this.util=_4.util;
this.id=_4.id||this.util.uid("anchor");
this.org=_1.mixin({},this.point);
this.stencil=_4.stencil;
if(this.stencil.anchorPositionCheck){
this.anchorPositionCheck=_1.hitch(this.stencil,this.stencil.anchorPositionCheck);
}
if(this.stencil.anchorConstrain){
this.anchorConstrain=_1.hitch(this.stencil,this.stencil.anchorConstrain);
}
this._zCon=_1.connect(this.mouse,"setZoom",this,"render");
this.render();
this.connectMouse();
},{y_anchor:null,x_anchor:null,render:function(){
this.shape&&this.shape.removeShape();
var d=this.defaults.anchors,z=this.mouse.zoom,b=d.width*z,s=d.size*z,p=s/2,_5={width:b,style:d.style,color:d.color,cap:d.cap};
var _6={x:this.point.x-p,y:this.point.y-p,width:s,height:s};
this.shape=this.stencil.container.createRect(_6).setStroke(_5).setFill(d.fill);
this.shape.setTransform({dx:0,dy:0});
this.util.attr(this,"drawingType","anchor");
this.util.attr(this,"id",this.id);
},onRenderStencil:function(_7){
},onTransformPoint:function(_8){
},onAnchorDown:function(_9){
this.selected=_9.id==this.id;
},onAnchorUp:function(_a){
this.selected=false;
this.stencil.onTransformEnd(this);
},onAnchorDrag:function(_b){
if(this.selected){
var mx=this.shape.getTransform();
var _c=this.shape.getParent().getParent().getTransform();
var _d=this.defaults.anchors.marginZero;
var _e=_c.dx+this.org.x,_f=_c.dy+this.org.y,x=_b.x-_e,y=_b.y-_f,s=this.defaults.anchors.minSize;
var _10,_11,_12,_13;
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
_12=this.y_anchor.point.y+s-this.org.y;
_13=Infinity;
if(y<_12){
y=_12;
}
}else{
_12=-_f+_d;
_13=this.y_anchor.point.y-s-this.org.y;
if(y<_12){
y=_12;
}else{
if(y>_13){
y=_13;
}
}
}
}else{
_12=-_f+_d;
if(y<_12){
y=_12;
}
}
if(this.x_anchor){
if(this.org.x>this.x_anchor.org.x){
_10=this.x_anchor.point.x+s-this.org.x;
_11=Infinity;
if(x<_10){
x=_10;
}
}else{
_10=-_e+_d;
_11=this.x_anchor.point.x-s-this.org.x;
if(x<_10){
x=_10;
}else{
if(x>_11){
x=_11;
}
}
}
}else{
_10=-_e+_d;
if(x<_10){
x=_10;
}
}
var _14=this.anchorConstrain(x,y);
if(_14!=null){
x=_14.x;
y=_14.y;
}
this.shape.setTransform({dx:x,dy:y});
if(this.linkedAnchor){
this.linkedAnchor.shape.setTransform({dx:x,dy:y});
}
this.onTransformPoint(this);
}
},anchorConstrain:function(x,y){
return null;
},anchorPositionCheck:function(x,y,_15){
return {x:1,y:1};
},setPoint:function(mx){
this.shape.applyTransform(mx);
},connectMouse:function(){
this._mouseHandle=this.mouse.register(this);
},disconnectMouse:function(){
this.mouse.unregister(this._mouseHandle);
},reset:function(_16){
},destroy:function(){
_1.disconnect(this._zCon);
this.disconnectMouse();
this.shape.removeShape();
}});
return oo.declare(function(_17){
this.mouse=_17.mouse;
this.undo=_17.undo;
this.util=_17.util;
this.drawing=_17.drawing;
this.items={};
},{onAddAnchor:function(_18){
},onReset:function(_19){
var st=this.util.byId("drawing").stencils;
st.onDeselect(_19);
st.onSelect(_19);
},onRenderStencil:function(){
for(var nm in this.items){
_1.forEach(this.items[nm].anchors,function(a){
a.shape.moveToFront();
});
}
},onTransformPoint:function(_1a){
var _1b=this.items[_1a.stencil.id].anchors;
var _1c=this.items[_1a.stencil.id].item;
var pts=[];
_1.forEach(_1b,function(a,i){
if(_1a.id==a.id||_1a.stencil.anchorType!="group"){
}else{
if(_1a.org.y==a.org.y){
a.setPoint({dx:0,dy:_1a.shape.getTransform().dy-a.shape.getTransform().dy});
}else{
if(_1a.org.x==a.org.x){
a.setPoint({dx:_1a.shape.getTransform().dx-a.shape.getTransform().dx,dy:0});
}
}
a.shape.moveToFront();
}
var mx=a.shape.getTransform();
pts.push({x:mx.dx+a.org.x,y:mx.dy+a.org.y});
if(a.point.t){
pts[pts.length-1].t=a.point.t;
}
},this);
_1c.setPoints(pts);
_1c.onTransform(_1a);
this.onRenderStencil();
},onAnchorUp:function(_1d){
},onAnchorDown:function(_1e){
},onAnchorDrag:function(_1f){
},onChangeStyle:function(_20){
for(var nm in this.items){
_1.forEach(this.items[nm].anchors,function(a){
a.shape.moveToFront();
});
}
},add:function(_21){
this.items[_21.id]={item:_21,anchors:[]};
if(_21.anchorType=="none"){
return;
}
var pts=_21.points;
_1.forEach(pts,function(p,i){
if(p.noAnchor){
return;
}
if(i==0||i==_21.points.length-1){
}
var a=new _3({stencil:_21,point:p,pointIdx:i,mouse:this.mouse,util:this.util});
this.items[_21.id]._cons=[_1.connect(a,"onRenderStencil",this,"onRenderStencil"),_1.connect(a,"reset",this,"onReset"),_1.connect(a,"onAnchorUp",this,"onAnchorUp"),_1.connect(a,"onAnchorDown",this,"onAnchorDown"),_1.connect(a,"onAnchorDrag",this,"onAnchorDrag"),_1.connect(a,"onTransformPoint",this,"onTransformPoint"),_1.connect(_21,"onChangeStyle",this,"onChangeStyle")];
this.items[_21.id].anchors.push(a);
this.onAddAnchor(a);
},this);
if(_21.shortType=="path"){
var f=pts[0],l=pts[pts.length-1],a=this.items[_21.id].anchors;
if(f.x==l.x&&f.y==l.y){
console.warn("LINK ANVHROS",a[0],a[a.length-1]);
a[0].linkedAnchor=a[a.length-1];
a[a.length-1].linkedAnchor=a[0];
}
}
if(_21.anchorType=="group"){
_1.forEach(this.items[_21.id].anchors,function(_22){
_1.forEach(this.items[_21.id].anchors,function(a){
if(_22.id!=a.id){
if(_22.org.y==a.org.y){
_22.x_anchor=a;
}else{
if(_22.org.x==a.org.x){
_22.y_anchor=a;
}
}
}
},this);
},this);
}
},remove:function(_23){
if(!this.items[_23.id]){
return;
}
_1.forEach(this.items[_23.id].anchors,function(a){
a.destroy();
});
_1.forEach(this.items[_23.id]._cons,_1.disconnect,_1);
this.items[_23.id].anchors=null;
delete this.items[_23.id];
}});
});
