//>>built
define("dojox/form/_RangeSliderMixin",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/fx","dojo/_base/event","dojo/_base/sniff","dojo/dom-style","dojo/dom-geometry","dojo/keys","dijit","dojo/dnd/Mover","dojo/dnd/Moveable","dijit/form/_FormValueWidget","dijit/focus","dojo/fx","dojox/fx"],function(_1,_2,_3,fx,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e){
var _f=function(a,b){
return b-a;
},_10=function(a,b){
return a-b;
};
var _11=_1("dojox.form._RangeSliderMixin",null,{_setTabIndexAttr:["sliderHandle","sliderHandleMax"],value:[0,100],postMixInProperties:function(){
this.inherited(arguments);
this.value=_3.map(this.value,function(i){
return parseInt(i,10);
});
},postCreate:function(){
this.inherited(arguments);
this.value.sort(_10);
var _12=this;
var _13=_1(_14,{constructor:function(){
this.widget=_12;
}});
this._movableMax=new _b(this.sliderHandleMax,{mover:_13});
this.sliderHandle.setAttribute("aria-valuemin",this.minimum);
this.sliderHandle.setAttribute("aria-valuemax",this.maximum);
this.sliderHandleMax.setAttribute("aria-valuemin",this.minimum);
this.sliderHandleMax.setAttribute("aria-valuemax",this.maximum);
var _15=_1(_4d,{constructor:function(){
this.widget=_12;
}});
this._movableBar=new _b(this.progressBar,{mover:_15});
this.focusNode.removeAttribute("aria-valuemin");
this.focusNode.removeAttribute("aria-valuemax");
this.focusNode.removeAttribute("aria-valuenow");
},destroy:function(){
this.inherited(arguments);
this._movableMax.destroy();
this._movableBar.destroy();
},_onKeyPress:function(e){
if(this.disabled||this.readOnly||e.altKey||e.ctrlKey){
return;
}
var _16=e.target===this.sliderHandleMax;
var _17=e.target===this.progressBar;
var k=_2.delegate(_8,this.isLeftToRight()?{PREV_ARROW:_8.LEFT_ARROW,NEXT_ARROW:_8.RIGHT_ARROW}:{PREV_ARROW:_8.RIGHT_ARROW,NEXT_ARROW:_8.LEFT_ARROW});
var _18=0;
var _19=false;
switch(e.keyCode){
case k.HOME:
this._setValueAttr(this.minimum,true,_16);
_4.stop(e);
return;
case k.END:
this._setValueAttr(this.maximum,true,_16);
_4.stop(e);
return;
case k.PREV_ARROW:
case k.DOWN_ARROW:
_19=true;
case k.NEXT_ARROW:
case k.UP_ARROW:
_18=1;
break;
case k.PAGE_DOWN:
_19=true;
case k.PAGE_UP:
_18=this.pageIncrement;
break;
default:
this.inherited(arguments);
return;
}
if(_19){
_18=-_18;
}
if(_18){
if(_17){
this._bumpValue([{change:_18,useMaxValue:false},{change:_18,useMaxValue:true}]);
}else{
this._bumpValue(_18,_16);
}
_4.stop(e);
}
},_onHandleClickMax:function(e){
if(this.disabled||this.readOnly){
return;
}
if(!_5("ie")){
_d.focus(this.sliderHandleMax);
}
_4.stop(e);
},_onClkIncBumper:function(){
this._setValueAttr(this._descending===false?this.minimum:this.maximum,true,true);
},_bumpValue:function(_1a,_1b){
var _1c=_2.isArray(_1a)?[this._getBumpValue(_1a[0].change,_1a[0].useMaxValue),this._getBumpValue(_1a[1].change,_1a[1].useMaxValue)]:this._getBumpValue(_1a,_1b);
this._setValueAttr(_1c,true,_1b);
},_getBumpValue:function(_1d,_1e){
var idx=_1e?1:0;
var s=_6.getComputedStyle(this.sliderBarContainer),c=_7.getContentBox(this.sliderBarContainer,s),_1f=this.discreteValues,_20=this.value[idx];
if(_1f<=1||_1f==Infinity){
_1f=c[this._pixelCount];
}
_1f--;
var _21=this.maximum>this.minimum?((_20-this.minimum)*_1f/(this.maximum-this.minimum)+_1d):0;
if(_21<0){
_21=0;
}
if(_21>_1f){
_21=_1f;
}
return _21*(this.maximum-this.minimum)/_1f+this.minimum;
},_onBarClick:function(e){
if(this.disabled||this.readOnly){
return;
}
if(!_5("ie")){
_d.focus(this.progressBar);
}
_4.stop(e);
},_onRemainingBarClick:function(e){
if(this.disabled||this.readOnly){
return;
}
if(!_5("ie")){
_d.focus(this.progressBar);
}
var _22=_7.position(this.sliderBarContainer,true),bar=_7.position(this.progressBar,true),_23=e[this._mousePixelCoord],_24=bar[this._startingPixelCoord],_25=_24+bar[this._pixelCount],_26=this._isReversed()?_23<=_24:_23>=_25,p=this._isReversed()?(_22[this._pixelCount]-_23+_22[this._startingPixelCoord]):(_23-_22[this._startingPixelCoord]);
this._setPixelValue(p,_22[this._pixelCount],true,_26);
_4.stop(e);
},_setPixelValue:function(_27,_28,_29,_2a){
if(this.disabled||this.readOnly){
return;
}
var _2b=this._getValueByPixelValue(_27,_28);
this._setValueAttr(_2b,_29,_2a);
},_getValueByPixelValue:function(_2c,_2d){
_2c=_2c<0?0:_2d<_2c?_2d:_2c;
var _2e=this.discreteValues;
if(_2e<=1||_2e==Infinity){
_2e=_2d;
}
_2e--;
var _2f=_2d/_2e;
var _30=Math.round(_2c/_2f);
return (this.maximum-this.minimum)*_30/_2e+this.minimum;
},_setValueAttr:function(_31,_32,_33){
var _34=_2.clone(this.value);
if(!_2.isArray(_31)){
_34[_33?1:0]=_31;
}else{
_34=_31;
}
this._lastValueReported="";
this.valueNode.value=_31=_34;
_34.sort(_10);
this.sliderHandle.setAttribute("aria-valuenow",_34[0]);
this.sliderHandleMax.setAttribute("aria-valuenow",_34[1]);
_c.prototype._setValueAttr.apply(this,arguments);
this._printSliderBar(_32,_33);
},_printSliderBar:function(_35,_36){
var _37=this.maximum>this.minimum?((this.value[0]-this.minimum)/(this.maximum-this.minimum)):0;
var _38=this.maximum>this.minimum?((this.value[1]-this.minimum)/(this.maximum-this.minimum)):0;
var _39=_37;
if(_37>_38){
_37=_38;
_38=_39;
}
var _3a=this._isReversed()?((1-_37)*100):(_37*100);
var _3b=this._isReversed()?((1-_38)*100):(_38*100);
var _3c=this._isReversed()?((1-_38)*100):(_37*100);
if(_35&&this.slideDuration>0&&this.progressBar.style[this._progressPixelSize]){
var _3d=_36?_38:_37;
var _3e=this;
var _3f={};
var _40=parseFloat(this.progressBar.style[this._handleOffsetCoord]);
var _41=this.slideDuration/10;
if(_41===0){
return;
}
if(_41<0){
_41=0-_41;
}
var _42={};
var _43={};
var _44={};
_42[this._handleOffsetCoord]={start:this.sliderHandle.parentNode.style[this._handleOffsetCoord],end:_3a,units:"%"};
_43[this._handleOffsetCoord]={start:this.sliderHandleMax.parentNode.style[this._handleOffsetCoord],end:_3b,units:"%"};
_44[this._handleOffsetCoord]={start:this.progressBar.style[this._handleOffsetCoord],end:_3c,units:"%"};
_44[this._progressPixelSize]={start:this.progressBar.style[this._progressPixelSize],end:(_38-_37)*100,units:"%"};
var _45=fx.animateProperty({node:this.sliderHandle.parentNode,duration:_41,properties:_42});
var _46=fx.animateProperty({node:this.sliderHandleMax.parentNode,duration:_41,properties:_43});
var _47=fx.animateProperty({node:this.progressBar,duration:_41,properties:_44});
var _48=_e.combine([_45,_46,_47]);
_48.play();
}else{
this.sliderHandle.parentNode.style[this._handleOffsetCoord]=_3a+"%";
this.sliderHandleMax.parentNode.style[this._handleOffsetCoord]=_3b+"%";
this.progressBar.style[this._handleOffsetCoord]=_3c+"%";
this.progressBar.style[this._progressPixelSize]=((_38-_37)*100)+"%";
}
}});
var _14=_1("dijit.form._SliderMoverMax",_a,{onMouseMove:function(e){
var _49=this.widget;
var _4a=_49._abspos;
if(!_4a){
_4a=_49._abspos=_7.position(_49.sliderBarContainer,true);
_49._setPixelValue_=_2.hitch(_49,"_setPixelValue");
_49._isReversed_=_49._isReversed();
}
var _4b=e[_49._mousePixelCoord]-_4a[_49._startingPixelCoord];
_49._setPixelValue_(_49._isReversed_?(_4a[_49._pixelCount]-_4b):_4b,_4a[_49._pixelCount],false,true);
},destroy:function(e){
_a.prototype.destroy.apply(this,arguments);
var _4c=this.widget;
_4c._abspos=null;
_4c._setValueAttr(_4c.value,true);
}});
var _4d=_1("dijit.form._SliderBarMover",_a,{onMouseMove:function(e){
var _4e=this.widget;
if(_4e.disabled||_4e.readOnly){
return;
}
var _4f=_4e._abspos;
var bar=_4e._bar;
var _50=_4e._mouseOffset;
if(!_4f){
_4f=_4e._abspos=_7.position(_4e.sliderBarContainer,true);
_4e._setPixelValue_=_2.hitch(_4e,"_setPixelValue");
_4e._getValueByPixelValue_=_2.hitch(_4e,"_getValueByPixelValue");
_4e._isReversed_=_4e._isReversed();
}
if(!bar){
bar=_4e._bar=_7.position(_4e.progressBar,true);
}
if(!_50){
_50=_4e._mouseOffset=e[_4e._mousePixelCoord]-bar[_4e._startingPixelCoord];
}
var _51=e[_4e._mousePixelCoord]-_4f[_4e._startingPixelCoord]-_50,_52=_51+bar[_4e._pixelCount];
pixelValues=[_51,_52];
pixelValues.sort(_10);
if(pixelValues[0]<=0){
pixelValues[0]=0;
pixelValues[1]=bar[_4e._pixelCount];
}
if(pixelValues[1]>=_4f[_4e._pixelCount]){
pixelValues[1]=_4f[_4e._pixelCount];
pixelValues[0]=_4f[_4e._pixelCount]-bar[_4e._pixelCount];
}
var _53=[_4e._getValueByPixelValue(_4e._isReversed_?(_4f[_4e._pixelCount]-pixelValues[0]):pixelValues[0],_4f[_4e._pixelCount]),_4e._getValueByPixelValue(_4e._isReversed_?(_4f[_4e._pixelCount]-pixelValues[1]):pixelValues[1],_4f[_4e._pixelCount])];
_4e._setValueAttr(_53,false,false);
},destroy:function(){
_a.prototype.destroy.apply(this,arguments);
var _54=this.widget;
_54._abspos=null;
_54._bar=null;
_54._mouseOffset=null;
_54._setValueAttr(_54.value,true);
}});
return _11;
});
