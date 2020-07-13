//>>built
define("dojox/gfx/shape",["./_base","dojo/_base/lang","dojo/_base/declare","dojo/_base/kernel","dojo/_base/sniff","dojo/on","dojo/_base/array","dojo/dom-construct","dojo/_base/Color","./matrix"],function(g,_1,_2,_3,_4,on,_5,_6,_7,_8){
function _9(a,_a){
var _b=(a.length-1);
while(_a<_b){
a[_a]=a[++_a];
}
a.length=_b;
};
var _c=g.shape={};
_c.Shape=_2("dojox.gfx.shape.Shape",null,{constructor:function(){
this.rawNode=null;
this.shape=null;
this.matrix=null;
this.fillStyle=null;
this.strokeStyle=null;
this.bbox=null;
this.parent=null;
this.parentMatrix=null;
if(_4("gfxRegistry")){
var _d=_c.register(this);
this.getUID=function(){
return _d;
};
}
},destroy:function(){
if(_4("gfxRegistry")){
_c.dispose(this);
}
if(this.rawNode&&"__gfxObject__" in this.rawNode){
this.rawNode.__gfxObject__=null;
}
this.rawNode=null;
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
var m=this._getRealMatrix(),gm=_8;
return [gm.multiplyPoint(m,b.x,b.y),gm.multiplyPoint(m,b.x+b.width,b.y),gm.multiplyPoint(m,b.x+b.width,b.y+b.height),gm.multiplyPoint(m,b.x,b.y+b.height)];
},getEventSource:function(){
return this.rawNode;
},setClip:function(_e){
this.clip=_e;
},getClip:function(){
return this.clip;
},setShape:function(_f){
this.shape=g.makeParameters(this.shape,_f);
this.bbox=null;
return this;
},setFill:function(_10){
if(!_10){
this.fillStyle=null;
return this;
}
var f=null;
if(typeof (_10)=="object"&&"type" in _10){
switch(_10.type){
case "linear":
f=g.makeParameters(g.defaultLinearGradient,_10);
break;
case "radial":
f=g.makeParameters(g.defaultRadialGradient,_10);
break;
case "pattern":
f=g.makeParameters(g.defaultPattern,_10);
break;
}
}else{
f=g.normalizeColor(_10);
}
this.fillStyle=f;
return this;
},setStroke:function(_11){
if(!_11){
this.strokeStyle=null;
return this;
}
if(typeof _11=="string"||_1.isArray(_11)||_11 instanceof _7){
_11={color:_11};
}
var s=this.strokeStyle=g.makeParameters(g.defaultStroke,_11);
s.color=g.normalizeColor(s.color);
return this;
},setTransform:function(_12){
this.matrix=_8.clone(_12?_8.normalize(_12):_8.identity);
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
},applyRightTransform:function(_13){
return _13?this.setTransform([this.matrix,_13]):this;
},applyLeftTransform:function(_14){
return _14?this.setTransform([_14,this.matrix]):this;
},applyTransform:function(_15){
return _15?this.setTransform([this.matrix,_15]):this;
},removeShape:function(_16){
if(this.parent){
this.parent.remove(this,_16);
}
return this;
},_setParent:function(_17,_18){
this.parent=_17;
return this._updateParentMatrix(_18);
},_updateParentMatrix:function(_19){
this.parentMatrix=_19?_8.clone(_19):null;
return this._applyTransform();
},_getRealMatrix:function(){
var m=this.matrix;
var p=this.parent;
while(p){
if(p.matrix){
m=_8.multiply(p.matrix,m);
}
p=p.parent;
}
return m;
}});
_c._eventsProcessing={on:function(_1a,_1b){
return on(this.getEventSource(),_1a,_c.fixCallback(this,g.fixTarget,_1b));
},connect:function(_1c,_1d,_1e){
if(_1c.substring(0,2)=="on"){
_1c=_1c.substring(2);
}
return this.on(_1c,_1e?_1.hitch(_1d,_1e):_1d);
},disconnect:function(_1f){
return _1f.remove();
}};
_c.fixCallback=function(_20,_21,_22,_23){
if(!_23){
_23=_22;
_22=null;
}
if(_1.isString(_23)){
_22=_22||_3.global;
if(!_22[_23]){
throw (["dojox.gfx.shape.fixCallback: scope[\"",_23,"\"] is null (scope=\"",_22,"\")"].join(""));
}
return function(e){
return _21(e,_20)?_22[_23].apply(_22,arguments||[]):undefined;
};
}
return !_22?function(e){
return _21(e,_20)?_23.apply(_22,arguments):undefined;
}:function(e){
return _21(e,_20)?_23.apply(_22,arguments||[]):undefined;
};
};
_1.extend(_c.Shape,_c._eventsProcessing);
_c.Container={_init:function(){
this.children=[];
this._batch=0;
},openBatch:function(){
return this;
},closeBatch:function(){
return this;
},add:function(_24){
var _25=_24.getParent();
if(_25){
_25.remove(_24,true);
}
this.children.push(_24);
return _24._setParent(this,this._getRealMatrix());
},remove:function(_26,_27){
for(var i=0;i<this.children.length;++i){
if(this.children[i]==_26){
if(_27){
}else{
_26.parent=null;
_26.parentMatrix=null;
}
_9(this.children,i);
break;
}
}
return this;
},clear:function(_28){
var _29;
for(var i=0;i<this.children.length;++i){
_29=this.children[i];
_29.parent=null;
_29.parentMatrix=null;
if(_28){
_29.destroy();
}
}
this.children=[];
return this;
},getBoundingBox:function(){
if(this.children){
var _2a=null;
_5.forEach(this.children,function(_2b){
var bb=_2b.getBoundingBox();
if(bb){
var ct=_2b.getTransform();
if(ct){
bb=_8.multiplyRectangle(ct,bb);
}
if(_2a){
_2a.x=Math.min(_2a.x,bb.x);
_2a.y=Math.min(_2a.y,bb.y);
_2a.endX=Math.max(_2a.endX,bb.x+bb.width);
_2a.endY=Math.max(_2a.endY,bb.y+bb.height);
}else{
_2a={x:bb.x,y:bb.y,endX:bb.x+bb.width,endY:bb.y+bb.height};
}
}
});
if(_2a){
_2a.width=_2a.endX-_2a.x;
_2a.height=_2a.endY-_2a.y;
}
return _2a;
}
return null;
},_moveChildToFront:function(_2c){
for(var i=0;i<this.children.length;++i){
if(this.children[i]==_2c){
_9(this.children,i);
this.children.push(_2c);
break;
}
}
return this;
},_moveChildToBack:function(_2d){
for(var i=0;i<this.children.length;++i){
if(this.children[i]==_2d){
_9(this.children,i);
this.children.unshift(_2d);
break;
}
}
return this;
}};
_c.Surface=_2("dojox.gfx.shape.Surface",null,{constructor:function(){
this.rawNode=null;
this._parent=null;
this._nodes=[];
this._events=[];
},destroy:function(){
_5.forEach(this._nodes,_6.destroy);
this._nodes=[];
_5.forEach(this._events,function(h){
if(h){
h.remove();
}
});
this._events=[];
this.rawNode=null;
if(_4("ie")){
while(this._parent.lastChild){
_6.destroy(this._parent.lastChild);
}
}else{
this._parent.innerHTML="";
}
this._parent=null;
},getEventSource:function(){
return this.rawNode;
},_getRealMatrix:function(){
return null;
},isLoaded:true,onLoad:function(_2e){
},whenLoaded:function(_2f,_30){
var f=_1.hitch(_2f,_30);
if(this.isLoaded){
f(this);
}else{
on.once(this,"load",function(_31){
f(_31);
});
}
}});
_1.extend(_c.Surface,_c._eventsProcessing);
_c.Rect=_2("dojox.gfx.shape.Rect",_c.Shape,{constructor:function(_32){
this.shape=g.getDefault("Rect");
this.rawNode=_32;
},getBoundingBox:function(){
return this.shape;
}});
_c.Ellipse=_2("dojox.gfx.shape.Ellipse",_c.Shape,{constructor:function(_33){
this.shape=g.getDefault("Ellipse");
this.rawNode=_33;
},getBoundingBox:function(){
if(!this.bbox){
var _34=this.shape;
this.bbox={x:_34.cx-_34.rx,y:_34.cy-_34.ry,width:2*_34.rx,height:2*_34.ry};
}
return this.bbox;
}});
_c.Circle=_2("dojox.gfx.shape.Circle",_c.Shape,{constructor:function(_35){
this.shape=g.getDefault("Circle");
this.rawNode=_35;
},getBoundingBox:function(){
if(!this.bbox){
var _36=this.shape;
this.bbox={x:_36.cx-_36.r,y:_36.cy-_36.r,width:2*_36.r,height:2*_36.r};
}
return this.bbox;
}});
_c.Line=_2("dojox.gfx.shape.Line",_c.Shape,{constructor:function(_37){
this.shape=g.getDefault("Line");
this.rawNode=_37;
},getBoundingBox:function(){
if(!this.bbox){
var _38=this.shape;
this.bbox={x:Math.min(_38.x1,_38.x2),y:Math.min(_38.y1,_38.y2),width:Math.abs(_38.x2-_38.x1),height:Math.abs(_38.y2-_38.y1)};
}
return this.bbox;
}});
_c.Polyline=_2("dojox.gfx.shape.Polyline",_c.Shape,{constructor:function(_39){
this.shape=g.getDefault("Polyline");
this.rawNode=_39;
},setShape:function(_3a,_3b){
if(_3a&&_3a instanceof Array){
this.inherited(arguments,[{points:_3a}]);
if(_3b&&this.shape.points.length){
this.shape.points.push(this.shape.points[0]);
}
}else{
this.inherited(arguments,[_3a]);
}
return this;
},_normalizePoints:function(){
var p=this.shape.points,l=p&&p.length;
if(l&&typeof p[0]=="number"){
var _3c=[];
for(var i=0;i<l;i+=2){
_3c.push({x:p[i],y:p[i+1]});
}
this.shape.points=_3c;
}
},getBoundingBox:function(){
if(!this.bbox&&this.shape.points.length){
var p=this.shape.points;
var l=p.length;
var t=p[0];
var _3d={l:t.x,t:t.y,r:t.x,b:t.y};
for(var i=1;i<l;++i){
t=p[i];
if(_3d.l>t.x){
_3d.l=t.x;
}
if(_3d.r<t.x){
_3d.r=t.x;
}
if(_3d.t>t.y){
_3d.t=t.y;
}
if(_3d.b<t.y){
_3d.b=t.y;
}
}
this.bbox={x:_3d.l,y:_3d.t,width:_3d.r-_3d.l,height:_3d.b-_3d.t};
}
return this.bbox;
}});
_c.Image=_2("dojox.gfx.shape.Image",_c.Shape,{constructor:function(_3e){
this.shape=g.getDefault("Image");
this.rawNode=_3e;
},getBoundingBox:function(){
return this.shape;
},setStroke:function(){
return this;
},setFill:function(){
return this;
}});
_c.Text=_2(_c.Shape,{constructor:function(_3f){
this.fontStyle=null;
this.shape=g.getDefault("Text");
this.rawNode=_3f;
},getFont:function(){
return this.fontStyle;
},setFont:function(_40){
this.fontStyle=typeof _40=="string"?g.splitFontString(_40):g.makeParameters(g.defaultFont,_40);
this._setFont();
return this;
},getBoundingBox:function(){
var _41=null,s=this.getShape();
if(s.text){
_41=g._base._computeTextBoundingBox(this);
}
return _41;
}});
_c.Creator={createShape:function(_42){
switch(_42.type){
case g.defaultPath.type:
return this.createPath(_42);
case g.defaultRect.type:
return this.createRect(_42);
case g.defaultCircle.type:
return this.createCircle(_42);
case g.defaultEllipse.type:
return this.createEllipse(_42);
case g.defaultLine.type:
return this.createLine(_42);
case g.defaultPolyline.type:
return this.createPolyline(_42);
case g.defaultImage.type:
return this.createImage(_42);
case g.defaultText.type:
return this.createText(_42);
case g.defaultTextPath.type:
return this.createTextPath(_42);
}
return null;
},createGroup:function(){
return this.createObject(g.Group);
},createRect:function(_43){
return this.createObject(g.Rect,_43);
},createEllipse:function(_44){
return this.createObject(g.Ellipse,_44);
},createCircle:function(_45){
return this.createObject(g.Circle,_45);
},createLine:function(_46){
return this.createObject(g.Line,_46);
},createPolyline:function(_47){
return this.createObject(g.Polyline,_47);
},createImage:function(_48){
return this.createObject(g.Image,_48);
},createText:function(_49){
return this.createObject(g.Text,_49);
},createPath:function(_4a){
return this.createObject(g.Path,_4a);
},createTextPath:function(_4b){
return this.createObject(g.TextPath,{}).setText(_4b);
},createObject:function(_4c,_4d){
return null;
}};
return _c;
});
