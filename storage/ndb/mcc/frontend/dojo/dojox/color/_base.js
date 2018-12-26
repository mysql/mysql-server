//>>built
define("dojox/color/_base",["dojo/_base/kernel","../main","dojo/_base/lang","dojo/_base/Color","dojo/colors"],function(_1,_2,_3,_4,_5){
var cx=_3.getObject("dojox.color",true);
cx.Color=_4;
cx.blend=_4.blendColors;
cx.fromRgb=_4.fromRgb;
cx.fromHex=_4.fromHex;
cx.fromArray=_4.fromArray;
cx.fromString=_4.fromString;
cx.greyscale=_5.makeGrey;
_3.mixin(cx,{fromCmy:function(_6,_7,_8){
if(_3.isArray(_6)){
_7=_6[1],_8=_6[2],_6=_6[0];
}else{
if(_3.isObject(_6)){
_7=_6.m,_8=_6.y,_6=_6.c;
}
}
_6/=100,_7/=100,_8/=100;
var r=1-_6,g=1-_7,b=1-_8;
return new _4({r:Math.round(r*255),g:Math.round(g*255),b:Math.round(b*255)});
},fromCmyk:function(_9,_a,_b,_c){
if(_3.isArray(_9)){
_a=_9[1],_b=_9[2],_c=_9[3],_9=_9[0];
}else{
if(_3.isObject(_9)){
_a=_9.m,_b=_9.y,_c=_9.b,_9=_9.c;
}
}
_9/=100,_a/=100,_b/=100,_c/=100;
var r,g,b;
r=1-Math.min(1,_9*(1-_c)+_c);
g=1-Math.min(1,_a*(1-_c)+_c);
b=1-Math.min(1,_b*(1-_c)+_c);
return new _4({r:Math.round(r*255),g:Math.round(g*255),b:Math.round(b*255)});
},fromHsl:function(_d,_e,_f){
if(_3.isArray(_d)){
_e=_d[1],_f=_d[2],_d=_d[0];
}else{
if(_3.isObject(_d)){
_e=_d.s,_f=_d.l,_d=_d.h;
}
}
_e/=100;
_f/=100;
while(_d<0){
_d+=360;
}
while(_d>=360){
_d-=360;
}
var r,g,b;
if(_d<120){
r=(120-_d)/60,g=_d/60,b=0;
}else{
if(_d<240){
r=0,g=(240-_d)/60,b=(_d-120)/60;
}else{
r=(_d-240)/60,g=0,b=(360-_d)/60;
}
}
r=2*_e*Math.min(r,1)+(1-_e);
g=2*_e*Math.min(g,1)+(1-_e);
b=2*_e*Math.min(b,1)+(1-_e);
if(_f<0.5){
r*=_f,g*=_f,b*=_f;
}else{
r=(1-_f)*r+2*_f-1;
g=(1-_f)*g+2*_f-1;
b=(1-_f)*b+2*_f-1;
}
return new _4({r:Math.round(r*255),g:Math.round(g*255),b:Math.round(b*255)});
}});
cx.fromHsv=function(hue,_10,_11){
if(_3.isArray(hue)){
_10=hue[1],_11=hue[2],hue=hue[0];
}else{
if(_3.isObject(hue)){
_10=hue.s,_11=hue.v,hue=hue.h;
}
}
if(hue==360){
hue=0;
}
_10/=100;
_11/=100;
var r,g,b;
if(_10==0){
r=_11,b=_11,g=_11;
}else{
var _12=hue/60,i=Math.floor(_12),f=_12-i;
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
return new _4({r:Math.round(r*255),g:Math.round(g*255),b:Math.round(b*255)});
};
_3.extend(_4,{toCmy:function(){
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
