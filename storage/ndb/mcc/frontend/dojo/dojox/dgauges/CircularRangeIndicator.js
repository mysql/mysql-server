//>>built
define("dojox/dgauges/CircularRangeIndicator",["dojo/_base/declare","./ScaleIndicatorBase","./_circularUtils","dojo/_base/event"],function(_1,_2,_3,_4){
return _1("dojox.dgauges.CircularRangeIndicator",_2,{start:0,radius:NaN,startThickness:6,endThickness:6,fill:null,stroke:null,constructor:function(){
this.indicatorShapeFunc=null;
this.fill=[255,120,0];
this.stroke={color:"black",width:0.2};
this.interactionMode="none";
this.addInvalidatingProperties(["start","radius","startThickness","endThickness","fill","stroke"]);
},_interpolateColor:function(_5,_6,n){
var fr=(_5>>16)&255;
var fg=(_5>>8)&255;
var fb=_5&255;
var tr=(_6>>16)&255;
var tg=(_6>>8)&255;
var tb=_6&255;
var r=((1-n)*fr+n*tr)&255;
var g=((1-n)*fg+n*tg)&255;
var b=((1-n)*fb+n*tb)&255;
return r<<16|g<<8|b;
},_colorsInterpolation:function(_7,_8,_9){
var _a=[];
var _b=0;
for(var i=0;i<_7.length-1;i++){
_b=(_8[i+1]-_8[i])*_9;
_b=Math.round(_b);
_a=_a.concat(_colorInterpolation(_7[i],_7[i+1],_b));
}
return _a;
},_alphasInterpolation:function(_c,_d,_e){
var _f=[];
var _10=0;
for(var i=0;i<_c.length-1;i++){
_10=(_d[i+1]-_d[i])*_e;
_10=Math.round(_10);
_f=_f.concat(_alphaInterpolation(_c[i],_c[i+1],_10));
}
return _f;
},_alphaInterpolation:function(c1,c2,len){
var _11=(c2-c1)/(len-1);
var ret=[];
for(var i=0;i<len;i++){
ret.push(c1+i*_11);
}
return ret;
},_colorInterpolation:function(c1,c2,len){
var ret=[];
for(var i=0;i<len;i++){
ret.push(_interpolateColor(c1,c2,i/(len-1)));
}
return ret;
},_getEntriesFor:function(_12,_13){
var ret=[];
var e;
var val;
for(var i=0;i<_12.length;i++){
e=_12[i];
if(e[_13]==null||isNaN(e[_13])){
val=i/(_12.length-1);
}else{
val=e[_13];
}
ret.push(val);
}
return ret;
},_drawColorTrack:function(g,ox,oy,_14,_15,_16,_17,_18,_19,_1a,_1b,_1c){
var _1d=0.05;
var _1e;
_1e=6.28318530718-_3.computeAngle(_16,_17,_15);
if(!isNaN(_1c)){
var _1f=_3.computeAngle(_16,_1c,_15);
_19*=_1f/_1e;
_1e=_1f;
}
var _20=Math.max(2,Math.floor(_1e/_1d));
_1d=_1e/_20;
var _21;
var _22;
var _23=0;
var _24=0;
var px;
var py;
_21=-_18;
_22=0;
_24=(_18-_19)/_20;
var _25;
var i;
if(_15=="clockwise"){
_1d=-_1d;
}
var gp=[];
px=ox+Math.cos(_16)*(_14+_21);
py=oy-Math.sin(_16)*(_14+_21);
gp.push(px,py);
for(i=0;i<_20;i++){
_25=_16+i*_1d;
px=ox+Math.cos(_25+_1d)*(_14+_21+i*_24);
py=oy-Math.sin(_25+_1d)*(_14+_21+i*_24);
gp.push(px,py);
}
if(isNaN(_25)){
_25=_16;
}
px=ox+Math.cos(_25+_1d)*(_14+_22+(_20-1)*_23);
py=oy-Math.sin(_25+_1d)*(_14+_22+(_20-1)*_23);
gp.push(px,py);
for(i=_20-1;i>=0;i--){
_25=_16+i*_1d;
px=ox+Math.cos(_25+_1d)*(_14+_22+i*_23);
py=oy-Math.sin(_25+_1d)*(_14+_22+i*_23);
gp.push(px,py);
}
px=ox+Math.cos(_16)*(_14+_22);
py=oy-Math.sin(_16)*(_14+_22);
gp.push(px,py);
px=ox+Math.cos(_16)*(_14+_21);
py=oy-Math.sin(_16)*(_14+_21);
gp.push(px,py);
g.createPolyline(gp).setFill(_1a).setStroke(_1b);
},refreshRendering:function(){
this.inherited(arguments);
var g=this._gfxGroup;
g.clear();
var ox=this.scale.originX;
var oy=this.scale.originY;
var _26=isNaN(this.radius)?this.scale.radius:this.radius;
var _27=this.scale.orientation;
var _28=_3.toRadians(360-this.scale.positionForValue(this.start));
var v=isNaN(this._transitionValue)?this.value:this._transitionValue;
var _29=_3.toRadians(360-this.scale.positionForValue(v));
var _2a=this.startThickness;
var _2b=this.endThickness;
var _2c=NaN;
this._drawColorTrack(g,ox,oy,_26,_27,_28,_29,_2a,_2b,this.fill,this.stroke,_2c);
},_onMouseDown:function(_2d){
this.inherited(arguments);
var _2e=this.scale._gauge._gaugeToPage(this.scale.originX,this.scale.originY);
var _2f=((Math.atan2(_2d.pageY-_2e.y,_2d.pageX-_2e.x))*180)/(Math.PI);
this.set("value",this.scale.valueForPosition(_2f));
_4.stop(_2d);
},_onMouseMove:function(_30){
this.inherited(arguments);
var _31=this.scale._gauge._gaugeToPage(this.scale.originX,this.scale.originY);
var _32=((Math.atan2(_30.pageY-_31.y,_30.pageX-_31.x))*180)/(Math.PI);
this.set("value",this.scale.valueForPosition(_32));
}});
});
