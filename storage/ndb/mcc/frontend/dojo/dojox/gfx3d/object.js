//>>built
define("dojox/gfx3d/object",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojox/gfx","dojox/gfx/matrix","./_base","./scheduler","./gradient","./vector","./matrix","./lighting"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
var _c=_7.scheduler;
var _d=function(o,x){
if(arguments.length>1){
o=x;
}
var e={};
for(var i in o){
if(i in e){
continue;
}
}
};
_2("dojox.gfx3d.Object",null,{constructor:function(){
this.object=null;
this.matrix=null;
this.cache=null;
this.renderer=null;
this.parent=null;
this.strokeStyle=null;
this.fillStyle=null;
this.shape=null;
},setObject:function(_e){
this.object=_4.makeParameters(this.object,_e);
return this;
},setTransform:function(_f){
this.matrix=_a.clone(_f?_a.normalize(_f):_6.identity,true);
return this;
},applyRightTransform:function(_10){
return _10?this.setTransform([this.matrix,_10]):this;
},applyLeftTransform:function(_11){
return _11?this.setTransform([_11,this.matrix]):this;
},applyTransform:function(_12){
return _12?this.setTransform([this.matrix,_12]):this;
},setFill:function(_13){
this.fillStyle=_13;
return this;
},setStroke:function(_14){
this.strokeStyle=_14;
return this;
},toStdFill:function(_15,_16){
return (this.fillStyle&&typeof this.fillStyle["type"]!="undefined")?_15[this.fillStyle.type](_16,this.fillStyle.finish,this.fillStyle.color):this.fillStyle;
},invalidate:function(){
this.renderer.addTodo(this);
},destroy:function(){
if(this.shape){
var p=this.shape.getParent();
if(p){
p.remove(this.shape);
}
this.shape=null;
}
},render:function(_17){
throw "Pure virtual function, not implemented";
},draw:function(_18){
throw "Pure virtual function, not implemented";
},getZOrder:function(){
return 0;
},getOutline:function(){
return null;
}});
_2("dojox.gfx3d.Scene",_6.Object,{constructor:function(){
this.objects=[];
this.todos=[];
this.schedule=_c.zOrder;
this._draw=_6.drawer.conservative;
},setFill:function(_19){
this.fillStyle=_19;
_1.forEach(this.objects,function(_1a){
_1a.setFill(_19);
});
return this;
},setStroke:function(_1b){
this.strokeStyle=_1b;
_1.forEach(this.objects,function(_1c){
_1c.setStroke(_1b);
});
return this;
},render:function(_1d,_1e){
var m=_a.multiply(_1d,this.matrix);
if(_1e){
this.todos=this.objects;
}
_1.forEach(this.todos,function(_1f){
_1f.render(m,_1e);
});
},draw:function(_20){
this.objects=this.schedule(this.objects);
this._draw(this.todos,this.objects,this.renderer);
},addTodo:function(_21){
if(_1.every(this.todos,function(_22){
return _22!=_21;
})){
this.todos.push(_21);
this.invalidate();
}
},invalidate:function(){
this.parent.addTodo(this);
},getZOrder:function(){
var _23=0;
_1.forEach(this.objects,function(_24){
_23+=_24.getZOrder();
});
return (this.objects.length>1)?_23/this.objects.length:0;
}});
_2("dojox.gfx3d.Edges",_6.Object,{constructor:function(){
this.object=_3.clone(_6.defaultEdges);
},setObject:function(_25,_26){
this.object=_4.makeParameters(this.object,(_25 instanceof Array)?{points:_25,style:_26}:_25);
return this;
},getZOrder:function(){
var _27=0;
_1.forEach(this.cache,function(_28){
_27+=_28.z;
});
return (this.cache.length>1)?_27/this.cache.length:0;
},render:function(_29){
var m=_a.multiply(_29,this.matrix);
this.cache=_1.map(this.object.points,function(_2a){
return _a.multiplyPoint(m,_2a);
});
},draw:function(){
var c=this.cache;
if(this.shape){
this.shape.setShape("");
}else{
this.shape=this.renderer.createPath();
}
var p=this.shape.setAbsoluteMode("absolute");
if(this.object.style=="strip"||this.object.style=="loop"){
p.moveTo(c[0].x,c[0].y);
_1.forEach(c.slice(1),function(_2b){
p.lineTo(_2b.x,_2b.y);
});
if(this.object.style=="loop"){
p.closePath();
}
}else{
for(var i=0;i<this.cache.length;){
p.moveTo(c[i].x,c[i].y);
i++;
p.lineTo(c[i].x,c[i].y);
i++;
}
}
p.setStroke(this.strokeStyle);
}});
_2("dojox.gfx3d.Orbit",_6.Object,{constructor:function(){
this.object=_3.clone(_6.defaultOrbit);
},render:function(_2c){
var m=_a.multiply(_2c,this.matrix);
var _2d=[0,Math.PI/4,Math.PI/3];
var _2e=_a.multiplyPoint(m,this.object.center);
var _2f=_1.map(_2d,function(_30){
return {x:this.center.x+this.radius*Math.cos(_30),y:this.center.y+this.radius*Math.sin(_30),z:this.center.z};
},this.object);
_2f=_1.map(_2f,function(_31){
return _a.multiplyPoint(m,_31);
});
var _32=_9.normalize(_2f);
_2f=_1.map(_2f,function(_33){
return _9.substract(_33,_2e);
});
var A={xx:_2f[0].x*_2f[0].y,xy:_2f[0].y*_2f[0].y,xz:1,yx:_2f[1].x*_2f[1].y,yy:_2f[1].y*_2f[1].y,yz:1,zx:_2f[2].x*_2f[2].y,zy:_2f[2].y*_2f[2].y,zz:1,dx:0,dy:0,dz:0};
var B=_1.map(_2f,function(_34){
return -Math.pow(_34.x,2);
});
var X=_a.multiplyPoint(_a.invert(A),B[0],B[1],B[2]);
var _35=Math.atan2(X.x,1-X.y)/2;
var _36=_1.map(_2f,function(_37){
return _5.multiplyPoint(_5.rotate(-_35),_37.x,_37.y);
});
var a=Math.pow(_36[0].x,2);
var b=Math.pow(_36[0].y,2);
var c=Math.pow(_36[1].x,2);
var d=Math.pow(_36[1].y,2);
var rx=Math.sqrt((a*d-b*c)/(d-b));
var ry=Math.sqrt((a*d-b*c)/(a-c));
this.cache={cx:_2e.x,cy:_2e.y,rx:rx,ry:ry,theta:_35,normal:_32};
},draw:function(_38){
if(this.shape){
this.shape.setShape(this.cache);
}else{
this.shape=this.renderer.createEllipse(this.cache);
}
this.shape.applyTransform(_5.rotateAt(this.cache.theta,this.cache.cx,this.cache.cy)).setStroke(this.strokeStyle).setFill(this.toStdFill(_38,this.cache.normal));
}});
_2("dojox.gfx3d.Path3d",_6.Object,{constructor:function(){
this.object=_3.clone(_6.defaultPath3d);
this.segments=[];
this.absolute=true;
this.last={};
this.path="";
},_collectArgs:function(_39,_3a){
for(var i=0;i<_3a.length;++i){
var t=_3a[i];
if(typeof (t)=="boolean"){
_39.push(t?1:0);
}else{
if(typeof (t)=="number"){
_39.push(t);
}else{
if(t instanceof Array){
this._collectArgs(_39,t);
}else{
if("x" in t&&"y" in t){
_39.push(t.x);
_39.push(t.y);
}
}
}
}
}
},_validSegments:{m:3,l:3,z:0},_pushSegment:function(_3b,_3c){
var _3d=this._validSegments[_3b.toLowerCase()],_3e;
if(typeof (_3d)=="number"){
if(_3d){
if(_3c.length>=_3d){
_3e={action:_3b,args:_3c.slice(0,_3c.length-_3c.length%_3d)};
this.segments.push(_3e);
}
}else{
_3e={action:_3b,args:[]};
this.segments.push(_3e);
}
}
},moveTo:function(){
var _3f=[];
this._collectArgs(_3f,arguments);
this._pushSegment(this.absolute?"M":"m",_3f);
return this;
},lineTo:function(){
var _40=[];
this._collectArgs(_40,arguments);
this._pushSegment(this.absolute?"L":"l",_40);
return this;
},closePath:function(){
this._pushSegment("Z",[]);
return this;
},render:function(_41){
var m=_a.multiply(_41,this.matrix);
var _42="";
var _43=this._validSegments;
_1.forEach(this.segments,function(_44){
_42+=_44.action;
for(var i=0;i<_44.args.length;i+=_43[_44.action.toLowerCase()]){
var pt=_a.multiplyPoint(m,_44.args[i],_44.args[i+1],_44.args[i+2]);
_42+=" "+pt.x+" "+pt.y;
}
});
this.cache=_42;
},_draw:function(){
return this.parent.createPath(this.cache);
}});
_2("dojox.gfx3d.Triangles",_6.Object,{constructor:function(){
this.object=_3.clone(_6.defaultTriangles);
},setObject:function(_45,_46){
if(_45 instanceof Array){
this.object=_4.makeParameters(this.object,{points:_45,style:_46});
}else{
this.object=_4.makeParameters(this.object,_45);
}
return this;
},render:function(_47){
var m=_a.multiply(_47,this.matrix);
var c=_1.map(this.object.points,function(_48){
return _a.multiplyPoint(m,_48);
});
this.cache=[];
var _49=c.slice(0,2);
var _4a=c[0];
if(this.object.style=="strip"){
_1.forEach(c.slice(2),function(_4b){
_49.push(_4b);
_49.push(_49[0]);
this.cache.push(_49);
_49=_49.slice(1,3);
},this);
}else{
if(this.object.style=="fan"){
_1.forEach(c.slice(2),function(_4c){
_49.push(_4c);
_49.push(_4a);
this.cache.push(_49);
_49=[_4a,_4c];
},this);
}else{
for(var i=0;i<c.length;){
this.cache.push([c[i],c[i+1],c[i+2],c[i]]);
i+=3;
}
}
}
},draw:function(_4d){
this.cache=_c.bsp(this.cache,function(it){
return it;
});
if(this.shape){
this.shape.clear();
}else{
this.shape=this.renderer.createGroup();
}
_1.forEach(this.cache,function(_4e){
this.shape.createPolyline(_4e).setStroke(this.strokeStyle).setFill(this.toStdFill(_4d,_9.normalize(_4e)));
},this);
},getZOrder:function(){
var _4f=0;
_1.forEach(this.cache,function(_50){
_4f+=(_50[0].z+_50[1].z+_50[2].z)/3;
});
return (this.cache.length>1)?_4f/this.cache.length:0;
}});
_2("dojox.gfx3d.Quads",_6.Object,{constructor:function(){
this.object=_3.clone(_6.defaultQuads);
},setObject:function(_51,_52){
this.object=_4.makeParameters(this.object,(_51 instanceof Array)?{points:_51,style:_52}:_51);
return this;
},render:function(_53){
var m=_a.multiply(_53,this.matrix),i;
var c=_1.map(this.object.points,function(_54){
return _a.multiplyPoint(m,_54);
});
this.cache=[];
if(this.object.style=="strip"){
var _55=c.slice(0,2);
for(i=2;i<c.length;){
_55=_55.concat([c[i],c[i+1],_55[0]]);
this.cache.push(_55);
_55=_55.slice(2,4);
i+=2;
}
}else{
for(i=0;i<c.length;){
this.cache.push([c[i],c[i+1],c[i+2],c[i+3],c[i]]);
i+=4;
}
}
},draw:function(_56){
this.cache=_6.scheduler.bsp(this.cache,function(it){
return it;
});
if(this.shape){
this.shape.clear();
}else{
this.shape=this.renderer.createGroup();
}
for(var x=0;x<this.cache.length;x++){
this.shape.createPolyline(this.cache[x]).setStroke(this.strokeStyle).setFill(this.toStdFill(_56,_9.normalize(this.cache[x])));
}
},getZOrder:function(){
var _57=0;
for(var x=0;x<this.cache.length;x++){
var i=this.cache[x];
_57+=(i[0].z+i[1].z+i[2].z+i[3].z)/4;
}
return (this.cache.length>1)?_57/this.cache.length:0;
}});
_2("dojox.gfx3d.Polygon",_6.Object,{constructor:function(){
this.object=_3.clone(_6.defaultPolygon);
},setObject:function(_58){
this.object=_4.makeParameters(this.object,(_58 instanceof Array)?{path:_58}:_58);
return this;
},render:function(_59){
var m=_a.multiply(_59,this.matrix);
this.cache=_1.map(this.object.path,function(_5a){
return _a.multiplyPoint(m,_5a);
});
this.cache.push(this.cache[0]);
},draw:function(_5b){
if(this.shape){
this.shape.setShape({points:this.cache});
}else{
this.shape=this.renderer.createPolyline({points:this.cache});
}
this.shape.setStroke(this.strokeStyle).setFill(this.toStdFill(_5b,_a.normalize(this.cache)));
},getZOrder:function(){
var _5c=0;
for(var x=0;x<this.cache.length;x++){
_5c+=this.cache[x].z;
}
return (this.cache.length>1)?_5c/this.cache.length:0;
},getOutline:function(){
return this.cache.slice(0,3);
}});
_2("dojox.gfx3d.Cube",_6.Object,{constructor:function(){
this.object=_3.clone(_6.defaultCube);
this.polygons=[];
},setObject:function(_5d){
this.object=_4.makeParameters(this.object,_5d);
},render:function(_5e){
var a=this.object.top;
var g=this.object.bottom;
var b={x:g.x,y:a.y,z:a.z};
var c={x:g.x,y:g.y,z:a.z};
var d={x:a.x,y:g.y,z:a.z};
var e={x:a.x,y:a.y,z:g.z};
var f={x:g.x,y:a.y,z:g.z};
var h={x:a.x,y:g.y,z:g.z};
var _5f=[a,b,c,d,e,f,g,h];
var m=_a.multiply(_5e,this.matrix);
var p=_1.map(_5f,function(_60){
return _a.multiplyPoint(m,_60);
});
a=p[0];
b=p[1];
c=p[2];
d=p[3];
e=p[4];
f=p[5];
g=p[6];
h=p[7];
this.cache=[[a,b,c,d,a],[e,f,g,h,e],[a,d,h,e,a],[d,c,g,h,d],[c,b,f,g,c],[b,a,e,f,b]];
},draw:function(_61){
this.cache=_6.scheduler.bsp(this.cache,function(it){
return it;
});
var _62=this.cache.slice(3);
if(this.shape){
this.shape.clear();
}else{
this.shape=this.renderer.createGroup();
}
for(var x=0;x<_62.length;x++){
this.shape.createPolyline(_62[x]).setStroke(this.strokeStyle).setFill(this.toStdFill(_61,_9.normalize(_62[x])));
}
},getZOrder:function(){
var top=this.cache[0][0];
var _63=this.cache[1][2];
return (top.z+_63.z)/2;
}});
_2("dojox.gfx3d.Cylinder",_6.Object,{constructor:function(){
this.object=_3.clone(_6.defaultCylinder);
},render:function(_64){
var m=_a.multiply(_64,this.matrix);
var _65=[0,Math.PI/4,Math.PI/3];
var _66=_a.multiplyPoint(m,this.object.center);
var _67=_1.map(_65,function(_68){
return {x:this.center.x+this.radius*Math.cos(_68),y:this.center.y+this.radius*Math.sin(_68),z:this.center.z};
},this.object);
_67=_1.map(_67,function(_69){
return _9.substract(_a.multiplyPoint(m,_69),_66);
});
var A={xx:_67[0].x*_67[0].y,xy:_67[0].y*_67[0].y,xz:1,yx:_67[1].x*_67[1].y,yy:_67[1].y*_67[1].y,yz:1,zx:_67[2].x*_67[2].y,zy:_67[2].y*_67[2].y,zz:1,dx:0,dy:0,dz:0};
var B=_1.map(_67,function(_6a){
return -Math.pow(_6a.x,2);
});
var X=_a.multiplyPoint(_a.invert(A),B[0],B[1],B[2]);
var _6b=Math.atan2(X.x,1-X.y)/2;
var _6c=_1.map(_67,function(_6d){
return _5.multiplyPoint(_5.rotate(-_6b),_6d.x,_6d.y);
});
var a=Math.pow(_6c[0].x,2);
var b=Math.pow(_6c[0].y,2);
var c=Math.pow(_6c[1].x,2);
var d=Math.pow(_6c[1].y,2);
var rx=Math.sqrt((a*d-b*c)/(d-b));
var ry=Math.sqrt((a*d-b*c)/(a-c));
if(rx<ry){
var t=rx;
rx=ry;
ry=t;
_6b-=Math.PI/2;
}
var top=_a.multiplyPoint(m,_9.sum(this.object.center,{x:0,y:0,z:this.object.height}));
var _6e=this.fillStyle.type=="constant"?this.fillStyle.color:_8(this.renderer.lighting,this.fillStyle,this.object.center,this.object.radius,Math.PI,2*Math.PI,m);
if(isNaN(rx)||isNaN(ry)||isNaN(_6b)){
rx=this.object.radius,ry=0,_6b=0;
}
this.cache={center:_66,top:top,rx:rx,ry:ry,theta:_6b,gradient:_6e};
},draw:function(){
var c=this.cache,v=_9,m=_5,_6f=[c.center,c.top],_70=v.substract(c.top,c.center);
if(v.dotProduct(_70,this.renderer.lighting.incident)>0){
_6f=[c.top,c.center];
_70=v.substract(c.center,c.top);
}
var _71=this.renderer.lighting[this.fillStyle.type](_70,this.fillStyle.finish,this.fillStyle.color),d=Math.sqrt(Math.pow(c.center.x-c.top.x,2)+Math.pow(c.center.y-c.top.y,2));
if(this.shape){
this.shape.clear();
}else{
this.shape=this.renderer.createGroup();
}
this.shape.createPath("").moveTo(0,-c.rx).lineTo(d,-c.rx).lineTo(d,c.rx).lineTo(0,c.rx).arcTo(c.ry,c.rx,0,true,true,0,-c.rx).setFill(c.gradient).setStroke(this.strokeStyle).setTransform([m.translate(_6f[0]),m.rotate(Math.atan2(_6f[1].y-_6f[0].y,_6f[1].x-_6f[0].x))]);
if(c.rx>0&&c.ry>0){
this.shape.createEllipse({cx:_6f[1].x,cy:_6f[1].y,rx:c.rx,ry:c.ry}).setFill(_71).setStroke(this.strokeStyle).applyTransform(m.rotateAt(c.theta,_6f[1]));
}
}});
_2("dojox.gfx3d.Viewport",_4.Group,{constructor:function(){
this.dimension=null;
this.objects=[];
this.todos=[];
this.renderer=this;
this.schedule=_6.scheduler.zOrder;
this.draw=_6.drawer.conservative;
this.deep=false;
this.lights=[];
this.lighting=null;
},setCameraTransform:function(_72){
this.camera=_a.clone(_72?_a.normalize(_72):_6.identity,true);
this.invalidate();
return this;
},applyCameraRightTransform:function(_73){
return _73?this.setCameraTransform([this.camera,_73]):this;
},applyCameraLeftTransform:function(_74){
return _74?this.setCameraTransform([_74,this.camera]):this;
},applyCameraTransform:function(_75){
return this.applyCameraRightTransform(_75);
},setLights:function(_76,_77,_78){
this.lights=(_76 instanceof Array)?{sources:_76,ambient:_77,specular:_78}:_76;
var _79={x:0,y:0,z:1};
this.lighting=new _b.Model(_79,this.lights.sources,this.lights.ambient,this.lights.specular);
this.invalidate();
return this;
},addLights:function(_7a){
return this.setLights(this.lights.sources.concat(_7a));
},addTodo:function(_7b){
if(_1.every(this.todos,function(_7c){
return _7c!=_7b;
})){
this.todos.push(_7b);
}
},invalidate:function(){
this.deep=true;
this.todos=this.objects;
},setDimensions:function(dim){
if(dim){
var w=_3.isString(dim.width)?parseInt(dim.width):dim.width;
var h=_3.isString(dim.height)?parseInt(dim.height):dim.height;
if(this.rawNode){
var trs=this.rawNode.style;
if(trs){
trs.height=h;
trs.width=w;
}else{
this.rawNode.width=w;
this.rawNode.height=h;
}
}
this.dimension={width:w,height:h};
}else{
this.dimension=null;
}
},render:function(){
if(!this.todos.length){
return;
}
var m=_a;
for(var x=0;x<this.todos.length;x++){
this.todos[x].render(_a.normalize([m.cameraRotateXg(180),m.cameraTranslate(0,this.dimension.height,0),this.camera]),this.deep);
}
this.objects=this.schedule(this.objects);
this.draw(this.todos,this.objects,this);
this.todos=[];
this.deep=false;
}});
_6.Viewport.nodeType=_4.Group.nodeType;
_6._creators={createEdges:function(_7d,_7e){
return this.create3DObject(_6.Edges,_7d,_7e);
},createTriangles:function(_7f,_80){
return this.create3DObject(_6.Triangles,_7f,_80);
},createQuads:function(_81,_82){
return this.create3DObject(_6.Quads,_81,_82);
},createPolygon:function(_83){
return this.create3DObject(_6.Polygon,_83);
},createOrbit:function(_84){
return this.create3DObject(_6.Orbit,_84);
},createCube:function(_85){
return this.create3DObject(_6.Cube,_85);
},createCylinder:function(_86){
return this.create3DObject(_6.Cylinder,_86);
},createPath3d:function(_87){
return this.create3DObject(_6.Path3d,_87);
},createScene:function(){
return this.create3DObject(_6.Scene);
},create3DObject:function(_88,_89,_8a){
var obj=new _88();
this.adopt(obj);
if(_89){
obj.setObject(_89,_8a);
}
return obj;
},adopt:function(obj){
obj.renderer=this.renderer;
obj.parent=this;
this.objects.push(obj);
this.addTodo(obj);
return this;
},abandon:function(obj,_8b){
for(var i=0;i<this.objects.length;++i){
if(this.objects[i]==obj){
this.objects.splice(i,1);
}
}
obj.parent=null;
return this;
},setScheduler:function(_8c){
this.schedule=_8c;
},setDrawer:function(_8d){
this.draw=_8d;
}};
_3.extend(_6.Viewport,_6._creators);
_3.extend(_6.Scene,_6._creators);
delete _6._creators;
_3.extend(_4.Surface,{createViewport:function(){
var _8e=this.createObject(_6.Viewport,null,true);
_8e.setDimensions(this.getDimensions());
return _8e;
}});
return _6.Object;
});
