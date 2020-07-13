//>>built
define("dojox/mdnd/dropMode/VerticalDropMode",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/array","dojo/dom-geometry","dojox/mdnd/AreaManager"],function(_1,_2,_3,_4,_5){
var _6=_2("dojox.mdnd.dropMode.VerticalDropMode",null,{_oldXPoint:null,_oldYPoint:null,_oldBehaviour:"up",addArea:function(_7,_8){
var _9=_7.length;
var _a=_4.position(_8.node,true);
_8.coords={"x":_a.x,"y":_a.y};
if(_9==0){
_7.push(_8);
}else{
var x=_8.coords.x;
for(var i=0;i<_9;i++){
if(x<_7[i].coords.x){
for(var j=_9-1;j>=i;j--){
_7[j+1]=_7[j];
}
_7[i]=_8;
break;
}
}
if(i==_9){
_7.push(_8);
}
}
return _7;
},updateAreas:function(_b){
var _c=_b.length;
if(_c>1){
var _d,_e;
for(var i=0;i<_c;i++){
var _f=_b[i];
var _10;
_f.coords.x1=-1;
_f.coords.x2=-1;
if(i==0){
_10=_b[i+1];
this._updateArea(_f);
this._updateArea(_10);
_d=_f.coords.x+_f.node.offsetWidth;
_e=_10.coords.x;
_f.coords.x2=_d+(_e-_d)/2;
}else{
if(i==_c-1){
_f.coords.x1=_b[i-1].coords.x2;
}else{
_10=_b[i+1];
this._updateArea(_10);
_d=_f.coords.x+_f.node.offsetWidth;
_e=_10.coords.x;
_f.coords.x1=_b[i-1].coords.x2;
_f.coords.x2=_d+(_e-_d)/2;
}
}
}
}
},_updateArea:function(_11){
var _12=_4.position(_11.node,true);
_11.coords.x=_12.x;
_11.coords.y=_12.y;
},initItems:function(_13){
_3.forEach(_13.items,function(obj){
var _14=obj.item.node;
var _15=_4.position(_14,true);
var y=_15.y+_15.h/2;
obj.y=y;
});
_13.initItems=true;
},refreshItems:function(_16,_17,_18,_19){
if(_17==-1){
return;
}else{
if(_16&&_18&&_18.h){
var _1a=_18.h;
if(_16.margin){
_1a+=_16.margin.t;
}
var _1b=_16.items.length;
for(var i=_17;i<_1b;i++){
var _1c=_16.items[i];
if(_19){
_1c.y+=_1a;
}else{
_1c.y-=_1a;
}
}
}
}
},getDragPoint:function(_1d,_1e,_1f){
var y=_1d.y;
if(this._oldYPoint){
if(y>this._oldYPoint){
this._oldBehaviour="down";
y+=_1e.h;
}else{
if(y<=this._oldYPoint){
this._oldBehaviour="up";
}
}
}
this._oldYPoint=y;
return {"x":_1d.x+(_1e.w/2),"y":y};
},getTargetArea:function(_20,_21,_22){
var _23=0;
var x=_21.x;
var end=_20.length;
if(end>1){
var _24=0,_25="right",_26=false;
if(_22==-1||arguments.length<3){
_26=true;
}else{
if(this._checkInterval(_20,_22,x)){
_23=_22;
}else{
if(this._oldXPoint<x){
_24=_22+1;
}else{
_24=_22-1;
end=0;
_25="left";
}
_26=true;
}
}
if(_26){
if(_25==="right"){
for(var i=_24;i<end;i++){
if(this._checkInterval(_20,i,x)){
_23=i;
break;
}
}
}else{
for(var i=_24;i>=end;i--){
if(this._checkInterval(_20,i,x)){
_23=i;
break;
}
}
}
}
}
this._oldXPoint=x;
return _23;
},_checkInterval:function(_27,_28,x){
var _29=_27[_28].coords;
if(_29.x1==-1){
if(x<=_29.x2){
return true;
}
}else{
if(_29.x2==-1){
if(x>_29.x1){
return true;
}
}else{
if(_29.x1<x&&x<=_29.x2){
return true;
}
}
}
return false;
},getDropIndex:function(_2a,_2b){
var _2c=_2a.items.length;
var _2d=_2a.coords;
var y=_2b.y;
if(_2c>0){
for(var i=0;i<_2c;i++){
if(y<_2a.items[i].y){
return i;
}else{
if(i==_2c-1){
return -1;
}
}
}
}
return -1;
},destroy:function(){
}});
_5.areaManager()._dropMode=new _6();
return _6;
});
