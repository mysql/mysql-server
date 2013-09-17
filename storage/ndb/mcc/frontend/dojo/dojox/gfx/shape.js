//>>built
define("dojox/gfx/shape",["./_base","dojo/_base/lang","dojo/_base/declare","dojo/_base/window","dojo/_base/sniff","dojo/_base/connect","dojo/_base/array","dojo/dom-construct","dojo/_base/Color","./matrix"],function(g,_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=g.shape={};
var _b={};
var _c={};
_a.register=function(_d){
var t=_d.declaredClass.split(".").pop();
var i=t in _b?++_b[t]:((_b[t]=0));
var _e=t+i;
_c[_e]=_d;
return _e;
};
_a.byId=function(id){
return _c[id];
};
_a.dispose=function(_f){
delete _c[_f.getUID()];
};
_2("dojox.gfx.shape.Shape",null,{constructor:function(){
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
},setShape:function(_10){
this.shape=g.makeParameters(this.shape,_10);
this.bbox=null;
return this;
},setFill:function(_11){
if(!_11){
this.fillStyle=null;
return this;
}
var f=null;
if(typeof (_11)=="object"&&"type" in _11){
switch(_11.type){
case "linear":
f=g.makeParameters(g.defaultLinearGradient,_11);
break;
case "radial":
f=g.makeParameters(g.defaultRadialGradient,_11);
break;
case "pattern":
f=g.makeParameters(g.defaultPattern,_11);
break;
}
}else{
f=g.normalizeColor(_11);
}
this.fillStyle=f;
return this;
},setStroke:function(_12){
if(!_12){
this.strokeStyle=null;
return this;
}
if(typeof _12=="string"||_1.isArray(_12)||_12 instanceof _8){
_12={color:_12};
}
var s=this.strokeStyle=g.makeParameters(g.defaultStroke,_12);
s.color=g.normalizeColor(s.color);
return this;
},setTransform:function(_13){
this.matrix=_9.clone(_13?_9.normalize(_13):_9.identity);
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
},applyRightTransform:function(_14){
return _14?this.setTransform([this.matrix,_14]):this;
},applyLeftTransform:function(_15){
return _15?this.setTransform([_15,this.matrix]):this;
},applyTransform:function(_16){
return _16?this.setTransform([this.matrix,_16]):this;
},removeShape:function(_17){
if(this.parent){
this.parent.remove(this,_17);
}
return this;
},_setParent:function(_18,_19){
this.parent=_18;
return this._updateParentMatrix(_19);
},_updateParentMatrix:function(_1a){
this.parentMatrix=_1a?_9.clone(_1a):null;
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
_a._eventsProcessing={connect:function(_1b,_1c,_1d){
return _5.connect(this.getEventSource(),_1b,_a.fixCallback(this,g.fixTarget,_1c,_1d));
},disconnect:function(_1e){
_5.disconnect(_1e);
}};
_a.fixCallback=function(_1f,_20,_21,_22){
if(!_22){
_22=_21;
_21=null;
}
if(_1.isString(_22)){
_21=_21||_3.global;
if(!_21[_22]){
throw (["dojox.gfx.shape.fixCallback: scope[\"",_22,"\"] is null (scope=\"",_21,"\")"].join(""));
}
return function(e){
return _20(e,_1f)?_21[_22].apply(_21,arguments||[]):undefined;
};
}
return !_21?function(e){
return _20(e,_1f)?_22.apply(_21,arguments):undefined;
}:function(e){
return _20(e,_1f)?_22.apply(_21,arguments||[]):undefined;
};
};
_1.extend(_a.Shape,_a._eventsProcessing);
_a.Container={_init:function(){
this.children=[];
},openBatch:function(){
},closeBatch:function(){
},add:function(_23){
var _24=_23.getParent();
if(_24){
_24.remove(_23,true);
}
this.children.push(_23);
return _23._setParent(this,this._getRealMatrix());
},remove:function(_25,_26){
for(var i=0;i<this.children.length;++i){
if(this.children[i]==_25){
if(_26){
}else{
_25.parent=null;
_25.parentMatrix=null;
}
this.children.splice(i,1);
break;
}
}
return this;
},clear:function(){
var _27;
for(var i=0;i<this.children.length;++i){
_27=this.children[i];
_27.parent=null;
_27.parentMatrix=null;
}
this.children=[];
return this;
},_moveChildToFront:function(_28){
for(var i=0;i<this.children.length;++i){
if(this.children[i]==_28){
this.children.splice(i,1);
this.children.push(_28);
break;
}
}
return this;
},_moveChildToBack:function(_29){
for(var i=0;i<this.children.length;++i){
if(this.children[i]==_29){
this.children.splice(i,1);
this.children.unshift(_29);
break;
}
}
return this;
}};
_2("dojox.gfx.shape.Surface",null,{constructor:function(){
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
},isLoaded:true,onLoad:function(_2a){
},whenLoaded:function(_2b,_2c){
var f=_1.hitch(_2b,_2c);
if(this.isLoaded){
f(this);
}else{
var h=_5.connect(this,"onLoad",function(_2d){
_5.disconnect(h);
f(_2d);
});
}
}});
_1.extend(_a.Surface,_a._eventsProcessing);
_2("dojox.gfx.Point",null,{});
_2("dojox.gfx.Rectangle",null,{});
_2("dojox.gfx.shape.Rect",_a.Shape,{constructor:function(_2e){
this.shape=g.getDefault("Rect");
this.rawNode=_2e;
},getBoundingBox:function(){
return this.shape;
}});
_2("dojox.gfx.shape.Ellipse",_a.Shape,{constructor:function(_2f){
this.shape=g.getDefault("Ellipse");
this.rawNode=_2f;
},getBoundingBox:function(){
if(!this.bbox){
var _30=this.shape;
this.bbox={x:_30.cx-_30.rx,y:_30.cy-_30.ry,width:2*_30.rx,height:2*_30.ry};
}
return this.bbox;
}});
_2("dojox.gfx.shape.Circle",_a.Shape,{constructor:function(_31){
this.shape=g.getDefault("Circle");
this.rawNode=_31;
},getBoundingBox:function(){
if(!this.bbox){
var _32=this.shape;
this.bbox={x:_32.cx-_32.r,y:_32.cy-_32.r,width:2*_32.r,height:2*_32.r};
}
return this.bbox;
}});
_2("dojox.gfx.shape.Line",_a.Shape,{constructor:function(_33){
this.shape=g.getDefault("Line");
this.rawNode=_33;
},getBoundingBox:function(){
if(!this.bbox){
var _34=this.shape;
this.bbox={x:Math.min(_34.x1,_34.x2),y:Math.min(_34.y1,_34.y2),width:Math.abs(_34.x2-_34.x1),height:Math.abs(_34.y2-_34.y1)};
}
return this.bbox;
}});
_2("dojox.gfx.shape.Polyline",_a.Shape,{constructor:function(_35){
this.shape=g.getDefault("Polyline");
this.rawNode=_35;
},setShape:function(_36,_37){
if(_36&&_36 instanceof Array){
this.inherited(arguments,[{points:_36}]);
if(_37&&this.shape.points.length){
this.shape.points.push(this.shape.points[0]);
}
}else{
this.inherited(arguments,[_36]);
}
return this;
},_normalizePoints:function(){
var p=this.shape.points,l=p&&p.length;
if(l&&typeof p[0]=="number"){
var _38=[];
for(var i=0;i<l;i+=2){
_38.push({x:p[i],y:p[i+1]});
}
this.shape.points=_38;
}
},getBoundingBox:function(){
if(!this.bbox&&this.shape.points.length){
var p=this.shape.points;
var l=p.length;
var t=p[0];
var _39={l:t.x,t:t.y,r:t.x,b:t.y};
for(var i=1;i<l;++i){
t=p[i];
if(_39.l>t.x){
_39.l=t.x;
}
if(_39.r<t.x){
_39.r=t.x;
}
if(_39.t>t.y){
_39.t=t.y;
}
if(_39.b<t.y){
_39.b=t.y;
}
}
this.bbox={x:_39.l,y:_39.t,width:_39.r-_39.l,height:_39.b-_39.t};
}
return this.bbox;
}});
_2("dojox.gfx.shape.Image",_a.Shape,{constructor:function(_3a){
this.shape=g.getDefault("Image");
this.rawNode=_3a;
},getBoundingBox:function(){
return this.shape;
},setStroke:function(){
return this;
},setFill:function(){
return this;
}});
_2("dojox.gfx.shape.Text",_a.Shape,{constructor:function(_3b){
this.fontStyle=null;
this.shape=g.getDefault("Text");
this.rawNode=_3b;
},getFont:function(){
return this.fontStyle;
},setFont:function(_3c){
this.fontStyle=typeof _3c=="string"?g.splitFontString(_3c):g.makeParameters(g.defaultFont,_3c);
this._setFont();
return this;
}});
_a.Creator={createShape:function(_3d){
switch(_3d.type){
case g.defaultPath.type:
return this.createPath(_3d);
case g.defaultRect.type:
return this.createRect(_3d);
case g.defaultCircle.type:
return this.createCircle(_3d);
case g.defaultEllipse.type:
return this.createEllipse(_3d);
case g.defaultLine.type:
return this.createLine(_3d);
case g.defaultPolyline.type:
return this.createPolyline(_3d);
case g.defaultImage.type:
return this.createImage(_3d);
case g.defaultText.type:
return this.createText(_3d);
case g.defaultTextPath.type:
return this.createTextPath(_3d);
}
return null;
},createGroup:function(){
return this.createObject(g.Group);
},createRect:function(_3e){
return this.createObject(g.Rect,_3e);
},createEllipse:function(_3f){
return this.createObject(g.Ellipse,_3f);
},createCircle:function(_40){
return this.createObject(g.Circle,_40);
},createLine:function(_41){
return this.createObject(g.Line,_41);
},createPolyline:function(_42){
return this.createObject(g.Polyline,_42);
},createImage:function(_43){
return this.createObject(g.Image,_43);
},createText:function(_44){
return this.createObject(g.Text,_44);
},createPath:function(_45){
return this.createObject(g.Path,_45);
},createTextPath:function(_46){
return this.createObject(g.TextPath,{}).setText(_46);
},createObject:function(_47,_48){
return null;
}};
return _a;
});
