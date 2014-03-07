//>>built
define("dojox/layout/RadioGroup",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/html","dojo/_base/lang","dojo/_base/query","dijit/_Widget","dijit/_Templated","dijit/_Contained","dijit/layout/StackContainer","dojo/fx/easing","dojo/_base/fx","dojo/dom-construct","dojo/dom-class"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
_1.experimental("dojox.layout.RadioGroup");
var _e=_2("dojox.layout.RadioGroup",[_9,_7],{duration:750,hasButtons:false,buttonClass:"dojox.layout._RadioButton",templateString:"<div class=\"dojoxRadioGroup\">"+" \t<div dojoAttachPoint=\"buttonHolder\" style=\"display:none;\">"+"\t\t<table class=\"dojoxRadioButtons\"><tbody><tr class=\"dojoxRadioButtonRow\" dojoAttachPoint=\"buttonNode\"></tr></tbody></table>"+"\t</div>"+"\t<div class=\"dojoxRadioView\" dojoAttachPoint=\"containerNode\"></div>"+"</div>",startup:function(){
this.inherited(arguments);
this._children=this.getChildren();
this._buttons=this._children.length;
this._size=_3.coords(this.containerNode);
if(this.hasButtons){
_3.style(this.buttonHolder,"display","block");
}
},_setupChild:function(_f){
_3.style(_f.domNode,"position","absolute");
if(this.hasButtons){
var tmp=this.buttonNode.appendChild(_c.create("td"));
var n=_c.create("div",null,tmp),_10=_4.getObject(this.buttonClass),_11=new _10({label:_f.title,page:_f},n);
_4.mixin(_f,{_radioButton:_11});
_11.startup();
}
_f.domNode.style.display="none";
},removeChild:function(_12){
if(this.hasButtons&&_12._radioButton){
_12._radioButton.destroy();
delete _12._radioButton;
}
this.inherited(arguments);
},_transition:function(_13,_14){
this._showChild(_13);
if(_14){
this._hideChild(_14);
}
if(this.doLayout&&_13.resize){
_13.resize(this._containerContentBox||this._contentBox);
}
},_showChild:function(_15){
var _16=this.getChildren();
_15.isFirstChild=(_15==_16[0]);
_15.isLastChild=(_15==_16[_16.length-1]);
_15.selected=true;
_15.domNode.style.display="";
if(_15._onShow){
_15._onShow();
}else{
if(_15.onShow){
_15.onShow();
}
}
},_hideChild:function(_17){
_17.selected=false;
_17.domNode.style.display="none";
if(_17.onHide){
_17.onHide();
}
}});
_2("dojox.layout.RadioGroupFade",_e,{_hideChild:function(_18){
_b.fadeOut({node:_18.domNode,duration:this.duration,onEnd:_4.hitch(this,"inherited",arguments,arguments)}).play();
},_showChild:function(_19){
this.inherited(arguments);
_3.style(_19.domNode,"opacity",0);
_b.fadeIn({node:_19.domNode,duration:this.duration}).play();
}});
_2("dojox.layout.RadioGroupSlide",_e,{easing:"dojo.fx.easing.backOut",zTop:99,constructor:function(){
if(_4.isString(this.easing)){
this.easing=_4.getObject(this.easing);
}
},_positionChild:function(_1a){
if(!this._size){
return;
}
var rA=true,rB=true;
switch(_1a.slideFrom){
case "bottom":
rB=!rB;
break;
case "right":
rA=!rA;
rB=!rB;
break;
case "top":
break;
case "left":
rA=!rA;
break;
default:
rA=Math.round(Math.random());
rB=Math.round(Math.random());
break;
}
var _1b=rA?"top":"left",val=(rB?"-":"")+(this._size[rA?"h":"w"]+20)+"px";
_3.style(_1a.domNode,_1b,val);
},_showChild:function(_1c){
var _1d=this.getChildren();
_1c.isFirstChild=(_1c==_1d[0]);
_1c.isLastChild=(_1c==_1d[_1d.length-1]);
_1c.selected=true;
_3.style(_1c.domNode,{zIndex:this.zTop,display:""});
if(this._anim&&this._anim.status()=="playing"){
this._anim.gotoPercent(100,true);
}
this._anim=_b.animateProperty({node:_1c.domNode,properties:{left:0,top:0},duration:this.duration,easing:this.easing,onEnd:_4.hitch(_1c,function(){
if(this.onShow){
this.onShow();
}
if(this._onShow){
this._onShow();
}
}),beforeBegin:_4.hitch(this,"_positionChild",_1c)});
this._anim.play();
},_hideChild:function(_1e){
_1e.selected=false;
_1e.domNode.style.zIndex=this.zTop-1;
if(_1e.onHide){
_1e.onHide();
}
}});
_2("dojox.layout._RadioButton",[_6,_7,_8],{label:"",page:null,templateString:"<div dojoAttachPoint=\"focusNode\" class=\"dojoxRadioButton\"><span dojoAttachPoint=\"titleNode\" class=\"dojoxRadioButtonLabel\">${label}</span></div>",startup:function(){
this.connect(this.domNode,"onmouseenter","_onMouse");
},_onMouse:function(e){
this.getParent().selectChild(this.page);
this._clearSelected();
_d.add(this.domNode,"dojoxRadioButtonSelected");
},_clearSelected:function(){
_5(".dojoxRadioButtonSelected",this.domNode.parentNode.parentNode).removeClass("dojoxRadioButtonSelected");
}});
_4.extend(_6,{slideFrom:"random"});
});
