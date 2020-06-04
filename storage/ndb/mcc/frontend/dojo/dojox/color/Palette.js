//>>built
define("dojox/color/Palette",["dojo/_base/lang","dojo/_base/array","./_base"],function(_1,_2,_3){
_3.Palette=function(_4){
this.colors=[];
if(_4 instanceof _3.Palette){
this.colors=_4.colors.slice(0);
}else{
if(_4 instanceof _3.Color){
this.colors=[null,null,_4,null,null];
}else{
if(_1.isArray(_4)){
this.colors=_2.map(_4.slice(0),function(_5){
if(_1.isString(_5)){
return new _3.Color(_5);
}
return _5;
});
}else{
if(_1.isString(_4)){
this.colors=[null,null,new _3.Color(_4),null,null];
}
}
}
}
};
function _6(p,_7,_8){
var _9=new _3.Palette();
_9.colors=[];
_2.forEach(p.colors,function(_a){
var r=(_7=="dr")?_a.r+_8:_a.r,g=(_7=="dg")?_a.g+_8:_a.g,b=(_7=="db")?_a.b+_8:_a.b,a=(_7=="da")?_a.a+_8:_a.a;
_9.colors.push(new _3.Color({r:Math.min(255,Math.max(0,r)),g:Math.min(255,Math.max(0,g)),b:Math.min(255,Math.max(0,b)),a:Math.min(1,Math.max(0,a))}));
});
return _9;
};
function _b(p,_c,_d){
var _e=new _3.Palette();
_e.colors=[];
_2.forEach(p.colors,function(_f){
var o=_f.toCmy(),c=(_c=="dc")?o.c+_d:o.c,m=(_c=="dm")?o.m+_d:o.m,y=(_c=="dy")?o.y+_d:o.y;
_e.colors.push(_3.fromCmy(Math.min(100,Math.max(0,c)),Math.min(100,Math.max(0,m)),Math.min(100,Math.max(0,y))));
});
return _e;
};
function _10(p,_11,val){
var ret=new _3.Palette();
ret.colors=[];
_2.forEach(p.colors,function(_12){
var o=_12.toCmyk(),c=(_11=="dc")?o.c+val:o.c,m=(_11=="dm")?o.m+val:o.m,y=(_11=="dy")?o.y+val:o.y,k=(_11=="dk")?o.b+val:o.b;
ret.colors.push(_3.fromCmyk(Math.min(100,Math.max(0,c)),Math.min(100,Math.max(0,m)),Math.min(100,Math.max(0,y)),Math.min(100,Math.max(0,k))));
});
return ret;
};
function _13(p,_14,val){
var ret=new _3.Palette();
ret.colors=[];
_2.forEach(p.colors,function(_15){
var o=_15.toHsl(),h=(_14=="dh")?o.h+val:o.h,s=(_14=="ds")?o.s+val:o.s,l=(_14=="dl")?o.l+val:o.l;
ret.colors.push(_3.fromHsl(h%360,Math.min(100,Math.max(0,s)),Math.min(100,Math.max(0,l))));
});
return ret;
};
function _16(p,_17,val){
var ret=new _3.Palette();
ret.colors=[];
_2.forEach(p.colors,function(_18){
var o=_18.toHsv(),h=(_17=="dh")?o.h+val:o.h,s=(_17=="ds")?o.s+val:o.s,v=(_17=="dv")?o.v+val:o.v;
ret.colors.push(_3.fromHsv(h%360,Math.min(100,Math.max(0,s)),Math.min(100,Math.max(0,v))));
});
return ret;
};
function _19(val,low,_1a){
return _1a-((_1a-val)*((_1a-low)/_1a));
};
_1.extend(_3.Palette,{transform:function(_1b){
var fn=_6;
if(_1b.use){
var use=_1b.use.toLowerCase();
if(use.indexOf("hs")==0){
if(use.charAt(2)=="l"){
fn=_13;
}else{
fn=_16;
}
}else{
if(use.indexOf("cmy")==0){
if(use.charAt(3)=="k"){
fn=_10;
}else{
fn=_b;
}
}
}
}else{
if("dc" in _1b||"dm" in _1b||"dy" in _1b){
if("dk" in _1b){
fn=_10;
}else{
fn=_b;
}
}else{
if("dh" in _1b||"ds" in _1b){
if("dv" in _1b){
fn=_16;
}else{
fn=_13;
}
}
}
}
var _1c=this;
for(var p in _1b){
if(p=="use"){
continue;
}
_1c=fn(_1c,p,_1b[p]);
}
return _1c;
},clone:function(){
return new _3.Palette(this);
}});
_1.mixin(_3.Palette,{generators:{analogous:function(_1d){
var _1e=_1d.high||60,low=_1d.low||18,_1f=_1.isString(_1d.base)?new _3.Color(_1d.base):_1d.base,hsv=_1f.toHsv();
var h=[(hsv.h+low+360)%360,(hsv.h+Math.round(low/2)+360)%360,hsv.h,(hsv.h-Math.round(_1e/2)+360)%360,(hsv.h-_1e+360)%360];
var s1=Math.max(10,(hsv.s<=95)?hsv.s+5:(100-(hsv.s-95))),s2=(hsv.s>1)?hsv.s-1:21-hsv.s,v1=(hsv.v>=92)?hsv.v-9:Math.max(hsv.v+9,20),v2=(hsv.v<=90)?Math.max(hsv.v+5,20):(95+Math.ceil((hsv.v-90)/2)),s=[s1,s2,hsv.s,s1,s1],v=[v1,v2,hsv.v,v1,v2];
return new _3.Palette(_2.map(h,function(hue,i){
return _3.fromHsv(hue,s[i],v[i]);
}));
},monochromatic:function(_20){
var _21=_1.isString(_20.base)?new _3.Color(_20.base):_20.base,hsv=_21.toHsv();
var s1=(hsv.s-30>9)?hsv.s-30:hsv.s+30,s2=hsv.s,v1=_19(hsv.v,20,100),v2=(hsv.v-20>20)?hsv.v-20:hsv.v+60,v3=(hsv.v-50>20)?hsv.v-50:hsv.v+30;
return new _3.Palette([_3.fromHsv(hsv.h,s1,v1),_3.fromHsv(hsv.h,s2,v3),_21,_3.fromHsv(hsv.h,s1,v3),_3.fromHsv(hsv.h,s2,v2)]);
},triadic:function(_22){
var _23=_1.isString(_22.base)?new _3.Color(_22.base):_22.base,hsv=_23.toHsv();
var h1=(hsv.h+57+360)%360,h2=(hsv.h-157+360)%360,s1=(hsv.s>20)?hsv.s-10:hsv.s+10,s2=(hsv.s>90)?hsv.s-10:hsv.s+10,s3=(hsv.s>95)?hsv.s-5:hsv.s+5,v1=(hsv.v-20>20)?hsv.v-20:hsv.v+20,v2=(hsv.v-30>20)?hsv.v-30:hsv.v+30,v3=(hsv.v-30>70)?hsv.v-30:hsv.v+30;
return new _3.Palette([_3.fromHsv(h1,s1,hsv.v),_3.fromHsv(hsv.h,s2,v2),_23,_3.fromHsv(h2,s2,v1),_3.fromHsv(h2,s3,v3)]);
},complementary:function(_24){
var _25=_1.isString(_24.base)?new _3.Color(_24.base):_24.base,hsv=_25.toHsv();
var h1=((hsv.h*2)+137<360)?(hsv.h*2)+137:Math.floor(hsv.h/2)-137,s1=Math.max(hsv.s-10,0),s2=_19(hsv.s,10,100),s3=Math.min(100,hsv.s+20),v1=Math.min(100,hsv.v+30),v2=(hsv.v>20)?hsv.v-30:hsv.v+30;
return new _3.Palette([_3.fromHsv(hsv.h,s1,v1),_3.fromHsv(hsv.h,s2,v2),_25,_3.fromHsv(h1,s3,v2),_3.fromHsv(h1,hsv.s,hsv.v)]);
},splitComplementary:function(_26){
var _27=_1.isString(_26.base)?new _3.Color(_26.base):_26.base,_28=_26.da||30,hsv=_27.toHsv();
var _29=((hsv.h*2)+137<360)?(hsv.h*2)+137:Math.floor(hsv.h/2)-137,h1=(_29-_28+360)%360,h2=(_29+_28)%360,s1=Math.max(hsv.s-10,0),s2=_19(hsv.s,10,100),s3=Math.min(100,hsv.s+20),v1=Math.min(100,hsv.v+30),v2=(hsv.v>20)?hsv.v-30:hsv.v+30;
return new _3.Palette([_3.fromHsv(h1,s1,v1),_3.fromHsv(h1,s2,v2),_27,_3.fromHsv(h2,s3,v2),_3.fromHsv(h2,hsv.s,hsv.v)]);
},compound:function(_2a){
var _2b=_1.isString(_2a.base)?new _3.Color(_2a.base):_2a.base,hsv=_2b.toHsv();
var h1=((hsv.h*2)+18<360)?(hsv.h*2)+18:Math.floor(hsv.h/2)-18,h2=((hsv.h*2)+120<360)?(hsv.h*2)+120:Math.floor(hsv.h/2)-120,h3=((hsv.h*2)+99<360)?(hsv.h*2)+99:Math.floor(hsv.h/2)-99,s1=(hsv.s-40>10)?hsv.s-40:hsv.s+40,s2=(hsv.s-10>80)?hsv.s-10:hsv.s+10,s3=(hsv.s-25>10)?hsv.s-25:hsv.s+25,v1=(hsv.v-40>10)?hsv.v-40:hsv.v+40,v2=(hsv.v-20>80)?hsv.v-20:hsv.v+20,v3=Math.max(hsv.v,20);
return new _3.Palette([_3.fromHsv(h1,s1,v1),_3.fromHsv(h1,s2,v2),_2b,_3.fromHsv(h2,s3,v3),_3.fromHsv(h3,s2,v2)]);
},shades:function(_2c){
var _2d=_1.isString(_2c.base)?new _3.Color(_2c.base):_2c.base,hsv=_2d.toHsv();
var s=(hsv.s==100&&hsv.v==0)?0:hsv.s,v1=(hsv.v-50>20)?hsv.v-50:hsv.v+30,v2=(hsv.v-25>=20)?hsv.v-25:hsv.v+55,v3=(hsv.v-75>=20)?hsv.v-75:hsv.v+5,v4=Math.max(hsv.v-10,20);
return new _3.Palette([new _3.fromHsv(hsv.h,s,v1),new _3.fromHsv(hsv.h,s,v2),_2d,new _3.fromHsv(hsv.h,s,v3),new _3.fromHsv(hsv.h,s,v4)]);
}},generate:function(_2e,_2f){
if(_1.isFunction(_2f)){
return _2f({base:_2e});
}else{
if(_3.Palette.generators[_2f]){
return _3.Palette.generators[_2f]({base:_2e});
}
}
throw new Error("dojox.color.Palette.generate: the specified generator ('"+_2f+"') does not exist.");
}});
return _3.Palette;
});
