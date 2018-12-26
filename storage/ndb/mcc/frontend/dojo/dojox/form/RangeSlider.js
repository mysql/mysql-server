//>>built
require({cache:{"url:dojox/form/resources/HorizontalRangeSlider.html":"<table class=\"dijit dijitReset dijitSlider dijitSliderH dojoxRangeSlider\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\" rules=\"none\" dojoAttachEvent=\"onkeypress:_onKeyPress,onkeyup:_onKeyUp\"\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset\" colspan=\"2\"></td\n\t\t><td dojoAttachPoint=\"topDecoration\" class=\"dijitReset dijitSliderDecoration dijitSliderDecorationT dijitSliderDecorationH\"></td\n\t\t><td class=\"dijitReset\" colspan=\"2\"></td\n\t></tr\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset dijitSliderButtonContainer dijitSliderButtonContainerH\"\n\t\t\t><div class=\"dijitSliderDecrementIconH\" tabIndex=\"-1\" style=\"display:none\" dojoAttachPoint=\"decrementButton\"><span class=\"dijitSliderButtonInner\">-</span></div\n\t\t></td\n\t\t><td class=\"dijitReset\"\n\t\t\t><div class=\"dijitSliderBar dijitSliderBumper dijitSliderBumperH dijitSliderLeftBumper\" dojoAttachEvent=\"onmousedown:_onClkDecBumper\"></div\n\t\t></td\n\t\t><td class=\"dijitReset\"\n\t\t\t><input dojoAttachPoint=\"valueNode\" type=\"hidden\" ${!nameAttrSetting}\n\t\t\t/><div role=\"presentation\" class=\"dojoxRangeSliderBarContainer\" dojoAttachPoint=\"sliderBarContainer\"\n\t\t\t\t><div dojoAttachPoint=\"sliderHandle\" tabIndex=\"${tabIndex}\" class=\"dijitSliderMoveable dijitSliderMoveableH\" dojoAttachEvent=\"onmousedown:_onHandleClick\" role=\"slider\" valuemin=\"${minimum}\" valuemax=\"${maximum}\"\n\t\t\t\t\t><div class=\"dijitSliderImageHandle dijitSliderImageHandleH\"></div\n\t\t\t\t></div\n\t\t\t\t><div role=\"presentation\" dojoAttachPoint=\"progressBar,focusNode\" class=\"dijitSliderBar dijitSliderBarH dijitSliderProgressBar dijitSliderProgressBarH\" dojoAttachEvent=\"onmousedown:_onBarClick\"></div\n\t\t\t\t><div dojoAttachPoint=\"sliderHandleMax,focusNodeMax\" tabIndex=\"${tabIndex}\" class=\"dijitSliderMoveable dijitSliderMoveableH\" dojoAttachEvent=\"onmousedown:_onHandleClickMax\" role=\"sliderMax\" valuemin=\"${minimum}\" valuemax=\"${maximum}\"\n\t\t\t\t\t><div class=\"dijitSliderImageHandle dijitSliderImageHandleH\"></div\n\t\t\t\t></div\n\t\t\t\t><div role=\"presentation\" dojoAttachPoint=\"remainingBar\" class=\"dijitSliderBar dijitSliderBarH dijitSliderRemainingBar dijitSliderRemainingBarH\" dojoAttachEvent=\"onmousedown:_onRemainingBarClick\"></div\n\t\t\t></div\n\t\t></td\n\t\t><td class=\"dijitReset\"\n\t\t\t><div class=\"dijitSliderBar dijitSliderBumper dijitSliderBumperH dijitSliderRightBumper\" dojoAttachEvent=\"onmousedown:_onClkIncBumper\"></div\n\t\t></td\n\t\t><td class=\"dijitReset dijitSliderButtonContainer dijitSliderButtonContainerH\"\n\t\t\t><div class=\"dijitSliderIncrementIconH\" tabIndex=\"-1\" style=\"display:none\" dojoAttachPoint=\"incrementButton\"><span class=\"dijitSliderButtonInner\">+</span></div\n\t\t></td\n\t></tr\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset\" colspan=\"2\"></td\n\t\t><td dojoAttachPoint=\"containerNode,bottomDecoration\" class=\"dijitReset dijitSliderDecoration dijitSliderDecorationB dijitSliderDecorationH\"></td\n\t\t><td class=\"dijitReset\" colspan=\"2\"></td\n\t></tr\n></table>\n","url:dojox/form/resources/VerticalRangeSlider.html":"<table class=\"dijitReset dijitSlider dijitSliderV dojoxRangeSlider\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\" rules=\"none\"\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset\"></td\n\t\t><td class=\"dijitReset dijitSliderButtonContainer dijitSliderButtonContainerV\"\n\t\t\t><div class=\"dijitSliderIncrementIconV\" tabIndex=\"-1\" style=\"display:none\" dojoAttachPoint=\"decrementButton\" dojoAttachEvent=\"onclick: increment\"><span class=\"dijitSliderButtonInner\">+</span></div\n\t\t></td\n\t\t><td class=\"dijitReset\"></td\n\t></tr\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset\"></td\n\t\t><td class=\"dijitReset\"\n\t\t\t><center><div class=\"dijitSliderBar dijitSliderBumper dijitSliderBumperV dijitSliderTopBumper\" dojoAttachEvent=\"onclick:_onClkIncBumper\"></div></center\n\t\t></td\n\t\t><td class=\"dijitReset\"></td\n\t></tr\n\t><tr class=\"dijitReset\"\n\t\t><td dojoAttachPoint=\"leftDecoration\" class=\"dijitReset dijitSliderDecoration dijitSliderDecorationL dijitSliderDecorationV\" style=\"text-align:center;height:100%;\"></td\n\t\t><td class=\"dijitReset\" style=\"height:100%;\"\n\t\t\t><input dojoAttachPoint=\"valueNode\" type=\"hidden\" ${!nameAttrSetting}\n\t\t\t/><center role=\"presentation\" style=\"position:relative;height:100%;\" dojoAttachPoint=\"sliderBarContainer\"\n\t\t\t\t><div role=\"presentation\" dojoAttachPoint=\"remainingBar\" class=\"dijitSliderBar dijitSliderBarV dijitSliderRemainingBar dijitSliderRemainingBarV\" dojoAttachEvent=\"onmousedown:_onRemainingBarClick\"\n\t\t\t\t\t><div dojoAttachPoint=\"sliderHandle\" tabIndex=\"${tabIndex}\" class=\"dijitSliderMoveable dijitSliderMoveableV\" dojoAttachEvent=\"onkeypress:_onKeyPress,onmousedown:_onHandleClick\" style=\"vertical-align:top;\" role=\"slider\" valuemin=\"${minimum}\" valuemax=\"${maximum}\"\n\t\t\t\t\t\t><div class=\"dijitSliderImageHandle dijitSliderImageHandleV\"></div\n\t\t\t\t\t></div\n\t\t\t\t\t><div role=\"presentation\" dojoAttachPoint=\"progressBar,focusNode\" tabIndex=\"${tabIndex}\" class=\"dijitSliderBar dijitSliderBarV dijitSliderProgressBar dijitSliderProgressBarV\" dojoAttachEvent=\"onkeypress:_onKeyPress,onmousedown:_onBarClick\"\n\t\t\t\t\t></div\n\t\t\t\t\t><div dojoAttachPoint=\"sliderHandleMax,focusNodeMax\" tabIndex=\"${tabIndex}\" class=\"dijitSliderMoveable dijitSliderMoveableV\" dojoAttachEvent=\"onkeypress:_onKeyPress,onmousedown:_onHandleClickMax\" style=\"vertical-align:top;\" role=\"slider\" valuemin=\"${minimum}\" valuemax=\"${maximum}\"\n\t\t\t\t\t\t><div class=\"dijitSliderImageHandle dijitSliderImageHandleV\"></div\n\t\t\t\t\t></div\n\t\t\t\t></div\n\t\t\t></center\n\t\t></td\n\t\t><td dojoAttachPoint=\"containerNode,rightDecoration\" class=\"dijitReset dijitSliderDecoration dijitSliderDecorationR dijitSliderDecorationV\" style=\"text-align:center;height:100%;\"></td\n\t></tr\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset\"></td\n\t\t><td class=\"dijitReset\"\n\t\t\t><center><div class=\"dijitSliderBar dijitSliderBumper dijitSliderBumperV dijitSliderBottomBumper\" dojoAttachEvent=\"onclick:_onClkDecBumper\"></div></center\n\t\t></td\n\t\t><td class=\"dijitReset\"></td\n\t></tr\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset\"></td\n\t\t><td class=\"dijitReset dijitSliderButtonContainer dijitSliderButtonContainerV\"\n\t\t\t><div class=\"dijitSliderDecrementIconV\" tabIndex=\"-1\" style=\"display:none\" dojoAttachPoint=\"incrementButton\" dojoAttachEvent=\"onclick: decrement\"><span class=\"dijitSliderButtonInner\">-</span></div\n\t\t></td\n\t\t><td class=\"dijitReset\"></td\n\t></tr\n></table>\n"}});
define("dojox/form/RangeSlider",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/fx","dojo/_base/event","dojo/_base/sniff","dojo/dom-style","dojo/dom-geometry","dojo/keys","dijit","dojo/dnd/Mover","dojo/dnd/Moveable","dojo/text!./resources/HorizontalRangeSlider.html","dojo/text!./resources/VerticalRangeSlider.html","dijit/form/HorizontalSlider","dijit/form/VerticalSlider","dijit/form/_FormValueWidget","dijit/focus","dojo/fx","dojox/fx"],function(_1,_2,_3,fx,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12){
var _13=function(a,b){
return b-a;
},_14=function(a,b){
return a-b;
};
_2.getObject("form",true,dojox);
var _15=_1("dojox.form._RangeSliderMixin",null,{value:[0,100],postMixInProperties:function(){
this.inherited(arguments);
this.value=_3.map(this.value,function(i){
return parseInt(i,10);
});
},postCreate:function(){
this.inherited(arguments);
this.value.sort(this._isReversed()?_13:_14);
var _16=this;
var _17=_1(_18,{constructor:function(){
this.widget=_16;
}});
this._movableMax=new _b(this.sliderHandleMax,{mover:_17});
this.focusNodeMax.setAttribute("aria-valuemin",this.minimum);
this.focusNodeMax.setAttribute("aria-valuemax",this.maximum);
var _19=_1(_52,{constructor:function(){
this.widget=_16;
}});
this._movableBar=new _b(this.progressBar,{mover:_19});
},destroy:function(){
this.inherited(arguments);
this._movableMax.destroy();
this._movableBar.destroy();
},_onKeyPress:function(e){
if(this.disabled||this.readOnly||e.altKey||e.ctrlKey){
return;
}
var _1a=e.target===this.sliderHandleMax;
var _1b=e.target===this.progressBar;
var k=_2.delegate(_8,this.isLeftToRight()?{PREV_ARROW:_8.LEFT_ARROW,NEXT_ARROW:_8.RIGHT_ARROW}:{PREV_ARROW:_8.RIGHT_ARROW,NEXT_ARROW:_8.LEFT_ARROW});
var _1c=0;
var _1d=false;
switch(e.keyCode){
case k.HOME:
this._setValueAttr(this.minimum,true,_1a);
_4.stop(e);
return;
case k.END:
this._setValueAttr(this.maximum,true,_1a);
_4.stop(e);
return;
case k.PREV_ARROW:
case k.DOWN_ARROW:
_1d=true;
case k.NEXT_ARROW:
case k.UP_ARROW:
_1c=1;
break;
case k.PAGE_DOWN:
_1d=true;
case k.PAGE_UP:
_1c=this.pageIncrement;
break;
default:
this.inherited(arguments);
return;
}
if(_1d){
_1c=-_1c;
}
if(_1c){
if(_1b){
this._bumpValue([{change:_1c,useMaxValue:false},{change:_1c,useMaxValue:true}]);
}else{
this._bumpValue(_1c,_1a);
}
_4.stop(e);
}
},_onHandleClickMax:function(e){
if(this.disabled||this.readOnly){
return;
}
if(!_5("ie")){
_11.focus(this.sliderHandleMax);
}
_4.stop(e);
},_onClkIncBumper:function(){
this._setValueAttr(this._descending===false?this.minimum:this.maximum,true,true);
},_bumpValue:function(_1e,_1f){
var _20=_2.isArray(_1e)?[this._getBumpValue(_1e[0].change,_1e[0].useMaxValue),this._getBumpValue(_1e[1].change,_1e[1].useMaxValue)]:this._getBumpValue(_1e,_1f);
this._setValueAttr(_20,true,_1f);
},_getBumpValue:function(_21,_22){
var idx=_22?1:0;
if(this._isReversed()){
idx=1-idx;
}
var s=_6.getComputedStyle(this.sliderBarContainer),c=_7.getContentBox(this.sliderBarContainer,s),_23=this.discreteValues,_24=this.value[idx];
if(_23<=1||_23==Infinity){
_23=c[this._pixelCount];
}
_23--;
var _25=(_24-this.minimum)*_23/(this.maximum-this.minimum)+_21;
if(_25<0){
_25=0;
}
if(_25>_23){
_25=_23;
}
return _25*(this.maximum-this.minimum)/_23+this.minimum;
},_onBarClick:function(e){
if(this.disabled||this.readOnly){
return;
}
if(!_5("ie")){
_11.focus(this.progressBar);
}
_4.stop(e);
},_onRemainingBarClick:function(e){
if(this.disabled||this.readOnly){
return;
}
if(!_5("ie")){
_11.focus(this.progressBar);
}
var _26=_7.position(this.sliderBarContainer,true),bar=_7.position(this.progressBar,true),_27=e[this._mousePixelCoord]-_26[this._startingPixelCoord],_28=bar[this._startingPixelCoord],_29=_28+bar[this._pixelCount],_2a=this._isReversed()?_27<=_28:_27>=_29,p=this._isReversed()?_26[this._pixelCount]-_27:_27;
this._setPixelValue(p,_26[this._pixelCount],true,_2a);
_4.stop(e);
},_setPixelValue:function(_2b,_2c,_2d,_2e){
if(this.disabled||this.readOnly){
return;
}
var _2f=this._getValueByPixelValue(_2b,_2c);
this._setValueAttr(_2f,_2d,_2e);
},_getValueByPixelValue:function(_30,_31){
_30=_30<0?0:_31<_30?_31:_30;
var _32=this.discreteValues;
if(_32<=1||_32==Infinity){
_32=_31;
}
_32--;
var _33=_31/_32;
var _34=Math.round(_30/_33);
return (this.maximum-this.minimum)*_34/_32+this.minimum;
},_setValueAttr:function(_35,_36,_37){
var _38=this.value;
if(!_2.isArray(_35)){
if(_37){
if(this._isReversed()){
_38[0]=_35;
}else{
_38[1]=_35;
}
}else{
if(this._isReversed()){
_38[1]=_35;
}else{
_38[0]=_35;
}
}
}else{
_38=_35;
}
this._lastValueReported="";
this.valueNode.value=this.value=_35=_38;
this.focusNode.setAttribute("aria-valuenow",_38[0]);
this.focusNodeMax.setAttribute("aria-valuenow",_38[1]);
this.value.sort(this._isReversed()?_13:_14);
_10.prototype._setValueAttr.apply(this,arguments);
this._printSliderBar(_36,_37);
},_printSliderBar:function(_39,_3a){
var _3b=(this.value[0]-this.minimum)/(this.maximum-this.minimum);
var _3c=(this.value[1]-this.minimum)/(this.maximum-this.minimum);
var _3d=_3b;
if(_3b>_3c){
_3b=_3c;
_3c=_3d;
}
var _3e=this._isReversed()?((1-_3b)*100):(_3b*100);
var _3f=this._isReversed()?((1-_3c)*100):(_3c*100);
var _40=this._isReversed()?((1-_3c)*100):(_3b*100);
if(_39&&this.slideDuration>0&&this.progressBar.style[this._progressPixelSize]){
var _41=_3a?_3c:_3b;
var _42=this;
var _43={};
var _44=parseFloat(this.progressBar.style[this._handleOffsetCoord]);
var _45=this.slideDuration/10;
if(_45===0){
return;
}
if(_45<0){
_45=0-_45;
}
var _46={};
var _47={};
var _48={};
_46[this._handleOffsetCoord]={start:this.sliderHandle.style[this._handleOffsetCoord],end:_3e,units:"%"};
_47[this._handleOffsetCoord]={start:this.sliderHandleMax.style[this._handleOffsetCoord],end:_3f,units:"%"};
_48[this._handleOffsetCoord]={start:this.progressBar.style[this._handleOffsetCoord],end:_40,units:"%"};
_48[this._progressPixelSize]={start:this.progressBar.style[this._progressPixelSize],end:(_3c-_3b)*100,units:"%"};
var _49=fx.animateProperty({node:this.sliderHandle,duration:_45,properties:_46});
var _4a=fx.animateProperty({node:this.sliderHandleMax,duration:_45,properties:_47});
var _4b=fx.animateProperty({node:this.progressBar,duration:_45,properties:_48});
var _4c=_12.combine([_49,_4a,_4b]);
_4c.play();
}else{
this.sliderHandle.style[this._handleOffsetCoord]=_3e+"%";
this.sliderHandleMax.style[this._handleOffsetCoord]=_3f+"%";
this.progressBar.style[this._handleOffsetCoord]=_40+"%";
this.progressBar.style[this._progressPixelSize]=((_3c-_3b)*100)+"%";
}
}});
var _18=_1("dijit.form._SliderMoverMax",_9.form._SliderMover,{onMouseMove:function(e){
var _4d=this.widget;
var _4e=_4d._abspos;
if(!_4e){
_4e=_4d._abspos=_7.position(_4d.sliderBarContainer,true);
_4d._setPixelValue_=_2.hitch(_4d,"_setPixelValue");
_4d._isReversed_=_4d._isReversed();
}
var _4f=e.touches?e.touches[0]:e;
var _50=_4f[_4d._mousePixelCoord]-_4e[_4d._startingPixelCoord];
_4d._setPixelValue_(_4d._isReversed_?(_4e[_4d._pixelCount]-_50):_50,_4e[_4d._pixelCount],false,true);
},destroy:function(e){
_a.prototype.destroy.apply(this,arguments);
var _51=this.widget;
_51._abspos=null;
_51._setValueAttr(_51.value,true);
}});
var _52=_1("dijit.form._SliderBarMover",_a,{onMouseMove:function(e){
var _53=this.widget;
if(_53.disabled||_53.readOnly){
return;
}
var _54=_53._abspos;
var bar=_53._bar;
var _55=_53._mouseOffset;
if(!_54){
_54=_53._abspos=_7.position(_53.sliderBarContainer,true);
_53._setPixelValue_=_2.hitch(_53,"_setPixelValue");
_53._getValueByPixelValue_=_2.hitch(_53,"_getValueByPixelValue");
_53._isReversed_=_53._isReversed();
}
if(!bar){
bar=_53._bar=_7.position(_53.progressBar,true);
}
var _56=e.touches?e.touches[0]:e;
if(!_55){
_55=_53._mouseOffset=_56[_53._mousePixelCoord]-_54[_53._startingPixelCoord]-bar[_53._startingPixelCoord];
}
var _57=_56[_53._mousePixelCoord]-_54[_53._startingPixelCoord]-_55,_58=_57+bar[_53._pixelCount];
pixelValues=[_57,_58];
pixelValues.sort(_14);
if(pixelValues[0]<=0){
pixelValues[0]=0;
pixelValues[1]=bar[_53._pixelCount];
}
if(pixelValues[1]>=_54[_53._pixelCount]){
pixelValues[1]=_54[_53._pixelCount];
pixelValues[0]=_54[_53._pixelCount]-bar[_53._pixelCount];
}
var _59=[_53._getValueByPixelValue(_53._isReversed_?(_54[_53._pixelCount]-pixelValues[0]):pixelValues[0],_54[_53._pixelCount]),_53._getValueByPixelValue(_53._isReversed_?(_54[_53._pixelCount]-pixelValues[1]):pixelValues[1],_54[_53._pixelCount])];
_53._setValueAttr(_59,false,false);
},destroy:function(){
_a.prototype.destroy.apply(this,arguments);
var _5a=this.widget;
_5a._abspos=null;
_5a._bar=null;
_5a._mouseOffset=null;
_5a._setValueAttr(_5a.value,true);
}});
_1("dojox.form.HorizontalRangeSlider",[_e,_15],{templateString:_c});
_1("dojox.form.VerticalRangeSlider",[_f,_15],{templateString:_d});
return _15;
});
