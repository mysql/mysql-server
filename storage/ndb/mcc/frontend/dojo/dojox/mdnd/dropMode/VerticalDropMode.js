//>>built
define("dojox/mdnd/dropMode/VerticalDropMode",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/html","dojo/_base/array","dojox/mdnd/AreaManager"],function(_1){
var _2=_1.declare("dojox.mdnd.dropMode.VerticalDropMode",null,{_oldXPoint:null,_oldYPoint:null,_oldBehaviour:"up",addArea:function(_3,_4){
var _5=_3.length;
var _6=_1.position(_4.node,true);
_4.coords={"x":_6.x,"y":_6.y};
if(_5==0){
_3.push(_4);
}else{
var x=_4.coords.x;
for(var i=0;i<_5;i++){
if(x<_3[i].coords.x){
for(var j=_5-1;j>=i;j--){
_3[j+1]=_3[j];
}
_3[i]=_4;
break;
}
}
if(i==_5){
_3.push(_4);
}
}
return _3;
},updateAreas:function(_7){
var _8=_7.length;
if(_8>1){
var _9,_a;
for(var i=0;i<_8;i++){
var _b=_7[i];
var _c;
_b.coords.x1=-1;
_b.coords.x2=-1;
if(i==0){
_c=_7[i+1];
this._updateArea(_b);
this._updateArea(_c);
_9=_b.coords.x+_b.node.offsetWidth;
_a=_c.coords.x;
_b.coords.x2=_9+(_a-_9)/2;
}else{
if(i==_8-1){
_b.coords.x1=_7[i-1].coords.x2;
}else{
_c=_7[i+1];
this._updateArea(_c);
_9=_b.coords.x+_b.node.offsetWidth;
_a=_c.coords.x;
_b.coords.x1=_7[i-1].coords.x2;
_b.coords.x2=_9+(_a-_9)/2;
}
}
}
}
},_updateArea:function(_d){
var _e=_1.position(_d.node,true);
_d.coords.x=_e.x;
_d.coords.y=_e.y;
},initItems:function(_f){
_1.forEach(_f.items,function(obj){
var _10=obj.item.node;
var _11=_1.position(_10,true);
var y=_11.y+_11.h/2;
obj.y=y;
});
_f.initItems=true;
},refreshItems:function(_12,_13,_14,_15){
if(_13==-1){
return;
}else{
if(_12&&_14&&_14.h){
var _16=_14.h;
if(_12.margin){
_16+=_12.margin.t;
}
var _17=_12.items.length;
for(var i=_13;i<_17;i++){
var _18=_12.items[i];
if(_15){
_18.y+=_16;
}else{
_18.y-=_16;
}
}
}
}
},getDragPoint:function(_19,_1a,_1b){
var y=_19.y;
if(this._oldYPoint){
if(y>this._oldYPoint){
this._oldBehaviour="down";
y+=_1a.h;
}else{
if(y<=this._oldYPoint){
this._oldBehaviour="up";
}
}
}
this._oldYPoint=y;
return {"x":_19.x+(_1a.w/2),"y":y};
},getTargetArea:function(_1c,_1d,_1e){
var _1f=0;
var x=_1d.x;
var end=_1c.length;
if(end>1){
var _20=0,_21="right",_22=false;
if(_1e==-1||arguments.length<3){
_22=true;
}else{
if(this._checkInterval(_1c,_1e,x)){
_1f=_1e;
}else{
if(this._oldXPoint<x){
_20=_1e+1;
}else{
_20=_1e-1;
end=0;
_21="left";
}
_22=true;
}
}
if(_22){
if(_21==="right"){
for(var i=_20;i<end;i++){
if(this._checkInterval(_1c,i,x)){
_1f=i;
break;
}
}
}else{
for(var i=_20;i>=end;i--){
if(this._checkInterval(_1c,i,x)){
_1f=i;
break;
}
}
}
}
}
this._oldXPoint=x;
return _1f;
},_checkInterval:function(_23,_24,x){
var _25=_23[_24].coords;
if(_25.x1==-1){
if(x<=_25.x2){
return true;
}
}else{
if(_25.x2==-1){
if(x>_25.x1){
return true;
}
}else{
if(_25.x1<x&&x<=_25.x2){
return true;
}
}
}
return false;
},getDropIndex:function(_26,_27){
var _28=_26.items.length;
var _29=_26.coords;
var y=_27.y;
if(_28>0){
for(var i=0;i<_28;i++){
if(y<_26.items[i].y){
return i;
}else{
if(i==_28-1){
return -1;
}
}
}
}
return -1;
},destroy:function(){
}});
dojox.mdnd.areaManager()._dropMode=new dojox.mdnd.dropMode.VerticalDropMode();
return _2;
});
