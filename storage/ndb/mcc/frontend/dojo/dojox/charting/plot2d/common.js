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
},defaultStats:{vmin:Number.POSITIVE_INFINITY,vmax:Number.NEGATIVE_INFINITY,hmin:Number.POSITIVE_INFINITY,hmax:Number.NEGATIVE_INFINITY},collectSimpleStats:function(_c){
var _d=_1.delegate(_4.defaultStats);
for(var i=0;i<_c.length;++i){
var _e=_c[i];
for(var j=0;j<_e.data.length;j++){
if(_e.data[j]!==null){
if(typeof _e.data[j]=="number"){
var _f=_d.vmin,_10=_d.vmax;
if(!("ymin" in _e)||!("ymax" in _e)){
_2.forEach(_e.data,function(val,i){
if(val!==null){
var x=i+1,y=val;
if(isNaN(y)){
y=0;
}
_d.hmin=Math.min(_d.hmin,x);
_d.hmax=Math.max(_d.hmax,x);
_d.vmin=Math.min(_d.vmin,y);
_d.vmax=Math.max(_d.vmax,y);
}
});
}
if("ymin" in _e){
_d.vmin=Math.min(_f,_e.ymin);
}
if("ymax" in _e){
_d.vmax=Math.max(_10,_e.ymax);
}
}else{
var _11=_d.hmin,_12=_d.hmax,_f=_d.vmin,_10=_d.vmax;
if(!("xmin" in _e)||!("xmax" in _e)||!("ymin" in _e)||!("ymax" in _e)){
_2.forEach(_e.data,function(val,i){
if(val!==null){
var x="x" in val?val.x:i+1,y=val.y;
if(isNaN(x)){
x=0;
}
if(isNaN(y)){
y=0;
}
_d.hmin=Math.min(_d.hmin,x);
_d.hmax=Math.max(_d.hmax,x);
_d.vmin=Math.min(_d.vmin,y);
_d.vmax=Math.max(_d.vmax,y);
}
});
}
if("xmin" in _e){
_d.hmin=Math.min(_11,_e.xmin);
}
if("xmax" in _e){
_d.hmax=Math.max(_12,_e.xmax);
}
if("ymin" in _e){
_d.vmin=Math.min(_f,_e.ymin);
}
if("ymax" in _e){
_d.vmax=Math.max(_10,_e.ymax);
}
}
break;
}
}
}
return _d;
},calculateBarSize:function(_13,opt,_14){
if(!_14){
_14=1;
}
var gap=opt.gap,_15=(_13-2*gap)/_14;
if("minBarSize" in opt){
_15=Math.max(_15,opt.minBarSize);
}
if("maxBarSize" in opt){
_15=Math.min(_15,opt.maxBarSize);
}
_15=Math.max(_15,1);
gap=(_13-_15*_14)/2;
return {size:_15,gap:gap};
},collectStackedStats:function(_16){
var _17=_1.clone(_4.defaultStats);
if(_16.length){
_17.hmin=Math.min(_17.hmin,1);
_17.hmax=df.foldl(_16,"seed, run -> Math.max(seed, run.data.length)",_17.hmax);
for(var i=0;i<_17.hmax;++i){
var v=_16[0].data[i];
v=v&&(typeof v=="number"?v:v.y);
if(isNaN(v)){
v=0;
}
_17.vmin=Math.min(_17.vmin,v);
for(var j=1;j<_16.length;++j){
var t=_16[j].data[i];
t=t&&(typeof t=="number"?t:t.y);
if(isNaN(t)){
t=0;
}
v+=t;
}
_17.vmax=Math.max(_17.vmax,v);
}
}
return _17;
},curve:function(a,_18){
var _19=a.slice(0);
if(_18=="x"){
_19[_19.length]=_19[0];
}
var p=_2.map(_19,function(_1a,i){
if(i==0){
return "M"+_1a.x+","+_1a.y;
}
if(!isNaN(_18)){
var dx=_1a.x-_19[i-1].x,dy=_19[i-1].y;
return "C"+(_1a.x-(_18-1)*(dx/_18))+","+dy+" "+(_1a.x-(dx/_18))+","+_1a.y+" "+_1a.x+","+_1a.y;
}else{
if(_18=="X"||_18=="x"||_18=="S"){
var p0,p1=_19[i-1],p2=_19[i],p3;
var _1b,_1c,_1d,_1e;
var f=1/6;
if(i==1){
if(_18=="x"){
p0=_19[_19.length-2];
}else{
p0=p1;
}
f=1/3;
}else{
p0=_19[i-2];
}
if(i==(_19.length-1)){
if(_18=="x"){
p3=_19[1];
}else{
p3=p2;
}
f=1/3;
}else{
p3=_19[i+1];
}
var _1f=Math.sqrt((p2.x-p1.x)*(p2.x-p1.x)+(p2.y-p1.y)*(p2.y-p1.y));
var _20=Math.sqrt((p2.x-p0.x)*(p2.x-p0.x)+(p2.y-p0.y)*(p2.y-p0.y));
var _21=Math.sqrt((p3.x-p1.x)*(p3.x-p1.x)+(p3.y-p1.y)*(p3.y-p1.y));
var _22=_20*f;
var _23=_21*f;
if(_22>_1f/2&&_23>_1f/2){
_22=_1f/2;
_23=_1f/2;
}else{
if(_22>_1f/2){
_22=_1f/2;
_23=_1f/2*_21/_20;
}else{
if(_23>_1f/2){
_23=_1f/2;
_22=_1f/2*_20/_21;
}
}
}
if(_18=="S"){
if(p0==p1){
_22=0;
}
if(p2==p3){
_23=0;
}
}
_1b=p1.x+_22*(p2.x-p0.x)/_20;
_1c=p1.y+_22*(p2.y-p0.y)/_20;
_1d=p2.x-_23*(p3.x-p1.x)/_21;
_1e=p2.y-_23*(p3.y-p1.y)/_21;
}
}
return "C"+(_1b+","+_1c+" "+_1d+","+_1e+" "+p2.x+","+p2.y);
});
return p.join(" ");
},getLabel:function(_24,_25,_26){
return sc.doIfLoaded("dojo/number",function(_27){
return (_25?_27.format(_24,{places:_26}):_27.format(_24))||"";
},function(){
return _25?_24.toFixed(_26):_24.toString();
});
}});
});
