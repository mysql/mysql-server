//>>built
define("dojox/mobile/Slider",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/sniff","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/keys","dojo/touch","dijit/_WidgetBase","dijit/form/_FormValueMixin","./common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
return _3("dojox.mobile.Slider",[_d,_e],{value:0,min:0,max:100,step:1,baseClass:"mblSlider",flip:false,orientation:"auto",halo:"8pt",buildRendering:function(){
if(!this.templateString){
this.focusNode=this.domNode=_8.create("div",{});
this.valueNode=_8.create("input",(this.srcNodeRef&&this.srcNodeRef.name)?{type:"hidden",name:this.srcNodeRef.name}:{type:"hidden"},this.domNode,"last");
var _10=_8.create("div",{style:{position:"relative",height:"100%",width:"100%"}},this.domNode,"last");
this.progressBar=_8.create("div",{style:{position:"absolute"},"class":"mblSliderProgressBar"},_10,"last");
this.touchBox=_8.create("div",{style:{position:"absolute"},"class":"mblSliderTouchBox"},_10,"last");
this.handle=_8.create("div",{style:{position:"absolute"},"class":"mblSliderHandle"},_10,"last");
this.handle.setAttribute("role","slider");
this.handle.setAttribute("tabindex",0);
}
this.inherited(arguments);
_f._setTouchAction(this.domNode,"none");
},_setMinAttr:function(min){
this.handle.setAttribute("aria-valuemin",min);
this._set("min",min);
},_setMaxAttr:function(max){
this.handle.setAttribute("aria-valuemax",max);
this._set("max",max);
},_setValueAttr:function(_11,_12){
_11=Math.max(Math.min(_11,this.max),this.min);
var _13=(this.value-this.min)*100/(this.max-this.min);
this.valueNode.value=_11;
this.inherited(arguments);
if(!this._started){
return;
}
var _14=(_11-this.min)*100/(this.max-this.min);
var _15=this.orientation!="V";
if(_12===true){
_7.add(this.handle,"mblSliderTransition");
_7.add(this.progressBar,"mblSliderTransition");
}else{
_7.remove(this.handle,"mblSliderTransition");
_7.remove(this.progressBar,"mblSliderTransition");
}
_a.set(this.handle,this._attrs.handleLeft,(this._reversed?(100-_14):_14)+"%");
_a.set(this.progressBar,this._attrs.width,_14+"%");
this.handle.setAttribute("aria-valuenow",_11);
},postCreate:function(){
this.inherited(arguments);
function _16(e){
e.stopPropagation();
e.target.focus();
function _17(e){
_2c=_18?e[this._attrs.pageX]:(e.touches?e.touches[0][this._attrs.pageX]:e[this._attrs.clientX]);
_2d=_2c-_19;
_2d=Math.min(Math.max(_2d,0),_1a);
var _1b=this.step?((this.max-this.min)/this.step):_1a;
if(_1b<=1||_1b==Infinity){
_1b=_1a;
}
var _1c=Math.round(_2d*_1b/_1a);
_23=(this.max-this.min)*_1c/_1b;
_23=this._reversed?(this.max-_23):(this.min+_23);
};
function _1d(e){
e.preventDefault();
_4.hitch(this,_17)(e);
this.set("value",_23,false);
};
function _1e(e){
e.preventDefault();
_1.forEach(_1f,_4.hitch(this,"disconnect"));
_1f=[];
this.set("value",this.value,true);
};
e.preventDefault();
var _18=e.type=="mousedown";
var box=_9.position(_20,false);
var _21=(_6("ie")||_6("trident")>6)?1:(_a.get(_5.body(),"zoom")||1);
if(isNaN(_21)){
_21=1;
}
var _22=(_6("ie")||_6("trident")>6)?1:(_a.get(_20,"zoom")||1);
if(isNaN(_22)){
_22=1;
}
var _19=box[this._attrs.x]*_22*_21+_9.docScroll()[this._attrs.x];
var _1a=box[this._attrs.w]*_22*_21;
_4.hitch(this,_17)(e);
if(e.target==this.touchBox){
this.set("value",_23,true);
}
_1.forEach(_1f,_2.disconnect);
var _24=_5.doc.documentElement;
var _1f=[this.connect(_24,_c.move,_1d),this.connect(_24,_c.release,_1e)];
};
function _25(e){
if(this.disabled||this.readOnly||e.altKey||e.ctrlKey||e.metaKey){
return;
}
var _26=this.step,_27=1,_28;
switch(e.keyCode){
case _b.HOME:
_28=this.min;
break;
case _b.END:
_28=this.max;
break;
case _b.RIGHT_ARROW:
_27=-1;
case _b.LEFT_ARROW:
_28=this.value+_27*((_29&&_2a)?_26:-_26);
break;
case _b.DOWN_ARROW:
_27=-1;
case _b.UP_ARROW:
_28=this.value+_27*((!_29||_2a)?_26:-_26);
break;
default:
return;
}
e.preventDefault();
this._setValueAttr(_28,false);
};
function _2b(e){
if(this.disabled||this.readOnly||e.altKey||e.ctrlKey||e.metaKey){
return;
}
this._setValueAttr(this.value,true);
};
var _2c,_2d,_23,_20=this.domNode;
if(this.orientation=="auto"){
this.orientation=_20.offsetHeight<=_20.offsetWidth?"H":"V";
}
_7.add(this.domNode,_1.map(this.baseClass.split(" "),_4.hitch(this,function(c){
return c+this.orientation;
})));
var _2a=this.orientation!="V",ltr=_2a?this.isLeftToRight():false,_29=!!this.flip;
this._reversed=!((_2a&&((ltr&&!_29)||(!ltr&&_29)))||(!_2a&&_29));
this._attrs=_2a?{x:"x",w:"w",l:"l",r:"r",pageX:"pageX",clientX:"clientX",handleLeft:"left",left:this._reversed?"right":"left",width:"width"}:{x:"y",w:"h",l:"t",r:"b",pageX:"pageY",clientX:"clientY",handleLeft:"top",left:this._reversed?"bottom":"top",width:"height"};
this.progressBar.style[this._attrs.left]="0px";
this.connect(this.touchBox,_c.press,_16);
this.connect(this.handle,_c.press,_16);
this.connect(this.domNode,"onkeypress",_25);
this.connect(this.domNode,"onkeyup",_2b);
this.startup();
this.set("value",this.value);
}});
});
