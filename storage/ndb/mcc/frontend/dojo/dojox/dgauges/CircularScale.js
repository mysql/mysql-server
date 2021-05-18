//>>built
define("dojox/dgauges/CircularScale",["dojo/_base/declare","dojox/gfx","./ScaleBase","./_circularUtils"],function(_1,_2,_3,_4){
return _1("dojox.dgauges.CircularScale",_3,{originX:50,originY:50,radius:50,startAngle:0,endAngle:180,orientation:"clockwise",constructor:function(){
this.labelPosition="inside";
this.addInvalidatingProperties(["originX","originY","radius","startAngle","endAngle","orientation"]);
},_getOrientationNum:function(){
return this.orientation=="cclockwise"?-1:1;
},positionForValue:function(_5){
var _6=_4.computeTotalAngle(this.startAngle,this.endAngle,this.orientation);
var _7=this.scaler.positionForValue(_5);
return _4.modAngle(this.startAngle+this._getOrientationNum()*_6*_7,360);
},_positionForTickItem:function(_8){
var _9=_4.computeTotalAngle(this.startAngle,this.endAngle,this.orientation);
return _4.modAngle(this.startAngle+this._getOrientationNum()*_9*_8.position,360);
},valueForPosition:function(_a){
if(!this.positionInRange(_a)){
var _b=_4.modAngle(this.startAngle-_a,360);
var _c=360-_b;
var _d=_4.modAngle(this.endAngle-_a,360);
var _e=360-_d;
var _f;
if(Math.min(_b,_c)<Math.min(_d,_e)){
_f=0;
}else{
_f=1;
}
}else{
var _10=_4.modAngle(this._getOrientationNum()*(_a-this.startAngle),360);
var _11=_4.computeTotalAngle(this.startAngle,this.endAngle,this.orientation);
_f=_10/_11;
}
return this.scaler.valueForPosition(_f);
},positionInRange:function(_12){
if(this.startAngle==this.endAngle){
return true;
}
_12=_4.modAngle(_12,360);
if(this._getOrientationNum()==1){
if(this.startAngle<this.endAngle){
return _12>=this.startAngle&&_12<=this.endAngle;
}else{
return !(_12>this.endAngle&&_12<this.startAngle);
}
}else{
if(this.startAngle<this.endAngle){
return !(_12>this.startAngle&&_12<this.endAngle);
}else{
return _12>=this.endAngle&&_12<=this.startAngle;
}
}
},_distance:function(x1,y1,x2,y2){
return Math.sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
},_layoutLabel:function(_13,txt,ox,oy,_14,_15,_16){
var _17=this._getFont();
var box=_2._base._getTextBox(txt,{font:_2.makeFontString(_2.makeParameters(_2.defaultFont,_17))});
var tw=box.w;
var fz=_17.size;
var th=_2.normalizedLength(fz);
var tfx=ox+Math.cos(_15)*_14-tw/2;
var tfy=oy-Math.sin(_15)*_14-th/2;
var _18;
var _19=[];
_18=tfx;
var ipx=_18;
var ipy=-Math.tan(_15)*_18+oy+Math.tan(_15)*ox;
if(ipy>=tfy&&ipy<=tfy+th){
_19.push({x:ipx,y:ipy});
}
_18=tfx+tw;
ipx=_18;
ipy=-Math.tan(_15)*_18+oy+Math.tan(_15)*ox;
if(ipy>=tfy&&ipy<=tfy+th){
_19.push({x:ipx,y:ipy});
}
_18=tfy;
ipx=-1/Math.tan(_15)*_18+ox+1/Math.tan(_15)*oy;
ipy=_18;
if(ipx>=tfx&&ipx<=tfx+tw){
_19.push({x:ipx,y:ipy});
}
_18=tfy+th;
ipx=-1/Math.tan(_15)*_18+ox+1/Math.tan(_15)*oy;
ipy=_18;
if(ipx>=tfx&&ipx<=tfx+tw){
_19.push({x:ipx,y:ipy});
}
var dif;
if(_16=="inside"){
for(var it=0;it<_19.length;it++){
var ip=_19[it];
dif=this._distance(ip.x,ip.y,ox,oy)-_14;
if(dif>=0){
tfx=ox+Math.cos(_15)*(_14-dif)-tw/2;
tfy=oy-Math.sin(_15)*(_14-dif)-th/2;
break;
}
}
}else{
for(it=0;it<_19.length;it++){
ip=_19[it];
dif=this._distance(ip.x,ip.y,ox,oy)-_14;
if(dif<=0){
tfx=ox+Math.cos(_15)*(_14-dif)-tw/2;
tfy=oy-Math.sin(_15)*(_14-dif)-th/2;
break;
}
}
}
if(_13){
_13.setTransform([{dx:tfx+tw/2,dy:tfy+th}]);
}
},refreshRendering:function(){
this.inherited(arguments);
if(!this._gfxGroup||!this.scaler){
return;
}
this.startAngle=_4.modAngle(this.startAngle,360);
this.endAngle=_4.modAngle(this.endAngle,360);
this._ticksGroup.clear();
var _1a;
var _1b;
var _1c;
var _1d=this.scaler.computeTicks();
var _1e;
for(var i=0;i<_1d.length;i++){
var _1f=_1d[i];
_1a=this.tickShapeFunc(this._ticksGroup,this,_1f);
_1e=this._gauge._computeBoundingBox(_1a);
var a;
if(_1f.position){
a=this._positionForTickItem(_1f);
}else{
a=this.positionForValue(_1f.value);
}
if(_1a){
_1a.setTransform([{dx:this.originX,dy:this.originY},_2.matrix.rotateg(a),{dx:this.radius-_1e.width-2*_1e.x,dy:0}]);
}
_1c=this.tickLabelFunc(_1f);
if(_1c){
_1b=this._ticksGroup.createText({x:0,y:0,text:_1c,align:"middle"}).setFont(this._getFont()).setFill(this._getFont().color?this._getFont().color:"black");
var rad=this.radius;
if(this.labelPosition=="inside"){
rad-=(_1e.width+this.labelGap);
}else{
rad+=this.labelGap;
}
this._layoutLabel(_1b,_1c,this.originX,this.originY,rad,_4.toRadians(360-a),this.labelPosition);
}
}
for(var key in this._indicatorsIndex){
this._indicatorsRenderers[key]=this._indicatorsIndex[key].invalidateRendering();
}
}});
});
