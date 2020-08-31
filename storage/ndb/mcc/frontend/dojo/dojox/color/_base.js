//>>built
define("dojox/color/_base",["../main","dojo/_base/lang","dojo/_base/Color","dojo/colors"],function(_1,_2,_3,_4){
var cx=_2.getObject("color",true,_1);
cx.Color=_3;
cx.blend=_3.blendColors;
cx.fromRgb=_3.fromRgb;
cx.fromHex=_3.fromHex;
cx.fromArray=_3.fromArray;
cx.fromString=_3.fromString;
cx.greyscale=_4.makeGrey;
_2.mixin(cx,{fromCmy:function(_5,_6,_7){
if(_2.isArray(_5)){
_6=_5[1],_7=_5[2],_5=_5[0];
}else{
if(_2.isObject(_5)){
_6=_5.m,_7=_5.y,_5=_5.c;
}
}
_5/=100,_6/=100,_7/=100;
var r=1-_5,g=1-_6,b=1-_7;
return new _3({r:Math.round(r*255),g:Math.round(g*255),b:Math.round(b*255)});
},fromCmyk:function(_8,_9,_a,_b){
if(_2.isArray(_8)){
_9=_8[1],_a=_8[2],_b=_8[3],_8=_8[0];
}else{
if(_2.isObject(_8)){
_9=_8.m,_a=_8.y,_b=_8.b,_8=_8.c;
}
}
_8/=100,_9/=100,_a/=100,_b/=100;
var r,g,b;
r=1-Math.min(1,_8*(1-_b)+_b);
g=1-Math.min(1,_9*(1-_b)+_b);
b=1-Math.min(1,_a*(1-_b)+_b);
return new _3({r:Math.round(r*255),g:Math.round(g*255),b:Math.round(b*255)});
},fromHsl:function(_c,_d,_e){
if(_2.isArray(_c)){
_d=_c[1],_e=_c[2],_c=_c[0];
}else{
if(_2.isObject(_c)){
_d=_c.s,_e=_c.l,_c=_c.h;
}
}
_d/=100;
_e/=100;
while(_c<0){
_c+=360;
}
while(_c>=360){
_c-=360;
}
var r,g,b;
if(_c<120){
r=(120-_c)/60,g=_c/60,b=0;
}else{
if(_c<240){
r=0,g=(240-_c)/60,b=(_c-120)/60;
}else{
r=(_c-240)/60,g=0,b=(360-_c)/60;
}
}
r=2*_d*Math.min(r,1)+(1-_d);
g=2*_d*Math.min(g,1)+(1-_d);
b=2*_d*Math.min(b,1)+(1-_d);
if(_e<0.5){
r*=_e,g*=_e,b*=_e;
}else{
r=(1-_e)*r+2*_e-1;
g=(1-_e)*g+2*_e-1;
b=(1-_e)*b+2*_e-1;
}
return new _3({r:Math.round(r*255),g:Math.round(g*255),b:Math.round(b*255)});
}});
cx.fromHsv=function(_f,_10,_11){
if(_2.isArray(_f)){
_10=_f[1],_11=_f[2],_f=_f[0];
}else{
if(_2.isObject(_f)){
_10=_f.s,_11=_f.v,_f=_f.h;
}
}
if(_f==360){
_f=0;
}
_10/=100;
_11/=100;
var r,g,b;
if(_10==0){
r=_11,b=_11,g=_11;
}else{
var _12=_f/60,i=Math.floor(_12),f=_12-i;
var p=_11*(1-_10);
var q=_11*(1-(_10*f));
var t=_11*(1-(_10*(1-f)));
switch(i){
case 0:
r=_11,g=t,b=p;
break;
case 1:
r=q,g=_11,b=p;
break;
case 2:
r=p,g=_11,b=t;
break;
case 3:
r=p,g=q,b=_11;
break;
case 4:
r=t,g=p,b=_11;
break;
case 5:
r=_11,g=p,b=q;
break;
}
}
return new _3({r:Math.round(r*255),g:Math.round(g*255),b:Math.round(b*255)});
};
_2.extend(_3,{toCmy:function(){
var _13=1-(this.r/255),_14=1-(this.g/255),_15=1-(this.b/255);
return {c:Math.round(_13*100),m:Math.round(_14*100),y:Math.round(_15*100)};
},toCmyk:function(){
var _16,_17,_18,_19;
var r=this.r/255,g=this.g/255,b=this.b/255;
_19=Math.min(1-r,1-g,1-b);
_16=(1-r-_19)/(1-_19);
_17=(1-g-_19)/(1-_19);
_18=(1-b-_19)/(1-_19);
return {c:Math.round(_16*100),m:Math.round(_17*100),y:Math.round(_18*100),b:Math.round(_19*100)};
},toHsl:function(){
var r=this.r/255,g=this.g/255,b=this.b/255;
var min=Math.min(r,b,g),max=Math.max(r,g,b);
var _1a=max-min;
var h=0,s=0,l=(min+max)/2;
if(l>0&&l<1){
s=_1a/((l<0.5)?(2*l):(2-2*l));
}
if(_1a>0){
if(max==r&&max!=g){
h+=(g-b)/_1a;
}
if(max==g&&max!=b){
h+=(2+(b-r)/_1a);
}
if(max==b&&max!=r){
h+=(4+(r-g)/_1a);
}
h*=60;
}
return {h:h,s:Math.round(s*100),l:Math.round(l*100)};
},toHsv:function(){
var r=this.r/255,g=this.g/255,b=this.b/255;
var min=Math.min(r,b,g),max=Math.max(r,g,b);
var _1b=max-min;
var h=null,s=(max==0)?0:(_1b/max);
if(s==0){
h=0;
}else{
if(r==max){
h=60*(g-b)/_1b;
}else{
if(g==max){
h=120+60*(b-r)/_1b;
}else{
h=240+60*(r-g)/_1b;
}
}
if(h<0){
h+=360;
}
}
return {h:h,s:Math.round(s*100),v:Math.round(max*100)};
}});
return cx;
});
