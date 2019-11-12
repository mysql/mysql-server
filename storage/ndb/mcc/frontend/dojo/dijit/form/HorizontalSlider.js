//>>built
require({cache:{"url:dijit/form/templates/HorizontalSlider.html":"<table class=\"dijit dijitReset dijitSlider dijitSliderH\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\" rules=\"none\" data-dojo-attach-event=\"onkeypress:_onKeyPress,onkeyup:_onKeyUp\"\n\trole=\"presentation\"\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset\" colspan=\"2\"></td\n\t\t><td data-dojo-attach-point=\"topDecoration\" class=\"dijitReset dijitSliderDecoration dijitSliderDecorationT dijitSliderDecorationH\"></td\n\t\t><td class=\"dijitReset\" colspan=\"2\"></td\n\t></tr\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset dijitSliderButtonContainer dijitSliderButtonContainerH\"\n\t\t\t><div class=\"dijitSliderDecrementIconH\" style=\"display:none\" data-dojo-attach-point=\"decrementButton\"><span class=\"dijitSliderButtonInner\">-</span></div\n\t\t></td\n\t\t><td class=\"dijitReset\"\n\t\t\t><div class=\"dijitSliderBar dijitSliderBumper dijitSliderBumperH dijitSliderLeftBumper\" data-dojo-attach-event=\"press:_onClkDecBumper\"></div\n\t\t></td\n\t\t><td class=\"dijitReset\"\n\t\t\t><input data-dojo-attach-point=\"valueNode\" type=\"hidden\" ${!nameAttrSetting}\n\t\t\t/><div class=\"dijitReset dijitSliderBarContainerH\" role=\"presentation\" data-dojo-attach-point=\"sliderBarContainer\"\n\t\t\t\t><div role=\"presentation\" data-dojo-attach-point=\"progressBar\" class=\"dijitSliderBar dijitSliderBarH dijitSliderProgressBar dijitSliderProgressBarH\" data-dojo-attach-event=\"press:_onBarClick\"\n\t\t\t\t\t><div class=\"dijitSliderMoveable dijitSliderMoveableH\"\n\t\t\t\t\t\t><div data-dojo-attach-point=\"sliderHandle,focusNode\" class=\"dijitSliderImageHandle dijitSliderImageHandleH\" data-dojo-attach-event=\"press:_onHandleClick\" role=\"slider\"></div\n\t\t\t\t\t></div\n\t\t\t\t></div\n\t\t\t\t><div role=\"presentation\" data-dojo-attach-point=\"remainingBar\" class=\"dijitSliderBar dijitSliderBarH dijitSliderRemainingBar dijitSliderRemainingBarH\" data-dojo-attach-event=\"press:_onBarClick\"></div\n\t\t\t></div\n\t\t></td\n\t\t><td class=\"dijitReset\"\n\t\t\t><div class=\"dijitSliderBar dijitSliderBumper dijitSliderBumperH dijitSliderRightBumper\" data-dojo-attach-event=\"press:_onClkIncBumper\"></div\n\t\t></td\n\t\t><td class=\"dijitReset dijitSliderButtonContainer dijitSliderButtonContainerH\"\n\t\t\t><div class=\"dijitSliderIncrementIconH\" style=\"display:none\" data-dojo-attach-point=\"incrementButton\"><span class=\"dijitSliderButtonInner\">+</span></div\n\t\t></td\n\t></tr\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset\" colspan=\"2\"></td\n\t\t><td data-dojo-attach-point=\"containerNode,bottomDecoration\" class=\"dijitReset dijitSliderDecoration dijitSliderDecorationB dijitSliderDecorationH\"></td\n\t\t><td class=\"dijitReset\" colspan=\"2\"></td\n\t></tr\n></table>\n"}});
define("dijit/form/HorizontalSlider",["dojo/_base/array","dojo/_base/declare","dojo/dnd/move","dojo/_base/event","dojo/_base/fx","dojo/dom-geometry","dojo/dom-style","dojo/keys","dojo/_base/lang","dojo/sniff","dojo/dnd/Moveable","dojo/dnd/Mover","dojo/query","dojo/mouse","../registry","../focus","../typematic","./Button","./_FormValueWidget","../_Container","dojo/text!./templates/HorizontalSlider.html"],function(_1,_2,_3,_4,fx,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14){
var _15=_2("dijit.form._SliderMover",_b,{onMouseMove:function(e){
var _16=this.widget;
var _17=_16._abspos;
if(!_17){
_17=_16._abspos=_5.position(_16.sliderBarContainer,true);
_16._setPixelValue_=_8.hitch(_16,"_setPixelValue");
_16._isReversed_=_16._isReversed();
}
var _18=e[_16._mousePixelCoord]-_17[_16._startingPixelCoord];
_16._setPixelValue_(_16._isReversed_?(_17[_16._pixelCount]-_18):_18,_17[_16._pixelCount],false);
},destroy:function(e){
_b.prototype.destroy.apply(this,arguments);
var _19=this.widget;
_19._abspos=null;
_19._setValueAttr(_19.value,true);
}});
var _1a=_2("dijit.form.HorizontalSlider",[_12,_13],{templateString:_14,value:0,showButtons:true,minimum:0,maximum:100,discreteValues:Infinity,pageIncrement:2,clickSelect:true,slideDuration:_e.defaultDuration,_setIdAttr:"",baseClass:"dijitSlider",cssStateNodes:{incrementButton:"dijitSliderIncrementButton",decrementButton:"dijitSliderDecrementButton",focusNode:"dijitSliderThumb"},_mousePixelCoord:"pageX",_pixelCount:"w",_startingPixelCoord:"x",_handleOffsetCoord:"left",_progressPixelSize:"width",_onKeyUp:function(e){
if(this.disabled||this.readOnly||e.altKey||e.ctrlKey||e.metaKey){
return;
}
this._setValueAttr(this.value,true);
},_onKeyPress:function(e){
if(this.disabled||this.readOnly||e.altKey||e.ctrlKey||e.metaKey){
return;
}
switch(e.charOrCode){
case _7.HOME:
this._setValueAttr(this.minimum,false);
break;
case _7.END:
this._setValueAttr(this.maximum,false);
break;
case ((this._descending||this.isLeftToRight())?_7.RIGHT_ARROW:_7.LEFT_ARROW):
case (this._descending===false?_7.DOWN_ARROW:_7.UP_ARROW):
case (this._descending===false?_7.PAGE_DOWN:_7.PAGE_UP):
this.increment(e);
break;
case ((this._descending||this.isLeftToRight())?_7.LEFT_ARROW:_7.RIGHT_ARROW):
case (this._descending===false?_7.UP_ARROW:_7.DOWN_ARROW):
case (this._descending===false?_7.PAGE_UP:_7.PAGE_DOWN):
this.decrement(e);
break;
default:
return;
}
_4.stop(e);
},_onHandleClick:function(e){
if(this.disabled||this.readOnly){
return;
}
if(!_9("ie")){
_f.focus(this.sliderHandle);
}
_4.stop(e);
},_isReversed:function(){
return !this.isLeftToRight();
},_onBarClick:function(e){
if(this.disabled||this.readOnly||!this.clickSelect){
return;
}
_f.focus(this.sliderHandle);
_4.stop(e);
var _1b=_5.position(this.sliderBarContainer,true);
var _1c=e[this._mousePixelCoord]-_1b[this._startingPixelCoord];
this._setPixelValue(this._isReversed()?(_1b[this._pixelCount]-_1c):_1c,_1b[this._pixelCount],true);
this._movable.onMouseDown(e);
},_setPixelValue:function(_1d,_1e,_1f){
if(this.disabled||this.readOnly){
return;
}
var _20=this.discreteValues;
if(_20<=1||_20==Infinity){
_20=_1e;
}
_20--;
var _21=_1e/_20;
var _22=Math.round(_1d/_21);
this._setValueAttr(Math.max(Math.min((this.maximum-this.minimum)*_22/_20+this.minimum,this.maximum),this.minimum),_1f);
},_setValueAttr:function(_23,_24){
this._set("value",_23);
this.valueNode.value=_23;
this.focusNode.setAttribute("aria-valuenow",_23);
this.inherited(arguments);
var _25=(_23-this.minimum)/(this.maximum-this.minimum);
var _26=(this._descending===false)?this.remainingBar:this.progressBar;
var _27=(this._descending===false)?this.progressBar:this.remainingBar;
if(this._inProgressAnim&&this._inProgressAnim.status!="stopped"){
this._inProgressAnim.stop(true);
}
if(_24&&this.slideDuration>0&&_26.style[this._progressPixelSize]){
var _28=this;
var _29={};
var _2a=parseFloat(_26.style[this._progressPixelSize]);
var _2b=this.slideDuration*(_25-_2a/100);
if(_2b==0){
return;
}
if(_2b<0){
_2b=0-_2b;
}
_29[this._progressPixelSize]={start:_2a,end:_25*100,units:"%"};
this._inProgressAnim=fx.animateProperty({node:_26,duration:_2b,onAnimate:function(v){
_27.style[_28._progressPixelSize]=(100-parseFloat(v[_28._progressPixelSize]))+"%";
},onEnd:function(){
delete _28._inProgressAnim;
},properties:_29});
this._inProgressAnim.play();
}else{
_26.style[this._progressPixelSize]=(_25*100)+"%";
_27.style[this._progressPixelSize]=((1-_25)*100)+"%";
}
},_bumpValue:function(_2c,_2d){
if(this.disabled||this.readOnly){
return;
}
var s=_6.getComputedStyle(this.sliderBarContainer);
var c=_5.getContentBox(this.sliderBarContainer,s);
var _2e=this.discreteValues;
if(_2e<=1||_2e==Infinity){
_2e=c[this._pixelCount];
}
_2e--;
var _2f=(this.value-this.minimum)*_2e/(this.maximum-this.minimum)+_2c;
if(_2f<0){
_2f=0;
}
if(_2f>_2e){
_2f=_2e;
}
_2f=_2f*(this.maximum-this.minimum)/_2e+this.minimum;
this._setValueAttr(_2f,_2d);
},_onClkBumper:function(val){
if(this.disabled||this.readOnly||!this.clickSelect){
return;
}
this._setValueAttr(val,true);
},_onClkIncBumper:function(){
this._onClkBumper(this._descending===false?this.minimum:this.maximum);
},_onClkDecBumper:function(){
this._onClkBumper(this._descending===false?this.maximum:this.minimum);
},decrement:function(e){
this._bumpValue(e.charOrCode==_7.PAGE_DOWN?-this.pageIncrement:-1);
},increment:function(e){
this._bumpValue(e.charOrCode==_7.PAGE_UP?this.pageIncrement:1);
},_mouseWheeled:function(evt){
_4.stop(evt);
this._bumpValue(evt.wheelDelta<0?-1:1,true);
},startup:function(){
if(this._started){
return;
}
_1.forEach(this.getChildren(),function(_30){
if(this[_30.container]!=this.containerNode){
this[_30.container].appendChild(_30.domNode);
}
},this);
this.inherited(arguments);
},_typematicCallback:function(_31,_32,e){
if(_31==-1){
this._setValueAttr(this.value,true);
}else{
this[(_32==(this._descending?this.incrementButton:this.decrementButton))?"decrement":"increment"](e);
}
},buildRendering:function(){
this.inherited(arguments);
if(this.showButtons){
this.incrementButton.style.display="";
this.decrementButton.style.display="";
}
var _33=_c("label[for=\""+this.id+"\"]");
if(_33.length){
if(!_33[0].id){
_33[0].id=this.id+"_label";
}
this.focusNode.setAttribute("aria-labelledby",_33[0].id);
}
this.focusNode.setAttribute("aria-valuemin",this.minimum);
this.focusNode.setAttribute("aria-valuemax",this.maximum);
},postCreate:function(){
this.inherited(arguments);
if(this.showButtons){
this.own(_10.addMouseListener(this.decrementButton,this,"_typematicCallback",25,500),_10.addMouseListener(this.incrementButton,this,"_typematicCallback",25,500));
}
this.connect(this.domNode,_d.wheel,"_mouseWheeled");
var _34=_2(_15,{widget:this});
this._movable=new _a(this.sliderHandle,{mover:_34});
this._layoutHackIE7();
},destroy:function(){
this._movable.destroy();
if(this._inProgressAnim&&this._inProgressAnim.status!="stopped"){
this._inProgressAnim.stop(true);
}
this.inherited(arguments);
}});
_1a._Mover=_15;
return _1a;
});
