//>>built
define("dojox/gfx/fx",["dojo/_base/lang","./_base","./matrix","dojo/_base/Color","dojo/_base/array","dojo/_base/fx","dojo/_base/connect","dojo/sniff"],function(_1,g,m,_2,_3,fx,_4,_5){
var _6=g.fx={};
function _7(_8,_9){
this.start=_8,this.end=_9;
};
_7.prototype.getValue=function(r){
return (this.end-this.start)*r+this.start;
};
function _a(_b,_c,_d){
this.start=_b,this.end=_c;
this.units=_d;
};
_a.prototype.getValue=function(r){
return (this.end-this.start)*r+this.start+this.units;
};
function _e(_f,end){
this.start=_f,this.end=end;
this.temp=new _2();
};
_e.prototype.getValue=function(r){
return _2.blendColors(this.start,this.end,r,this.temp);
};
function _10(_11){
this.values=_11;
this.length=_11.length;
};
_10.prototype.getValue=function(r){
return this.values[Math.min(Math.floor(r*this.length),this.length-1)];
};
function _12(_13,def){
this.values=_13;
this.def=def?def:{};
};
_12.prototype.getValue=function(r){
var ret=_1.clone(this.def);
for(var i in this.values){
ret[i]=this.values[i].getValue(r);
}
return ret;
};
function _14(_15,_16){
this.stack=_15;
this.original=_16;
};
_14.prototype.getValue=function(r){
var ret=[];
_3.forEach(this.stack,function(t){
if(t instanceof m.Matrix2D){
ret.push(t);
return;
}
if(t.name=="original"&&this.original){
ret.push(this.original);
return;
}
if(t.name=="matrix"){
if((t.start instanceof m.Matrix2D)&&(t.end instanceof m.Matrix2D)){
var _17=new m.Matrix2D();
for(var p in t.start){
_17[p]=(t.end[p]-t.start[p])*r+t.start[p];
}
ret.push(_17);
}
return;
}
if(!(t.name in m)){
return;
}
var f=m[t.name];
if(typeof f!="function"){
ret.push(f);
return;
}
var val=_3.map(t.start,function(v,i){
return (t.end[i]-v)*r+v;
}),_18=f.apply(m,val);
if(_18 instanceof m.Matrix2D){
ret.push(_18);
}
},this);
return ret;
};
var _19=new _2(0,0,0,0);
function _1a(_1b,obj,_1c,def){
if(_1b.values){
return new _10(_1b.values);
}
var _1d,_1e,end;
if(_1b.start){
_1e=g.normalizeColor(_1b.start);
}else{
_1e=_1d=obj?(_1c?obj[_1c]:obj):def;
}
if(_1b.end){
end=g.normalizeColor(_1b.end);
}else{
if(!_1d){
_1d=obj?(_1c?obj[_1c]:obj):def;
}
end=_1d;
}
return new _e(_1e,end);
};
function _1f(_20,obj,_21,def){
if(_20.values){
return new _10(_20.values);
}
var _22,_23,end;
if(_20.start){
_23=_20.start;
}else{
_23=_22=obj?obj[_21]:def;
}
if(_20.end){
end=_20.end;
}else{
if(typeof _22!="number"){
_22=obj?obj[_21]:def;
}
end=_22;
}
return new _7(_23,end);
};
_6.animateStroke=function(_24){
if(!_24.easing){
_24.easing=fx._defaultEasing;
}
var _25=new fx.Animation(_24),_26=_24.shape,_27;
_4.connect(_25,"beforeBegin",_25,function(){
_27=_26.getStroke();
var _28=_24.color,_29={},_2a,_2b,end;
if(_28){
_29.color=_1a(_28,_27,"color",_19);
}
_28=_24.style;
if(_28&&_28.values){
_29.style=new _10(_28.values);
}
_28=_24.width;
if(_28){
_29.width=_1f(_28,_27,"width",1);
}
_28=_24.cap;
if(_28&&_28.values){
_29.cap=new _10(_28.values);
}
_28=_24.join;
if(_28){
if(_28.values){
_29.join=new _10(_28.values);
}else{
_2b=_28.start?_28.start:(_27&&_27.join||0);
end=_28.end?_28.end:(_27&&_27.join||0);
if(typeof _2b=="number"&&typeof end=="number"){
_29.join=new _7(_2b,end);
}
}
}
this.curve=new _12(_29,_27);
});
_4.connect(_25,"onAnimate",_26,"setStroke");
return _25;
};
_6.animateFill=function(_2c){
if(!_2c.easing){
_2c.easing=fx._defaultEasing;
}
var _2d=new fx.Animation(_2c),_2e=_2c.shape,_2f;
_4.connect(_2d,"beforeBegin",_2d,function(){
_2f=_2e.getFill();
var _30=_2c.color,_31={};
if(_30){
this.curve=_1a(_30,_2f,"",_19);
}
});
_4.connect(_2d,"onAnimate",_2e,"setFill");
return _2d;
};
_6.animateFont=function(_32){
if(!_32.easing){
_32.easing=fx._defaultEasing;
}
var _33=new fx.Animation(_32),_34=_32.shape,_35;
_4.connect(_33,"beforeBegin",_33,function(){
_35=_34.getFont();
var _36=_32.style,_37={},_38,_39,end;
if(_36&&_36.values){
_37.style=new _10(_36.values);
}
_36=_32.variant;
if(_36&&_36.values){
_37.variant=new _10(_36.values);
}
_36=_32.weight;
if(_36&&_36.values){
_37.weight=new _10(_36.values);
}
_36=_32.family;
if(_36&&_36.values){
_37.family=new _10(_36.values);
}
_36=_32.size;
if(_36&&_36.units){
_39=parseFloat(_36.start?_36.start:(_34.font&&_34.font.size||"0"));
end=parseFloat(_36.end?_36.end:(_34.font&&_34.font.size||"0"));
_37.size=new _a(_39,end,_36.units);
}
this.curve=new _12(_37,_35);
});
_4.connect(_33,"onAnimate",_34,"setFont");
return _33;
};
_6.animateTransform=function(_3a){
if(!_3a.easing){
_3a.easing=fx._defaultEasing;
}
var _3b=new fx.Animation(_3a),_3c=_3a.shape,_3d;
_4.connect(_3b,"beforeBegin",_3b,function(){
_3d=_3c.getTransform();
this.curve=new _14(_3a.transform,_3d);
});
_4.connect(_3b,"onAnimate",_3c,"setTransform");
if(g.renderer==="svg"&&(_5("ie")>=9||_5("ff"))){
var _3e=[_4.connect(_3b,"onBegin",_3b,function(){
var _3f=_3c.getParent();
while(_3f&&_3f.getParent){
_3f=_3f.getParent();
}
if(_3f){
_3c.__svgContainer=_3f.rawNode.parentNode;
_3c.__svgRoot=_3f.rawNode;
if(_3c.__svgRoot&&_3c.__svgRoot.getAttribute){
_3c.__svgWidth=parseInt(_3c.__svgRoot.getAttribute("width"),10);
if(isNaN(_3c.__svgWidth)){
delete _3c.__svgWidth;
}
}
}
}),_4.connect(_3b,"onAnimate",_3b,function(){
try{
if(_3c.__svgContainer){
var ov=_3c.__svgContainer.style.visibility;
_3c.__svgContainer.style.visibility="visible";
var _40=_3c.__svgContainer.offsetHeight;
_3c.__svgContainer.style.visibility=ov;
var _41=_3c.__svgWidth;
if(!isNaN(_41)){
try{
_3c.__svgRoot.setAttribute("width",_41-0.000005);
_3c.__svgRoot.setAttribute("width",_41);
}
catch(ignore){
}
}
}
}
catch(e){
}
}),_4.connect(_3b,"onEnd",_3b,function(){
_3.forEach(_3e,_4.disconnect);
if(_3c.__svgContainer){
var sn=_3c.__svgContainer;
if(sn.getAttribute("__gotVis")==null){
sn.setAttribute("__gotVis",true);
var ov=_3c.__svgContainer.style.visibility;
var _42=_3c.__svgRoot;
var _43=_3c.__svgWidth;
sn.style.visibility="visible";
setTimeout(function(){
try{
sn.style.visibility=ov;
sn.removeAttribute("__gotVis");
sn=null;
try{
if(!isNaN(_43)){
_42.setAttribute("width",_43-0.000005);
_42.setAttribute("width",_43);
}
}
catch(ignore){
}
}
catch(e){
}
},100);
}
}
delete _3c.__svgContainer;
delete _3c.__svgRoot;
delete _3c.__svgWidth;
})];
}
return _3b;
};
return _6;
});
