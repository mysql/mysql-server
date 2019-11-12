//>>built
define("dojox/charting/plot2d/commonStacked",["dojo/_base/lang","./common"],function(_1,_2){
var _3=_1.getObject("dojox.charting.plot2d.commonStacked",true);
return _1.mixin(_3,{collectStats:function(_4){
var _5=_1.delegate(_2.defaultStats);
for(var i=0;i<_4.length;++i){
var _6=_4[i];
for(var j=0;j<_6.data.length;j++){
var x,y;
if(_6.data[j]!==null){
if(typeof _6.data[j]=="number"||!_6.data[j].hasOwnProperty("x")){
y=_3.getIndexValue(_4,i,j);
x=j+1;
}else{
x=_6.data[j].x;
if(x!==null){
y=_3.getValue(_4,i,x);
y=y!=null&&y.y?y.y:null;
}
}
_5.hmin=Math.min(_5.hmin,x);
_5.hmax=Math.max(_5.hmax,x);
_5.vmin=Math.min(_5.vmin,y);
_5.vmax=Math.max(_5.vmax,y);
}
}
}
return _5;
},getIndexValue:function(_7,i,_8){
var _9=0,v,j;
for(j=0;j<=i;++j){
v=_7[j].data[_8];
if(v!=null){
if(isNaN(v)){
v=v.y||0;
}
_9+=v;
}
}
return _9;
},getValue:function(_a,i,x){
var _b=null,j,z;
for(j=0;j<=i;++j){
for(z=0;z<_a[j].data.length;z++){
v=_a[j].data[z];
if(v!==null){
if(v.x==x){
if(!_b){
_b={x:x};
}
if(v.y!=null){
if(_b.y==null){
_b.y=0;
}
_b.y+=v.y;
}
break;
}else{
if(v.x>x){
break;
}
}
}
}
}
return _b;
}});
});
