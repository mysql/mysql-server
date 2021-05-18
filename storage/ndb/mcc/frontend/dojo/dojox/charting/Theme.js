//>>built
define("dojox/charting/Theme",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","./SimpleTheme","dojox/color/_base","dojox/color/Palette","dojox/gfx/gradutils"],function(_1,_2,_3,_4,_5,_6){
var _7=_2("dojox.charting.Theme",_4,{});
_1.mixin(_7,{defineColors:function(_8){
_8=_8||{};
var l,c=[],n=_8.num||5;
if(_8.colors){
l=_8.colors.length;
for(var i=0;i<n;i++){
c.push(_8.colors[i%l]);
}
return c;
}
if(_8.hue){
var s=_8.saturation||100,st=_8.low||30,_9=_8.high||90;
l=(_9+st)/2;
return _6.generate(_5.fromHsv(_8.hue,s,l),"monochromatic").colors;
}
if(_8.generator){
return _5.Palette.generate(_8.base,_8.generator).colors;
}
return c;
},generateGradient:function(_a,_b,_c){
var _d=_1.delegate(_a);
_d.colors=[{offset:0,color:_b},{offset:1,color:_c}];
return _d;
},generateHslColor:function(_e,_f){
_e=new _3(_e);
var hsl=_e.toHsl(),_10=_5.fromHsl(hsl.h,hsl.s,_f);
_10.a=_e.a;
return _10;
},generateHslGradient:function(_11,_12,_13,_14){
_11=new _3(_11);
var hsl=_11.toHsl(),_15=_5.fromHsl(hsl.h,hsl.s,_13),_16=_5.fromHsl(hsl.h,hsl.s,_14);
_15.a=_16.a=_11.a;
return _7.generateGradient(_12,_15,_16);
}});
_7.defaultMarkers=_4.defaultMarkers;
_7.defaultColors=_4.defaultColors;
_7.defaultTheme=_4.defaultTheme;
return _7;
});
