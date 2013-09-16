//>>built
define("dojox/color/Palette",["dojo/_base/kernel","../main","dojo/_base/lang","dojo/_base/array","./_base"],function(_1,_2,_3,_4,_5){
_5.Palette=function(_6){
this.colors=[];
if(_6 instanceof _5.Palette){
this.colors=_6.colors.slice(0);
}else{
if(_6 instanceof _5.Color){
this.colors=[null,null,_6,null,null];
}else{
if(_3.isArray(_6)){
this.colors=_4.map(_6.slice(0),function(_7){
if(_3.isString(_7)){
return new _5.Color(_7);
}
return _7;
});
}else{
if(_3.isString(_6)){
this.colors=[null,null,new _5.Color(_6),null,null];
}
}
}
}
};
function _8(p,_9,_a){
var _b=new _5.Palette();
_b.colors=[];
_4.forEach(p.colors,function(_c){
var r=(_9=="dr")?_c.r+_a:_c.r,g=(_9=="dg")?_c.g+_a:_c.g,b=(_9=="db")?_c.b+_a:_c.b,a=(_9=="da")?_c.a+_a:_c.a;
_b.colors.push(new _5.Color({r:Math.min(255,Math.max(0,r)),g:Math.min(255,Math.max(0,g)),b:Math.min(255,Math.max(0,b)),a:Math.min(1,Math.max(0,a))}));
});
return _b;
};
function _d(p,_e,_f){
var ret=new _5.Palette();
ret.colors=[];
_4.forEach(p.colors,function(_10){
var o=_10.toCmy(),c=(_e=="dc")?o.c+_f:o.c,m=(_e=="dm")?o.m+_f:o.m,y=(_e=="dy")?o.y+_f:o.y;
ret.colors.push(_5.fromCmy(Math.min(100,Math.max(0,c)),Math.min(100,Math.max(0,m)),Math.min(100,Math.max(0,y))));
});
return ret;
};
function _11(p,_12,val){
var ret=new _5.Palette();
ret.colors=[];
_4.forEach(p.colors,function(_13){
var o=_13.toCmyk(),c=(_12=="dc")?o.c+val:o.c,m=(_12=="dm")?o.m+val:o.m,y=(_12=="dy")?o.y+val:o.y,k=(_12=="dk")?o.b+val:o.b;
ret.colors.push(_5.fromCmyk(Math.min(100,Math.max(0,c)),Math.min(100,Math.max(0,m)),Math.min(100,Math.max(0,y)),Math.min(100,Math.max(0,k))));
});
return ret;
};
function _14(p,_15,val){
var ret=new _5.Palette();
ret.colors=[];
_4.forEach(p.colors,function(_16){
var o=_16.toHsl(),h=(_15=="dh")?o.h+val:o.h,s=(_15=="ds")?o.s+val:o.s,l=(_15=="dl")?o.l+val:o.l;
ret.colors.push(_5.fromHsl(h%360,Math.min(100,Math.max(0,s)),Math.min(100,Math.max(0,l))));
});
return ret;
};
function _17(p,_18,val){
var ret=new _5.Palette();
ret.colors=[];
_4.forEach(p.colors,function(_19){
var o=_19.toHsv(),h=(_18=="dh")?o.h+val:o.h,s=(_18=="ds")?o.s+val:o.s,v=(_18=="dv")?o.v+val:o.v;
ret.colors.push(_5.fromHsv(h%360,Math.min(100,Math.max(0,s)),Math.min(100,Math.max(0,v))));
});
return ret;
};
function _1a(val,low,_1b){
return _1b-((_1b-val)*((_1b-low)/_1b));
};
_3.extend(_5.Palette,{transform:function(_1c){
var fn=_8;
if(_1c.use){
var use=_1c.use.toLowerCase();
if(use.indexOf("hs")==0){
if(use.charAt(2)=="l"){
fn=_14;
}else{
fn=_17;
}
}else{
if(use.indexOf("cmy")==0){
if(use.charAt(3)=="k"){
fn=_11;
}else{
fn=_d;
}
}
}
}else{
if("dc" in _1c||"dm" in _1c||"dy" in _1c){
if("dk" in _1c){
fn=_11;
}else{
fn=_d;
}
}else{
if("dh" in _1c||"ds" in _1c){
if("dv" in _1c){
fn=_17;
}else{
fn=_14;
}
}
}
}
var _1d=this;
for(var p in _1c){
if(p=="use"){
continue;
}
_1d=fn(_1d,p,_1c[p]);
}
return _1d;
},clone:function(){
return new _5.Palette(this);
}});
_3.mixin(_5.Palette,{generators:{analogous:function(_1e){
var _1f=_1e.high||60,low=_1e.low||18,_20=_3.isString(_1e.base)?new _5.Color(_1e.base):_1e.base,hsv=_20.toHsv();
var h=[(hsv.h+low+360)%360,(hsv.h+Math.round(low/2)+360)%360,hsv.h,(hsv.h-Math.round(_1f/2)+360)%360,(hsv.h-_1f+360)%360];
var s1=Math.max(10,(hsv.s<=95)?hsv.s+5:(100-(hsv.s-95))),s2=(hsv.s>1)?hsv.s-1:21-hsv.s,v1=(hsv.v>=92)?hsv.v-9:Math.max(hsv.v+9,20),v2=(hsv.v<=90)?Math.max(hsv.v+5,20):(95+Math.ceil((hsv.v-90)/2)),s=[s1,s2,hsv.s,s1,s1],v=[v1,v2,hsv.v,v1,v2];
return new _5.Palette(_4.map(h,function(hue,i){
return _5.fromHsv(hue,s[i],v[i]);
}));
},monochromatic:function(_21){
var _22=_3.isString(_21.base)?new _5.Color(_21.base):_21.base,hsv=_22.toHsv();
var s1=(hsv.s-30>9)?hsv.s-30:hsv.s+30,s2=hsv.s,v1=_1a(hsv.v,20,100),v2=(hsv.v-20>20)?hsv.v-20:hsv.v+60,v3=(hsv.v-50>20)?hsv.v-50:hsv.v+30;
return new _5.Palette([_5.fromHsv(hsv.h,s1,v1),_5.fromHsv(hsv.h,s2,v3),_22,_5.fromHsv(hsv.h,s1,v3),_5.fromHsv(hsv.h,s2,v2)]);
},triadic:function(_23){
var _24=_3.isString(_23.base)?new _5.Color(_23.base):_23.base,hsv=_24.toHsv();
var h1=(hsv.h+57+360)%360,h2=(hsv.h-157+360)%360,s1=(hsv.s>20)?hsv.s-10:hsv.s+10,s2=(hsv.s>90)?hsv.s-10:hsv.s+10,s3=(hsv.s>95)?hsv.s-5:hsv.s+5,v1=(hsv.v-20>20)?hsv.v-20:hsv.v+20,v2=(hsv.v-30>20)?hsv.v-30:hsv.v+30,v3=(hsv.v-30>70)?hsv.v-30:hsv.v+30;
return new _5.Palette([_5.fromHsv(h1,s1,hsv.v),_5.fromHsv(hsv.h,s2,v2),_24,_5.fromHsv(h2,s2,v1),_5.fromHsv(h2,s3,v3)]);
},complementary:function(_25){
var _26=_3.isString(_25.base)?new _5.Color(_25.base):_25.base,hsv=_26.toHsv();
var h1=((hsv.h*2)+137<360)?(hsv.h*2)+137:Math.floor(hsv.h/2)-137,s1=Math.max(hsv.s-10,0),s2=_1a(hsv.s,10,100),s3=Math.min(100,hsv.s+20),v1=Math.min(100,hsv.v+30),v2=(hsv.v>20)?hsv.v-30:hsv.v+30;
return new _5.Palette([_5.fromHsv(hsv.h,s1,v1),_5.fromHsv(hsv.h,s2,v2),_26,_5.fromHsv(h1,s3,v2),_5.fromHsv(h1,hsv.s,hsv.v)]);
},splitComplementary:function(_27){
var _28=_3.isString(_27.base)?new _5.Color(_27.base):_27.base,_29=_27.da||30,hsv=_28.toHsv();
var _2a=((hsv.h*2)+137<360)?(hsv.h*2)+137:Math.floor(hsv.h/2)-137,h1=(_2a-_29+360)%360,h2=(_2a+_29)%360,s1=Math.max(hsv.s-10,0),s2=_1a(hsv.s,10,100),s3=Math.min(100,hsv.s+20),v1=Math.min(100,hsv.v+30),v2=(hsv.v>20)?hsv.v-30:hsv.v+30;
return new _5.Palette([_5.fromHsv(h1,s1,v1),_5.fromHsv(h1,s2,v2),_28,_5.fromHsv(h2,s3,v2),_5.fromHsv(h2,hsv.s,hsv.v)]);
},compound:function(_2b){
var _2c=_3.isString(_2b.base)?new _5.Color(_2b.base):_2b.base,hsv=_2c.toHsv();
var h1=((hsv.h*2)+18<360)?(hsv.h*2)+18:Math.floor(hsv.h/2)-18,h2=((hsv.h*2)+120<360)?(hsv.h*2)+120:Math.floor(hsv.h/2)-120,h3=((hsv.h*2)+99<360)?(hsv.h*2)+99:Math.floor(hsv.h/2)-99,s1=(hsv.s-40>10)?hsv.s-40:hsv.s+40,s2=(hsv.s-10>80)?hsv.s-10:hsv.s+10,s3=(hsv.s-25>10)?hsv.s-25:hsv.s+25,v1=(hsv.v-40>10)?hsv.v-40:hsv.v+40,v2=(hsv.v-20>80)?hsv.v-20:hsv.v+20,v3=Math.max(hsv.v,20);
return new _5.Palette([_5.fromHsv(h1,s1,v1),_5.fromHsv(h1,s2,v2),_2c,_5.fromHsv(h2,s3,v3),_5.fromHsv(h3,s2,v2)]);
},shades:function(_2d){
var _2e=_3.isString(_2d.base)?new _5.Color(_2d.base):_2d.base,hsv=_2e.toHsv();
var s=(hsv.s==100&&hsv.v==0)?0:hsv.s,v1=(hsv.v-50>20)?hsv.v-50:hsv.v+30,v2=(hsv.v-25>=20)?hsv.v-25:hsv.v+55,v3=(hsv.v-75>=20)?hsv.v-75:hsv.v+5,v4=Math.max(hsv.v-10,20);
return new _5.Palette([new _5.fromHsv(hsv.h,s,v1),new _5.fromHsv(hsv.h,s,v2),_2e,new _5.fromHsv(hsv.h,s,v3),new _5.fromHsv(hsv.h,s,v4)]);
}},generate:function(_2f,_30){
if(_3.isFunction(_30)){
return _30({base:_2f});
}else{
if(_5.Palette.generators[_30]){
return _5.Palette.generators[_30]({base:_2f});
}
}
throw new Error("dojox.color.Palette.generate: the specified generator ('"+_30+"') does not exist.");
}});
return _5.Palette;
});
