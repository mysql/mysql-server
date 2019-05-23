//>>built
define("dojox/dgauges/RectangularScale",["dojo/_base/declare","dojox/gfx","./ScaleBase"],function(_1,_2,_3){
return _1("dojox.dgauges.RectangularScale",_3,{paddingLeft:15,paddingTop:12,paddingRight:15,paddingBottom:0,_contentBox:null,constructor:function(){
this.labelPosition="leading";
this.addInvalidatingProperties(["paddingTop","paddingLeft","paddingRight","paddingBottom"]);
},positionForValue:function(_4){
var _5=0;
var _6;
var _7=0;
var _8=0;
if(this._contentBox){
if(this._gauge.orientation=="horizontal"){
_7=this._contentBox.x;
_8=this._contentBox.w;
}else{
_7=this._contentBox.y;
_8=this._contentBox.h;
}
}
_5=this.scaler.positionForValue(_4);
_6=_7+(_5*_8);
return _6;
},valueForPosition:function(_9){
var _a=this.scaler.minimum;
var _b=NaN;
var _c=0;
var _d=0;
if(this._gauge.orientation=="horizontal"){
_b=_9.x;
_c=this._contentBox.x;
_d=this._contentBox.x+this._contentBox.w;
}else{
_b=_9.y;
_c=this._contentBox.y;
_d=this._contentBox.y+this._contentBox.h;
}
if(_b<=_c){
_a=this.scaler.minimum;
}else{
if(_b>=_d){
_a=this.scaler.maximum;
}else{
_a=this.scaler.valueForPosition((_b-_c)/(_d-_c));
}
}
return _a;
},refreshRendering:function(){
this.inherited(arguments);
if(!this._gfxGroup||!this.scaler){
return;
}
this._ticksGroup.clear();
var _e=this._gauge._layoutInfos.middle;
this._contentBox={};
this._contentBox.x=_e.x+this.paddingLeft;
this._contentBox.y=_e.y+this.paddingTop;
this._contentBox.w=_e.w-(this.paddingLeft+this.paddingRight);
this._contentBox.h=_e.h-(this.paddingBottom+this.paddingTop);
var _f;
var _10;
var _11=this._getFont();
var _12=this.scaler.computeTicks();
for(var i=0;i<_12.length;i++){
var _13=_12[i];
_f=this.tickShapeFunc(this._ticksGroup,this,_13);
if(_f){
var a=this.positionForValue(_13.value);
var _14=this._gauge._computeBoundingBox(_f).width;
var x1=0,y1=0,_15=0;
if(this._gauge.orientation=="horizontal"){
x1=a;
y1=this._contentBox.y;
_15=90;
}else{
x1=this._contentBox.x;
y1=a;
}
_f.setTransform([{dx:x1,dy:y1},_2.matrix.rotateg(_15)]);
}
_10=this.tickLabelFunc(_13);
if(_10){
var _16=_2._base._getTextBox(_10,{font:_2.makeFontString(_2.makeParameters(_2.defaultFont,_11))});
var tw=_16.w;
var th=_16.h;
var al="start";
var xt=x1;
var yt=y1;
if(this._gauge.orientation=="horizontal"){
xt=x1;
if(this.labelPosition=="trailing"){
yt=y1+_14+this.labelGap+th;
}else{
yt=y1-this.labelGap;
}
al="middle";
}else{
if(this.labelPosition=="trailing"){
xt=x1+_14+this.labelGap;
}else{
xt=x1-this.labelGap-tw;
}
yt=y1+th/2;
}
var t=this._ticksGroup.createText({x:xt,y:yt,text:_10,align:al});
t.setFill(_11.color?_11.color:"black");
t.setFont(_11);
}
}
for(var key in this._indicatorsIndex){
this._indicatorsRenderers[key]=this._indicatorsIndex[key].invalidateRendering();
}
}});
});
