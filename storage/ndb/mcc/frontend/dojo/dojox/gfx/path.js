//>>built
define("dojox/gfx/path",["./_base","dojo/_base/lang","dojo/_base/declare","./matrix","./shape"],function(g,_1,_2,_3,_4){
var _5=_2("dojox.gfx.path.Path",_4.Shape,{constructor:function(_6){
this.shape=_1.clone(g.defaultPath);
this.segments=[];
this.tbbox=null;
this.absolute=true;
this.last={};
this.rawNode=_6;
this.segmented=false;
},setAbsoluteMode:function(_7){
this._confirmSegmented();
this.absolute=typeof _7=="string"?(_7=="absolute"):_7;
return this;
},getAbsoluteMode:function(){
this._confirmSegmented();
return this.absolute;
},getBoundingBox:function(){
this._confirmSegmented();
return (this.bbox&&("l" in this.bbox))?{x:this.bbox.l,y:this.bbox.t,width:this.bbox.r-this.bbox.l,height:this.bbox.b-this.bbox.t}:null;
},_getRealBBox:function(){
this._confirmSegmented();
if(this.tbbox){
return this.tbbox;
}
var _8=this.bbox,_3=this._getRealMatrix();
this.bbox=null;
for(var i=0,_9=this.segments.length;i<_9;++i){
this._updateWithSegment(this.segments[i],_3);
}
var t=this.bbox;
this.bbox=_8;
this.tbbox=t?[{x:t.l,y:t.t},{x:t.r,y:t.t},{x:t.r,y:t.b},{x:t.l,y:t.b}]:null;
return this.tbbox;
},getLastPosition:function(){
this._confirmSegmented();
return "x" in this.last?this.last:null;
},_applyTransform:function(){
this.tbbox=null;
return this.inherited(arguments);
},_updateBBox:function(x,y,m){
if(m){
var t=_3.multiplyPoint(m,x,y);
x=t.x;
y=t.y;
}
if(this.bbox&&("l" in this.bbox)){
if(this.bbox.l>x){
this.bbox.l=x;
}
if(this.bbox.r<x){
this.bbox.r=x;
}
if(this.bbox.t>y){
this.bbox.t=y;
}
if(this.bbox.b<y){
this.bbox.b=y;
}
}else{
this.bbox={l:x,b:y,r:x,t:y};
}
},_updateWithSegment:function(_a,_b){
var n=_a.args,l=n.length,i;
switch(_a.action){
case "M":
case "L":
case "C":
case "S":
case "Q":
case "T":
for(i=0;i<l;i+=2){
this._updateBBox(n[i],n[i+1],_b);
}
this.last.x=n[l-2];
this.last.y=n[l-1];
this.absolute=true;
break;
case "H":
for(i=0;i<l;++i){
this._updateBBox(n[i],this.last.y,_b);
}
this.last.x=n[l-1];
this.absolute=true;
break;
case "V":
for(i=0;i<l;++i){
this._updateBBox(this.last.x,n[i],_b);
}
this.last.y=n[l-1];
this.absolute=true;
break;
case "m":
var _c=0;
if(!("x" in this.last)){
this._updateBBox(this.last.x=n[0],this.last.y=n[1],_b);
_c=2;
}
for(i=_c;i<l;i+=2){
this._updateBBox(this.last.x+=n[i],this.last.y+=n[i+1],_b);
}
this.absolute=false;
break;
case "l":
case "t":
for(i=0;i<l;i+=2){
this._updateBBox(this.last.x+=n[i],this.last.y+=n[i+1],_b);
}
this.absolute=false;
break;
case "h":
for(i=0;i<l;++i){
this._updateBBox(this.last.x+=n[i],this.last.y,_b);
}
this.absolute=false;
break;
case "v":
for(i=0;i<l;++i){
this._updateBBox(this.last.x,this.last.y+=n[i],_b);
}
this.absolute=false;
break;
case "c":
for(i=0;i<l;i+=6){
this._updateBBox(this.last.x+n[i],this.last.y+n[i+1],_b);
this._updateBBox(this.last.x+n[i+2],this.last.y+n[i+3],_b);
this._updateBBox(this.last.x+=n[i+4],this.last.y+=n[i+5],_b);
}
this.absolute=false;
break;
case "s":
case "q":
for(i=0;i<l;i+=4){
this._updateBBox(this.last.x+n[i],this.last.y+n[i+1],_b);
this._updateBBox(this.last.x+=n[i+2],this.last.y+=n[i+3],_b);
}
this.absolute=false;
break;
case "A":
for(i=0;i<l;i+=7){
this._updateBBox(n[i+5],n[i+6],_b);
}
this.last.x=n[l-2];
this.last.y=n[l-1];
this.absolute=true;
break;
case "a":
for(i=0;i<l;i+=7){
this._updateBBox(this.last.x+=n[i+5],this.last.y+=n[i+6],_b);
}
this.absolute=false;
break;
}
var _d=[_a.action];
for(i=0;i<l;++i){
_d.push(g.formatNumber(n[i],true));
}
if(typeof this.shape.path=="string"){
this.shape.path+=_d.join("");
}else{
for(i=0,l=_d.length;i<l;++i){
this.shape.path.push(_d[i]);
}
}
},_validSegments:{m:2,l:2,h:1,v:1,c:6,s:4,q:4,t:2,a:7,z:0},_pushSegment:function(_e,_f){
this.tbbox=null;
var _10=this._validSegments[_e.toLowerCase()],_11;
if(typeof _10=="number"){
if(_10){
if(_f.length>=_10){
_11={action:_e,args:_f.slice(0,_f.length-_f.length%_10)};
this.segments.push(_11);
this._updateWithSegment(_11);
}
}else{
_11={action:_e,args:[]};
this.segments.push(_11);
this._updateWithSegment(_11);
}
}
},_collectArgs:function(_12,_13){
for(var i=0;i<_13.length;++i){
var t=_13[i];
if(typeof t=="boolean"){
_12.push(t?1:0);
}else{
if(typeof t=="number"){
_12.push(t);
}else{
if(t instanceof Array){
this._collectArgs(_12,t);
}else{
if("x" in t&&"y" in t){
_12.push(t.x,t.y);
}
}
}
}
}
},moveTo:function(){
this._confirmSegmented();
var _14=[];
this._collectArgs(_14,arguments);
this._pushSegment(this.absolute?"M":"m",_14);
return this;
},lineTo:function(){
this._confirmSegmented();
var _15=[];
this._collectArgs(_15,arguments);
this._pushSegment(this.absolute?"L":"l",_15);
return this;
},hLineTo:function(){
this._confirmSegmented();
var _16=[];
this._collectArgs(_16,arguments);
this._pushSegment(this.absolute?"H":"h",_16);
return this;
},vLineTo:function(){
this._confirmSegmented();
var _17=[];
this._collectArgs(_17,arguments);
this._pushSegment(this.absolute?"V":"v",_17);
return this;
},curveTo:function(){
this._confirmSegmented();
var _18=[];
this._collectArgs(_18,arguments);
this._pushSegment(this.absolute?"C":"c",_18);
return this;
},smoothCurveTo:function(){
this._confirmSegmented();
var _19=[];
this._collectArgs(_19,arguments);
this._pushSegment(this.absolute?"S":"s",_19);
return this;
},qCurveTo:function(){
this._confirmSegmented();
var _1a=[];
this._collectArgs(_1a,arguments);
this._pushSegment(this.absolute?"Q":"q",_1a);
return this;
},qSmoothCurveTo:function(){
this._confirmSegmented();
var _1b=[];
this._collectArgs(_1b,arguments);
this._pushSegment(this.absolute?"T":"t",_1b);
return this;
},arcTo:function(){
this._confirmSegmented();
var _1c=[];
this._collectArgs(_1c,arguments);
this._pushSegment(this.absolute?"A":"a",_1c);
return this;
},closePath:function(){
this._confirmSegmented();
this._pushSegment("Z",[]);
return this;
},_confirmSegmented:function(){
if(!this.segmented){
var _1d=this.shape.path;
this.shape.path=[];
this._setPath(_1d);
this.shape.path=this.shape.path.join("");
this.segmented=true;
}
},_setPath:function(_1e){
var p=_1.isArray(_1e)?_1e:_1e.match(g.pathSvgRegExp);
this.segments=[];
this.absolute=true;
this.bbox={};
this.last={};
if(!p){
return;
}
var _1f="",_20=[],l=p.length;
for(var i=0;i<l;++i){
var t=p[i],x=parseFloat(t);
if(isNaN(x)){
if(_1f){
this._pushSegment(_1f,_20);
}
_20=[];
_1f=t;
}else{
_20.push(x);
}
}
this._pushSegment(_1f,_20);
},setShape:function(_21){
this.inherited(arguments,[typeof _21=="string"?{path:_21}:_21]);
this.segmented=false;
this.segments=[];
if(!g.lazyPathSegmentation){
this._confirmSegmented();
}
return this;
},_2PI:Math.PI*2});
var _22=_2("dojox.gfx.path.TextPath",_5,{constructor:function(_23){
if(!("text" in this)){
this.text=_1.clone(g.defaultTextPath);
}
if(!("fontStyle" in this)){
this.fontStyle=_1.clone(g.defaultFont);
}
},getText:function(){
return this.text;
},setText:function(_24){
this.text=g.makeParameters(this.text,typeof _24=="string"?{text:_24}:_24);
this._setText();
return this;
},getFont:function(){
return this.fontStyle;
},setFont:function(_25){
this.fontStyle=typeof _25=="string"?g.splitFontString(_25):g.makeParameters(g.defaultFont,_25);
this._setFont();
return this;
}});
return g.path={Path:_5,TextPath:_22};
});
