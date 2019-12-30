//>>built
define("dojox/gfx/shape",["./_base","dojo/_base/lang","dojo/_base/declare","dojo/_base/kernel","dojo/_base/sniff","dojo/_base/connect","dojo/_base/array","dojo/dom-construct","dojo/_base/Color","./matrix"],function(g,_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=g.shape={};
var _b={};
var _c={};
var _d=0,_e=_4("ie")<9;
function _f(_10){
var _11={};
for(var key in _10){
if(_10.hasOwnProperty(key)){
_11[key]=_10[key];
}
}
return _11;
};
_a.register=function(s){
var t=s.declaredClass.split(".").pop();
var i=t in _b?++_b[t]:((_b[t]=0));
var uid=t+i;
_c[uid]=s;
return uid;
};
_a.byId=function(id){
return _c[id];
};
_a.dispose=function(s,_12){
if(_12&&s.children){
for(var i=0;i<s.children.length;++i){
_a.dispose(s.children[i],true);
}
}
delete _c[s.getUID()];
++_d;
if(_e&&_d>10000){
_c=_f(_c);
_d=0;
}
};
_a.Shape=_2("dojox.gfx.shape.Shape",null,{constructor:function(){
this.rawNode=null;
this.shape=null;
this.matrix=null;
this.fillStyle=null;
this.strokeStyle=null;
this.bbox=null;
this.parent=null;
this.parentMatrix=null;
var uid=_a.register(this);
this.getUID=function(){
return uid;
};
},destroy:function(){
_a.dispose(this);
},getNode:function(){
return this.rawNode;
},getShape:function(){
return this.shape;
},getTransform:function(){
return this.matrix;
},getFill:function(){
return this.fillStyle;
},getStroke:function(){
return this.strokeStyle;
},getParent:function(){
return this.parent;
},getBoundingBox:function(){
return this.bbox;
},getTransformedBoundingBox:function(){
var b=this.getBoundingBox();
if(!b){
return null;
}
var m=this._getRealMatrix(),gm=_9;
return [gm.multiplyPoint(m,b.x,b.y),gm.multiplyPoint(m,b.x+b.width,b.y),gm.multiplyPoint(m,b.x+b.width,b.y+b.height),gm.multiplyPoint(m,b.x,b.y+b.height)];
},getEventSource:function(){
return this.rawNode;
},setClip:function(_13){
this.clip=_13;
},getClip:function(){
return this.clip;
},setShape:function(_14){
this.shape=g.makeParameters(this.shape,_14);
this.bbox=null;
return this;
},setFill:function(_15){
if(!_15){
this.fillStyle=null;
return this;
}
var f=null;
if(typeof (_15)=="object"&&"type" in _15){
switch(_15.type){
case "linear":
f=g.makeParameters(g.defaultLinearGradient,_15);
break;
case "radial":
f=g.makeParameters(g.defaultRadialGradient,_15);
break;
case "pattern":
f=g.makeParameters(g.defaultPattern,_15);
break;
}
}else{
f=g.normalizeColor(_15);
}
this.fillStyle=f;
return this;
},setStroke:function(_16){
if(!_16){
this.strokeStyle=null;
return this;
}
if(typeof _16=="string"||_1.isArray(_16)||_16 instanceof _8){
_16={color:_16};
}
var s=this.strokeStyle=g.makeParameters(g.defaultStroke,_16);
s.color=g.normalizeColor(s.color);
return this;
},setTransform:function(_17){
this.matrix=_9.clone(_17?_9.normalize(_17):_9.identity);
return this._applyTransform();
},_applyTransform:function(){
return this;
},moveToFront:function(){
var p=this.getParent();
if(p){
p._moveChildToFront(this);
this._moveToFront();
}
return this;
},moveToBack:function(){
var p=this.getParent();
if(p){
p._moveChildToBack(this);
this._moveToBack();
}
return this;
},_moveToFront:function(){
},_moveToBack:function(){
},applyRightTransform:function(_18){
return _18?this.setTransform([this.matrix,_18]):this;
},applyLeftTransform:function(_19){
return _19?this.setTransform([_19,this.matrix]):this;
},applyTransform:function(_1a){
return _1a?this.setTransform([this.matrix,_1a]):this;
},removeShape:function(_1b){
if(this.parent){
this.parent.remove(this,_1b);
}
return this;
},_setParent:function(_1c,_1d){
this.parent=_1c;
return this._updateParentMatrix(_1d);
},_updateParentMatrix:function(_1e){
this.parentMatrix=_1e?_9.clone(_1e):null;
return this._applyTransform();
},_getRealMatrix:function(){
var m=this.matrix;
var p=this.parent;
while(p){
if(p.matrix){
m=_9.multiply(p.matrix,m);
}
p=p.parent;
}
return m;
}});
_a._eventsProcessing={connect:function(_1f,_20,_21){
return _5.connect(this.getEventSource(),_1f,_a.fixCallback(this,g.fixTarget,_20,_21));
},disconnect:function(_22){
_5.disconnect(_22);
}};
_a.fixCallback=function(_23,_24,_25,_26){
if(!_26){
_26=_25;
_25=null;
}
if(_1.isString(_26)){
_25=_25||_3.global;
if(!_25[_26]){
throw (["dojox.gfx.shape.fixCallback: scope[\"",_26,"\"] is null (scope=\"",_25,"\")"].join(""));
}
return function(e){
return _24(e,_23)?_25[_26].apply(_25,arguments||[]):undefined;
};
}
return !_25?function(e){
return _24(e,_23)?_26.apply(_25,arguments):undefined;
}:function(e){
return _24(e,_23)?_26.apply(_25,arguments||[]):undefined;
};
};
_1.extend(_a.Shape,_a._eventsProcessing);
_a.Container={_init:function(){
this.children=[];
},openBatch:function(){
},closeBatch:function(){
},add:function(_27){
var _28=_27.getParent();
if(_28){
_28.remove(_27,true);
}
this.children.push(_27);
return _27._setParent(this,this._getRealMatrix());
},remove:function(_29,_2a){
for(var i=0;i<this.children.length;++i){
if(this.children[i]==_29){
if(_2a){
}else{
_29.parent=null;
_29.parentMatrix=null;
}
this.children.splice(i,1);
break;
}
}
return this;
},clear:function(_2b){
var _2c;
for(var i=0;i<this.children.length;++i){
_2c=this.children[i];
_2c.parent=null;
_2c.parentMatrix=null;
if(_2b){
_2c.destroy();
}
}
this.children=[];
return this;
},getBoundingBox:function(){
if(this.children){
var _2d=null;
_6.forEach(this.children,function(_2e){
var bb=_2e.getBoundingBox();
if(bb){
var ct=_2e.getTransform();
if(ct){
bb=_9.multiplyRectangle(ct,bb);
}
if(_2d){
_2d.x=Math.min(_2d.x,bb.x);
_2d.y=Math.min(_2d.y,bb.y);
_2d.endX=Math.max(_2d.endX,bb.x+bb.width);
_2d.endY=Math.max(_2d.endY,bb.y+bb.height);
}else{
_2d={x:bb.x,y:bb.y,endX:bb.x+bb.width,endY:bb.y+bb.height};
}
}
});
if(_2d){
_2d.width=_2d.endX-_2d.x;
_2d.height=_2d.endY-_2d.y;
}
return _2d;
}
return null;
},_moveChildToFront:function(_2f){
for(var i=0;i<this.children.length;++i){
if(this.children[i]==_2f){
this.children.splice(i,1);
this.children.push(_2f);
break;
}
}
return this;
},_moveChildToBack:function(_30){
for(var i=0;i<this.children.length;++i){
if(this.children[i]==_30){
this.children.splice(i,1);
this.children.unshift(_30);
break;
}
}
return this;
}};
_a.Surface=_2("dojox.gfx.shape.Surface",null,{constructor:function(){
this.rawNode=null;
this._parent=null;
this._nodes=[];
this._events=[];
},destroy:function(){
_6.forEach(this._nodes,_7.destroy);
this._nodes=[];
_6.forEach(this._events,_5.disconnect);
this._events=[];
this.rawNode=null;
if(_4("ie")){
while(this._parent.lastChild){
_7.destroy(this._parent.lastChild);
}
}else{
this._parent.innerHTML="";
}
this._parent=null;
},getEventSource:function(){
return this.rawNode;
},_getRealMatrix:function(){
return null;
},isLoaded:true,onLoad:function(_31){
},whenLoaded:function(_32,_33){
var f=_1.hitch(_32,_33);
if(this.isLoaded){
f(this);
}else{
var h=_5.connect(this,"onLoad",function(_34){
_5.disconnect(h);
f(_34);
});
}
}});
_1.extend(_a.Surface,_a._eventsProcessing);
_a.Rect=_2("dojox.gfx.shape.Rect",_a.Shape,{constructor:function(_35){
this.shape=g.getDefault("Rect");
this.rawNode=_35;
},getBoundingBox:function(){
return this.shape;
}});
_a.Ellipse=_2("dojox.gfx.shape.Ellipse",_a.Shape,{constructor:function(_36){
this.shape=g.getDefault("Ellipse");
this.rawNode=_36;
},getBoundingBox:function(){
if(!this.bbox){
var _37=this.shape;
this.bbox={x:_37.cx-_37.rx,y:_37.cy-_37.ry,width:2*_37.rx,height:2*_37.ry};
}
return this.bbox;
}});
_a.Circle=_2("dojox.gfx.shape.Circle",_a.Shape,{constructor:function(_38){
this.shape=g.getDefault("Circle");
this.rawNode=_38;
},getBoundingBox:function(){
if(!this.bbox){
var _39=this.shape;
this.bbox={x:_39.cx-_39.r,y:_39.cy-_39.r,width:2*_39.r,height:2*_39.r};
}
return this.bbox;
}});
_a.Line=_2("dojox.gfx.shape.Line",_a.Shape,{constructor:function(_3a){
this.shape=g.getDefault("Line");
this.rawNode=_3a;
},getBoundingBox:function(){
if(!this.bbox){
var _3b=this.shape;
this.bbox={x:Math.min(_3b.x1,_3b.x2),y:Math.min(_3b.y1,_3b.y2),width:Math.abs(_3b.x2-_3b.x1),height:Math.abs(_3b.y2-_3b.y1)};
}
return this.bbox;
}});
_a.Polyline=_2("dojox.gfx.shape.Polyline",_a.Shape,{constructor:function(_3c){
this.shape=g.getDefault("Polyline");
this.rawNode=_3c;
},setShape:function(_3d,_3e){
if(_3d&&_3d instanceof Array){
this.inherited(arguments,[{points:_3d}]);
if(_3e&&this.shape.points.length){
this.shape.points.push(this.shape.points[0]);
}
}else{
this.inherited(arguments,[_3d]);
}
return this;
},_normalizePoints:function(){
var p=this.shape.points,l=p&&p.length;
if(l&&typeof p[0]=="number"){
var _3f=[];
for(var i=0;i<l;i+=2){
_3f.push({x:p[i],y:p[i+1]});
}
this.shape.points=_3f;
}
},getBoundingBox:function(){
if(!this.bbox&&this.shape.points.length){
var p=this.shape.points;
var l=p.length;
var t=p[0];
var _40={l:t.x,t:t.y,r:t.x,b:t.y};
for(var i=1;i<l;++i){
t=p[i];
if(_40.l>t.x){
_40.l=t.x;
}
if(_40.r<t.x){
_40.r=t.x;
}
if(_40.t>t.y){
_40.t=t.y;
}
if(_40.b<t.y){
_40.b=t.y;
}
}
this.bbox={x:_40.l,y:_40.t,width:_40.r-_40.l,height:_40.b-_40.t};
}
return this.bbox;
}});
_a.Image=_2("dojox.gfx.shape.Image",_a.Shape,{constructor:function(_41){
this.shape=g.getDefault("Image");
this.rawNode=_41;
},getBoundingBox:function(){
return this.shape;
},setStroke:function(){
return this;
},setFill:function(){
return this;
}});
_a.Text=_2(_a.Shape,{constructor:function(_42){
this.fontStyle=null;
this.shape=g.getDefault("Text");
this.rawNode=_42;
},getFont:function(){
return this.fontStyle;
},setFont:function(_43){
this.fontStyle=typeof _43=="string"?g.splitFontString(_43):g.makeParameters(g.defaultFont,_43);
this._setFont();
return this;
}});
_a.Creator={createShape:function(_44){
switch(_44.type){
case g.defaultPath.type:
return this.createPath(_44);
case g.defaultRect.type:
return this.createRect(_44);
case g.defaultCircle.type:
return this.createCircle(_44);
case g.defaultEllipse.type:
return this.createEllipse(_44);
case g.defaultLine.type:
return this.createLine(_44);
case g.defaultPolyline.type:
return this.createPolyline(_44);
case g.defaultImage.type:
return this.createImage(_44);
case g.defaultText.type:
return this.createText(_44);
case g.defaultTextPath.type:
return this.createTextPath(_44);
}
return null;
},createGroup:function(){
return this.createObject(g.Group);
},createRect:function(_45){
return this.createObject(g.Rect,_45);
},createEllipse:function(_46){
return this.createObject(g.Ellipse,_46);
},createCircle:function(_47){
return this.createObject(g.Circle,_47);
},createLine:function(_48){
return this.createObject(g.Line,_48);
},createPolyline:function(_49){
return this.createObject(g.Polyline,_49);
},createImage:function(_4a){
return this.createObject(g.Image,_4a);
},createText:function(_4b){
return this.createObject(g.Text,_4b);
},createPath:function(_4c){
return this.createObject(g.Path,_4c);
},createTextPath:function(_4d){
return this.createObject(g.TextPath,{}).setText(_4d);
},createObject:function(_4e,_4f){
return null;
}};
return _a;
});
