//>>built
define("dojox/mobile/Slider",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/keys","dijit/_WidgetBase","dijit/form/_FormValueMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
return _3("dojox.mobile.Slider",[_b,_c],{value:0,min:0,max:100,step:1,baseClass:"mblSlider",flip:false,orientation:"auto",halo:"8pt",buildRendering:function(){
this.focusNode=this.domNode=_7.create("div",{});
this.valueNode=_7.create("input",(this.srcNodeRef&&this.srcNodeRef.name)?{type:"hidden",name:this.srcNodeRef.name}:{type:"hidden"},this.domNode,"last");
var _d=_7.create("div",{style:{position:"relative",height:"100%",width:"100%"}},this.domNode,"last");
this.progressBar=_7.create("div",{style:{position:"absolute"},"class":"mblSliderProgressBar"},_d,"last");
this.touchBox=_7.create("div",{style:{position:"absolute"},"class":"mblSliderTouchBox"},_d,"last");
this.handle=_7.create("div",{style:{position:"absolute"},"class":"mblSliderHandle"},_d,"last");
this.inherited(arguments);
},_setValueAttr:function(_e,_f){
_e=Math.max(Math.min(_e,this.max),this.min);
var _10=(this.value-this.min)*100/(this.max-this.min);
this.valueNode.value=_e;
this.inherited(arguments);
if(!this._started){
return;
}
this.focusNode.setAttribute("aria-valuenow",_e);
var _11=(_e-this.min)*100/(this.max-this.min);
var _12=this.orientation!="V";
if(_f===true){
_6.add(this.handle,"mblSliderTransition");
_6.add(this.progressBar,"mblSliderTransition");
}else{
_6.remove(this.handle,"mblSliderTransition");
_6.remove(this.progressBar,"mblSliderTransition");
}
_9.set(this.handle,this._attrs.handleLeft,(this._reversed?(100-_11):_11)+"%");
_9.set(this.progressBar,this._attrs.width,_11+"%");
},postCreate:function(){
this.inherited(arguments);
function _13(e){
function _14(e){
_29=_15?e[this._attrs.pageX]:(e.touches?e.touches[0][this._attrs.pageX]:e[this._attrs.clientX]);
_2a=_29-_16;
_2a=Math.min(Math.max(_2a,0),_17);
var _18=this.step?((this.max-this.min)/this.step):_17;
if(_18<=1||_18==Infinity){
_18=_17;
}
var _19=Math.round(_2a*_18/_17);
_20=(this.max-this.min)*_19/_18;
_20=this._reversed?(this.max-_20):(this.min+_20);
};
function _1a(e){
e.preventDefault();
_4.hitch(this,_14)(e);
this.set("value",_20,false);
};
function _1b(e){
e.preventDefault();
_1.forEach(_1c,_4.hitch(this,"disconnect"));
_1c=[];
this.set("value",this.value,true);
};
e.preventDefault();
var _15=e.type=="mousedown";
var box=_8.position(_1d,false);
var _1e=_9.get(_5.body(),"zoom")||1;
if(isNaN(_1e)){
_1e=1;
}
var _1f=_9.get(_1d,"zoom")||1;
if(isNaN(_1f)){
_1f=1;
}
var _16=box[this._attrs.x]*_1f*_1e+_8.docScroll()[this._attrs.x];
var _17=box[this._attrs.w]*_1f*_1e;
_4.hitch(this,_14)(e);
if(e.target==this.touchBox){
this.set("value",_20,true);
}
_1.forEach(_1c,_2.disconnect);
var _21=_5.doc.documentElement;
var _1c=[this.connect(_21,_15?"onmousemove":"ontouchmove",_1a),this.connect(_21,_15?"onmouseup":"ontouchend",_1b)];
};
function _22(e){
if(this.disabled||this.readOnly||e.altKey||e.ctrlKey||e.metaKey){
return;
}
var _23=this.step,_24=1,_25;
switch(e.keyCode){
case _a.HOME:
_25=this.min;
break;
case _a.END:
_25=this.max;
break;
case _a.RIGHT_ARROW:
_24=-1;
case _a.LEFT_ARROW:
_25=this.value+_24*((_26&&_27)?_23:-_23);
break;
case _a.DOWN_ARROW:
_24=-1;
case _a.UP_ARROW:
_25=this.value+_24*((!_26||_27)?_23:-_23);
break;
default:
return;
}
e.preventDefault();
this._setValueAttr(_25,false);
};
function _28(e){
if(this.disabled||this.readOnly||e.altKey||e.ctrlKey||e.metaKey){
return;
}
this._setValueAttr(this.value,true);
};
var _29,_2a,_20,_1d=this.domNode;
if(this.orientation=="auto"){
this.orientation=_1d.offsetHeight<=_1d.offsetWidth?"H":"V";
}
_6.add(this.domNode,_1.map(this.baseClass.split(" "),_4.hitch(this,function(c){
return c+this.orientation;
})));
var _27=this.orientation!="V",ltr=_27?this.isLeftToRight():false,_26=!!this.flip;
this._reversed=!((_27&&((ltr&&!_26)||(!ltr&&_26)))||(!_27&&_26));
this._attrs=_27?{x:"x",w:"w",l:"l",r:"r",pageX:"pageX",clientX:"clientX",handleLeft:"left",left:this._reversed?"right":"left",width:"width"}:{x:"y",w:"h",l:"t",r:"b",pageX:"pageY",clientX:"clientY",handleLeft:"top",left:this._reversed?"bottom":"top",width:"height"};
this.progressBar.style[this._attrs.left]="0px";
this.connect(this.touchBox,"ontouchstart",_13);
this.connect(this.touchBox,"onmousedown",_13);
this.connect(this.handle,"ontouchstart",_13);
this.connect(this.handle,"onmousedown",_13);
this.connect(this.domNode,"onkeypress",_22);
this.connect(this.domNode,"onkeyup",_28);
this.startup();
this.set("value",this.value);
}});
});
