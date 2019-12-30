//>>built
define("dojox/mdnd/dropMode/DefaultDropMode",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/array","dojo/dom-geometry","dojox/mdnd/AreaManager"],function(_1,_2,_3,_4){
var _5=_2("dojox.mdnd.dropMode.DefaultDropMode",null,{_oldXPoint:null,_oldYPoint:null,_oldBehaviour:"up",addArea:function(_6,_7){
var _8=_6.length;
var _9=_4.position(_7.node,true);
_7.coords={"x":_9.x,"y":_9.y};
if(_8==0){
_6.push(_7);
}else{
var x=_7.coords.x;
for(var i=0;i<_8;i++){
if(x<_6[i].coords.x){
for(var j=_8-1;j>=i;j--){
_6[j+1]=_6[j];
}
_6[i]=_7;
break;
}
}
if(i==_8){
_6.push(_7);
}
}
return _6;
},updateAreas:function(_a){
var _b=_a.length;
if(_b>1){
var _c,_d;
for(var i=0;i<_b;i++){
var _e=_a[i];
var _f;
_e.coords.x1=-1;
_e.coords.x2=-1;
if(i==0){
_f=_a[i+1];
this._updateArea(_e);
this._updateArea(_f);
_c=_e.coords.x+_e.node.offsetWidth;
_d=_f.coords.x;
_e.coords.x2=_c+(_d-_c)/2;
}else{
if(i==_b-1){
_e.coords.x1=_a[i-1].coords.x2;
}else{
_f=_a[i+1];
this._updateArea(_f);
_c=_e.coords.x+_e.node.offsetWidth;
_d=_f.coords.x;
_e.coords.x1=_a[i-1].coords.x2;
_e.coords.x2=_c+(_d-_c)/2;
}
}
}
}
},_updateArea:function(_10){
var _11=_4.position(_10.node,true);
_10.coords.x=_11.x;
_10.coords.y=_11.y;
},initItems:function(_12){
_3.forEach(_12.items,function(obj){
var _13=obj.item.node;
var _14=_4.position(_13,true);
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
var y=_1c.y;
if(this._oldYPoint){
if(y>this._oldYPoint){
this._oldBehaviour="down";
y+=_1d.h;
}else{
if(y<=this._oldYPoint){
this._oldBehaviour="up";
}
}
}
this._oldYPoint=y;
return {"x":_1c.x+(_1d.w/2),"y":y};
},getTargetArea:function(_1f,_20,_21){
var _22=0;
var x=_20.x;
var end=_1f.length;
if(end>1){
var _23=0,_24="right",_25=false;
if(_21==-1||arguments.length<3){
_25=true;
}else{
if(this._checkInterval(_1f,_21,x)){
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
if(this._checkInterval(_1f,i,x)){
_22=i;
break;
}
}
}else{
for(var i=_23;i>=end;i--){
if(this._checkInterval(_1f,i,x)){
_22=i;
break;
}
}
}
}
}
this._oldXPoint=x;
return _22;
},_checkInterval:function(_26,_27,x){
var _28=_26[_27].coords;
if(_28.x1==-1){
if(x<=_28.x2){
return true;
}
}else{
if(_28.x2==-1){
if(x>_28.x1){
return true;
}
}else{
if(_28.x1<x&&x<=_28.x2){
return true;
}
}
}
return false;
},getDropIndex:function(_29,_2a){
var _2b=_29.items.length;
var _2c=_29.coords;
var y=_2a.y;
if(_2b>0){
for(var i=0;i<_2b;i++){
if(y<_29.items[i].y){
return i;
}else{
if(i==_2b-1){
return -1;
}
}
}
}
return -1;
},destroy:function(){
}});
dojox.mdnd.areaManager()._dropMode=new dojox.mdnd.dropMode.DefaultDropMode();
return _5;
});
