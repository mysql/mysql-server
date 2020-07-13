//>>built
define("dojox/charting/plot2d/common",["dojo/_base/lang","dojo/_base/array","dojo/_base/Color","dojox/gfx","dojox/lang/functional","../scaler/common"],function(_1,_2,_3,g,df,sc){
var _4=_1.getObject("dojox.charting.plot2d.common",true);
return _1.mixin(_4,{doIfLoaded:sc.doIfLoaded,makeStroke:function(_5){
if(!_5){
return _5;
}
if(typeof _5=="string"||_5 instanceof _3){
_5={color:_5};
}
return g.makeParameters(g.defaultStroke,_5);
},augmentColor:function(_6,_7){
var t=new _3(_6),c=new _3(_7);
c.a=t.a;
return c;
},augmentStroke:function(_8,_9){
var s=_4.makeStroke(_8);
if(s){
s.color=_4.augmentColor(s.color,_9);
}
return s;
},augmentFill:function(_a,_b){
var fc,c=new _3(_b);
if(typeof _a=="string"||_a instanceof _3){
return _4.augmentColor(_a,_b);
}
return _a;
},defaultStats:{vmin:Number.POSITIVE_INFINITY,vmax:Number.NEGATIVE_INFINITY,hmin:Number.POSITIVE_INFINITY,hmax:Number.NEGATIVE_INFINITY},collectSimpleStats:function(_c,_d){
var _e=_1.delegate(_4.defaultStats);
for(var i=0;i<_c.length;++i){
var _f=_c[i];
for(var j=0;j<_f.data.length;j++){
if(!_d(_f.data[j])){
if(typeof _f.data[j]=="number"){
var _10=_e.vmin,_11=_e.vmax;
_2.forEach(_f.data,function(val,i){
if(!_d(val)){
var x=i+1,y=val;
if(isNaN(y)){
y=0;
}
_e.hmin=Math.min(_e.hmin,x);
_e.hmax=Math.max(_e.hmax,x);
_e.vmin=Math.min(_e.vmin,y);
_e.vmax=Math.max(_e.vmax,y);
}
});
if("ymin" in _f){
_e.vmin=Math.min(_10,_f.ymin);
}
if("ymax" in _f){
_e.vmax=Math.max(_11,_f.ymax);
}
}else{
var _12=_e.hmin,_13=_e.hmax,_10=_e.vmin,_11=_e.vmax;
if(!("xmin" in _f)||!("xmax" in _f)||!("ymin" in _f)||!("ymax" in _f)){
_2.forEach(_f.data,function(val,i){
if(!_d(val)){
var x="x" in val?val.x:i+1,y=val.y;
if(isNaN(x)){
x=0;
}
if(isNaN(y)){
y=0;
}
_e.hmin=Math.min(_e.hmin,x);
_e.hmax=Math.max(_e.hmax,x);
_e.vmin=Math.min(_e.vmin,y);
_e.vmax=Math.max(_e.vmax,y);
}
});
}
if("xmin" in _f){
_e.hmin=Math.min(_12,_f.xmin);
}
if("xmax" in _f){
_e.hmax=Math.max(_13,_f.xmax);
}
if("ymin" in _f){
_e.vmin=Math.min(_10,_f.ymin);
}
if("ymax" in _f){
_e.vmax=Math.max(_11,_f.ymax);
}
}
break;
}
}
}
return _e;
},calculateBarSize:function(_14,opt,_15){
if(!_15){
_15=1;
}
var gap=opt.gap,_16=(_14-2*gap)/_15;
if("minBarSize" in opt){
_16=Math.max(_16,opt.minBarSize);
}
if("maxBarSize" in opt){
_16=Math.min(_16,opt.maxBarSize);
}
_16=Math.max(_16,1);
gap=(_14-_16*_15)/2;
return {size:_16,gap:gap};
},collectStackedStats:function(_17){
var _18=_1.clone(_4.defaultStats);
if(_17.length){
_18.hmin=Math.min(_18.hmin,1);
_18.hmax=df.foldl(_17,"seed, run -> Math.max(seed, run.data.length)",_18.hmax);
for(var i=0;i<_18.hmax;++i){
var v=_17[0].data[i];
v=v&&(typeof v=="number"?v:v.y);
if(isNaN(v)){
v=0;
}
_18.vmin=Math.min(_18.vmin,v);
for(var j=1;j<_17.length;++j){
var t=_17[j].data[i];
t=t&&(typeof t=="number"?t:t.y);
if(isNaN(t)){
t=0;
}
v+=t;
}
_18.vmax=Math.max(_18.vmax,v);
}
}
return _18;
},curve:function(a,_19){
var _1a=a.slice(0);
if(_19=="x"){
_1a[_1a.length]=_1a[0];
}
var p=_2.map(_1a,function(_1b,i){
if(i==0){
return "M"+_1b.x+","+_1b.y;
}
if(!isNaN(_19)){
var dx=_1b.x-_1a[i-1].x,dy=_1a[i-1].y;
return "C"+(_1b.x-(_19-1)*(dx/_19))+","+dy+" "+(_1b.x-(dx/_19))+","+_1b.y+" "+_1b.x+","+_1b.y;
}else{
if(_19=="X"||_19=="x"||_19=="S"){
var p0,p1=_1a[i-1],p2=_1a[i],p3;
var _1c,_1d,_1e,_1f;
var f=1/6;
if(i==1){
if(_19=="x"){
p0=_1a[_1a.length-2];
}else{
p0=p1;
}
f=1/3;
}else{
p0=_1a[i-2];
}
if(i==(_1a.length-1)){
if(_19=="x"){
p3=_1a[1];
}else{
p3=p2;
}
f=1/3;
}else{
p3=_1a[i+1];
}
var _20=Math.sqrt((p2.x-p1.x)*(p2.x-p1.x)+(p2.y-p1.y)*(p2.y-p1.y));
var _21=Math.sqrt((p2.x-p0.x)*(p2.x-p0.x)+(p2.y-p0.y)*(p2.y-p0.y));
var _22=Math.sqrt((p3.x-p1.x)*(p3.x-p1.x)+(p3.y-p1.y)*(p3.y-p1.y));
var _23=_21*f;
var _24=_22*f;
if(_23>_20/2&&_24>_20/2){
_23=_20/2;
_24=_20/2;
}else{
if(_23>_20/2){
_23=_20/2;
_24=_20/2*_22/_21;
}else{
if(_24>_20/2){
_24=_20/2;
_23=_20/2*_21/_22;
}
}
}
if(_19=="S"){
if(p0==p1){
_23=0;
}
if(p2==p3){
_24=0;
}
}
_1c=p1.x+_23*(p2.x-p0.x)/_21;
_1d=p1.y+_23*(p2.y-p0.y)/_21;
_1e=p2.x-_24*(p3.x-p1.x)/_22;
_1f=p2.y-_24*(p3.y-p1.y)/_22;
}
}
return "C"+(_1c+","+_1d+" "+_1e+","+_1f+" "+p2.x+","+p2.y);
});
return p.join(" ");
},getLabel:function(_25,_26,_27){
return sc.doIfLoaded("dojo/number",function(_28){
return (_26?_28.format(_25,{places:_27}):_28.format(_25))||"";
},function(){
return _26?_25.toFixed(_27):_25.toString();
});
},purgeGroup:function(_29){
return _29.purgeGroup();
}});
});
