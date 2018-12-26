//>>built
define("dojox/mdnd/dropMode/OverDropMode",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/html","dojo/_base/array","dojox/mdnd/AreaManager"],function(_1){
var _2=_1.declare("dojox.mdnd.dropMode.OverDropMode",null,{_oldXPoint:null,_oldYPoint:null,_oldBehaviour:"up",constructor:function(){
this._dragHandler=[_1.connect(dojox.mdnd.areaManager(),"onDragEnter",function(_3,_4){
var m=dojox.mdnd.areaManager();
if(m._oldIndexArea==-1){
m._oldIndexArea=m._lastValidIndexArea;
}
})];
},addArea:function(_5,_6){
var _7=_5.length,_8=_1.position(_6.node,true);
_6.coords={"x":_8.x,"y":_8.y};
if(_7==0){
_5.push(_6);
}else{
var x=_6.coords.x;
for(var i=0;i<_7;i++){
if(x<_5[i].coords.x){
for(var j=_7-1;j>=i;j--){
_5[j+1]=_5[j];
}
_5[i]=_6;
break;
}
}
if(i==_7){
_5.push(_6);
}
}
return _5;
},updateAreas:function(_9){
var _a=_9.length;
for(var i=0;i<_a;i++){
this._updateArea(_9[i]);
}
},_updateArea:function(_b){
var _c=_1.position(_b.node,true);
_b.coords.x=_c.x;
_b.coords.x2=_c.x+_c.w;
_b.coords.y=_c.y;
},initItems:function(_d){
_1.forEach(_d.items,function(_e){
var _f=_e.item.node;
var _10=_1.position(_f,true);
var y=_10.y+_10.h/2;
_e.y=y;
});
_d.initItems=true;
},refreshItems:function(_11,_12,_13,_14){
if(_12==-1){
return;
}else{
if(_11&&_13&&_13.h){
var _15=_13.h;
if(_11.margin){
_15+=_11.margin.t;
}
var _16=_11.items.length;
for(var i=_12;i<_16;i++){
var _17=_11.items[i];
if(_14){
_17.y+=_15;
}else{
_17.y-=_15;
}
}
}
}
},getDragPoint:function(_18,_19,_1a){
return {"x":_1a.x,"y":_1a.y};
},getTargetArea:function(_1b,_1c,_1d){
var _1e=0;
var x=_1c.x;
var y=_1c.y;
var end=_1b.length;
var _1f=0,_20="right",_21=false;
if(_1d==-1||arguments.length<3){
_21=true;
}else{
if(this._checkInterval(_1b,_1d,x,y)){
_1e=_1d;
}else{
if(this._oldXPoint<x){
_1f=_1d+1;
}else{
_1f=_1d-1;
end=0;
_20="left";
}
_21=true;
}
}
if(_21){
if(_20==="right"){
for(var i=_1f;i<end;i++){
if(this._checkInterval(_1b,i,x,y)){
_1e=i;
break;
}
}
if(i==end){
_1e=-1;
}
}else{
for(var i=_1f;i>=end;i--){
if(this._checkInterval(_1b,i,x,y)){
_1e=i;
break;
}
}
if(i==end-1){
_1e=-1;
}
}
}
this._oldXPoint=x;
return _1e;
},_checkInterval:function(_22,_23,x,y){
var _24=_22[_23];
var _25=_24.node;
var _26=_24.coords;
var _27=_26.x;
var _28=_26.x2;
var _29=_26.y;
var _2a=_29+_25.offsetHeight;
if(_27<=x&&x<=_28&&_29<=y&&y<=_2a){
return true;
}
return false;
},getDropIndex:function(_2b,_2c){
var _2d=_2b.items.length;
var _2e=_2b.coords;
var y=_2c.y;
if(_2d>0){
for(var i=0;i<_2d;i++){
if(y<_2b.items[i].y){
return i;
}else{
if(i==_2d-1){
return -1;
}
}
}
}
return -1;
},destroy:function(){
_1.forEach(this._dragHandler,_1.disconnect);
}});
dojox.mdnd.areaManager()._dropMode=new dojox.mdnd.dropMode.OverDropMode();
return _2;
});
