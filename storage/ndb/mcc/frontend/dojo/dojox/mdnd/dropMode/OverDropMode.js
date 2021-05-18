//>>built
define("dojox/mdnd/dropMode/OverDropMode",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/array","dojo/dom-geometry","dojox/mdnd/AreaManager"],function(_1,_2,_3,_4,_5,_6){
var _7=_2("dojox.mdnd.dropMode.OverDropMode",null,{_oldXPoint:null,_oldYPoint:null,_oldBehaviour:"up",constructor:function(){
this._dragHandler=[_3.connect(_6.areaManager(),"onDragEnter",function(_8,_9){
var m=_6.areaManager();
if(m._oldIndexArea==-1){
m._oldIndexArea=m._lastValidIndexArea;
}
})];
},addArea:function(_a,_b){
var _c=_a.length,_d=_5.position(_b.node,true);
_b.coords={"x":_d.x,"y":_d.y};
if(_c==0){
_a.push(_b);
}else{
var x=_b.coords.x;
for(var i=0;i<_c;i++){
if(x<_a[i].coords.x){
for(var j=_c-1;j>=i;j--){
_a[j+1]=_a[j];
}
_a[i]=_b;
break;
}
}
if(i==_c){
_a.push(_b);
}
}
return _a;
},updateAreas:function(_e){
var _f=_e.length;
for(var i=0;i<_f;i++){
this._updateArea(_e[i]);
}
},_updateArea:function(_10){
var _11=_5.position(_10.node,true);
_10.coords.x=_11.x;
_10.coords.x2=_11.x+_11.w;
_10.coords.y=_11.y;
},initItems:function(_12){
_4.forEach(_12.items,function(obj){
var _13=obj.item.node;
var _14=_5.position(_13,true);
var y=_14.y+_14.h/2;
obj.y=y;
});
_12.initItems=true;
},refreshItems:function(_15,_16,_17,_18){
if(_16==-1){
return;
}else{
if(_15&&_17&&_17.h){
var _19=_17.h;
if(_15.margin){
_19+=_15.margin.t;
}
var _1a=_15.items.length;
for(var i=_16;i<_1a;i++){
var _1b=_15.items[i];
if(_18){
_1b.y+=_19;
}else{
_1b.y-=_19;
}
}
}
}
},getDragPoint:function(_1c,_1d,_1e){
return {"x":_1e.x,"y":_1e.y};
},getTargetArea:function(_1f,_20,_21){
var _22=0;
var x=_20.x;
var y=_20.y;
var end=_1f.length;
var _23=0,_24="right",_25=false;
if(_21==-1||arguments.length<3){
_25=true;
}else{
if(this._checkInterval(_1f,_21,x,y)){
_22=_21;
}else{
if(this._oldXPoint<x){
_23=_21+1;
}else{
_23=_21-1;
end=0;
_24="left";
}
_25=true;
}
}
if(_25){
if(_24==="right"){
for(var i=_23;i<end;i++){
if(this._checkInterval(_1f,i,x,y)){
_22=i;
break;
}
}
if(i==end){
_22=-1;
}
}else{
for(var i=_23;i>=end;i--){
if(this._checkInterval(_1f,i,x,y)){
_22=i;
break;
}
}
if(i==end-1){
_22=-1;
}
}
}
this._oldXPoint=x;
return _22;
},_checkInterval:function(_26,_27,x,y){
var _28=_26[_27];
var _29=_28.node;
var _2a=_28.coords;
var _2b=_2a.x;
var _2c=_2a.x2;
var _2d=_2a.y;
var _2e=_2d+_29.offsetHeight;
if(_2b<=x&&x<=_2c&&_2d<=y&&y<=_2e){
return true;
}
return false;
},getDropIndex:function(_2f,_30){
var _31=_2f.items.length;
var _32=_2f.coords;
var y=_30.y;
if(_31>0){
for(var i=0;i<_31;i++){
if(y<_2f.items[i].y){
return i;
}else{
if(i==_31-1){
return -1;
}
}
}
}
return -1;
},destroy:function(){
_4.forEach(this._dragHandler,_3.disconnect);
}});
_6.areaManager()._dropMode=new _7();
return _7;
});
