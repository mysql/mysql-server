//>>built
define("dojox/mdnd/dropMode/OverDropMode",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/array","dojo/dom-geometry","dojox/mdnd/AreaManager"],function(_1,_2,_3,_4,_5){
var _6=_2("dojox.mdnd.dropMode.OverDropMode",null,{_oldXPoint:null,_oldYPoint:null,_oldBehaviour:"up",constructor:function(){
this._dragHandler=[_3.connect(dojox.mdnd.areaManager(),"onDragEnter",function(_7,_8){
var m=dojox.mdnd.areaManager();
if(m._oldIndexArea==-1){
m._oldIndexArea=m._lastValidIndexArea;
}
})];
},addArea:function(_9,_a){
var _b=_9.length,_c=_5.position(_a.node,true);
_a.coords={"x":_c.x,"y":_c.y};
if(_b==0){
_9.push(_a);
}else{
var x=_a.coords.x;
for(var i=0;i<_b;i++){
if(x<_9[i].coords.x){
for(var j=_b-1;j>=i;j--){
_9[j+1]=_9[j];
}
_9[i]=_a;
break;
}
}
if(i==_b){
_9.push(_a);
}
}
return _9;
},updateAreas:function(_d){
var _e=_d.length;
for(var i=0;i<_e;i++){
this._updateArea(_d[i]);
}
},_updateArea:function(_f){
var _10=_5.position(_f.node,true);
_f.coords.x=_10.x;
_f.coords.x2=_10.x+_10.w;
_f.coords.y=_10.y;
},initItems:function(_11){
_4.forEach(_11.items,function(obj){
var _12=obj.item.node;
var _13=_5.position(_12,true);
var y=_13.y+_13.h/2;
obj.y=y;
});
_11.initItems=true;
},refreshItems:function(_14,_15,_16,_17){
if(_15==-1){
return;
}else{
if(_14&&_16&&_16.h){
var _18=_16.h;
if(_14.margin){
_18+=_14.margin.t;
}
var _19=_14.items.length;
for(var i=_15;i<_19;i++){
var _1a=_14.items[i];
if(_17){
_1a.y+=_18;
}else{
_1a.y-=_18;
}
}
}
}
},getDragPoint:function(_1b,_1c,_1d){
return {"x":_1d.x,"y":_1d.y};
},getTargetArea:function(_1e,_1f,_20){
var _21=0;
var x=_1f.x;
var y=_1f.y;
var end=_1e.length;
var _22=0,_23="right",_24=false;
if(_20==-1||arguments.length<3){
_24=true;
}else{
if(this._checkInterval(_1e,_20,x,y)){
_21=_20;
}else{
if(this._oldXPoint<x){
_22=_20+1;
}else{
_22=_20-1;
end=0;
_23="left";
}
_24=true;
}
}
if(_24){
if(_23==="right"){
for(var i=_22;i<end;i++){
if(this._checkInterval(_1e,i,x,y)){
_21=i;
break;
}
}
if(i==end){
_21=-1;
}
}else{
for(var i=_22;i>=end;i--){
if(this._checkInterval(_1e,i,x,y)){
_21=i;
break;
}
}
if(i==end-1){
_21=-1;
}
}
}
this._oldXPoint=x;
return _21;
},_checkInterval:function(_25,_26,x,y){
var _27=_25[_26];
var _28=_27.node;
var _29=_27.coords;
var _2a=_29.x;
var _2b=_29.x2;
var _2c=_29.y;
var _2d=_2c+_28.offsetHeight;
if(_2a<=x&&x<=_2b&&_2c<=y&&y<=_2d){
return true;
}
return false;
},getDropIndex:function(_2e,_2f){
var _30=_2e.items.length;
var _31=_2e.coords;
var y=_2f.y;
if(_30>0){
for(var i=0;i<_30;i++){
if(y<_2e.items[i].y){
return i;
}else{
if(i==_30-1){
return -1;
}
}
}
}
return -1;
},destroy:function(){
_4.forEach(this._dragHandler,_3.disconnect);
}});
dojox.mdnd.areaManager()._dropMode=new dojox.mdnd.dropMode.OverDropMode();
return _6;
});
