//>>built
define("dojox/mobile/Slider",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dijit/_WidgetBase","dijit/form/_FormValueMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _3("dojox.mobile.Slider",[_a,_b],{value:0,min:0,max:100,step:1,baseClass:"mblSlider",flip:false,orientation:"auto",halo:"8pt",buildRendering:function(){
this.focusNode=this.domNode=_7.create("div",{});
this.valueNode=_7.create("input",(this.srcNodeRef&&this.srcNodeRef.name)?{type:"hidden",name:this.srcNodeRef.name}:{type:"hidden"},this.domNode,"last");
var _c=_7.create("div",{style:{position:"relative",height:"100%",width:"100%"}},this.domNode,"last");
this.progressBar=_7.create("div",{style:{position:"absolute"},"class":"mblSliderProgressBar"},_c,"last");
this.touchBox=_7.create("div",{style:{position:"absolute"},"class":"mblSliderTouchBox"},_c,"last");
this.handle=_7.create("div",{style:{position:"absolute"},"class":"mblSliderHandle"},_c,"last");
this.inherited(arguments);
},_setValueAttr:function(_d,_e){
var _f=(this.value-this.min)*100/(this.max-this.min);
this.valueNode.value=_d;
this.inherited(arguments);
if(!this._started){
return;
}
this.focusNode.setAttribute("aria-valuenow",_d);
var _10=(_d-this.min)*100/(this.max-this.min);
var _11=this.orientation!="V";
if(_e===true){
_6.add(this.handle,"mblSliderTransition");
_6.add(this.progressBar,"mblSliderTransition");
}else{
_6.remove(this.handle,"mblSliderTransition");
_6.remove(this.progressBar,"mblSliderTransition");
}
_9.set(this.handle,this._attrs.handleLeft,(this._reversed?(100-_10):_10)+"%");
_9.set(this.progressBar,this._attrs.width,_10+"%");
},postCreate:function(){
this.inherited(arguments);
function _12(e){
function _13(e){
_21=_14?e[this._attrs.pageX]:(e.touches?e.touches[0][this._attrs.pageX]:e[this._attrs.clientX]);
_22=_21-_15;
_22=Math.min(Math.max(_22,0),_16);
var _17=this.step?((this.max-this.min)/this.step):_16;
if(_17<=1||_17==Infinity){
_17=_16;
}
var _18=Math.round(_22*_17/_16);
_1f=(this.max-this.min)*_18/_17;
_1f=this._reversed?(this.max-_1f):(this.min+_1f);
};
function _19(e){
e.preventDefault();
_4.hitch(this,_13)(e);
this.set("value",_1f,false);
};
function _1a(e){
e.preventDefault();
_1.forEach(_1b,_4.hitch(this,"disconnect"));
_1b=[];
this.set("value",this.value,true);
};
e.preventDefault();
var _14=e.type=="mousedown";
var box=_8.position(_1c,false);
var _1d=_9.get(_5.body(),"zoom")||1;
if(isNaN(_1d)){
_1d=1;
}
var _1e=_9.get(_1c,"zoom")||1;
if(isNaN(_1e)){
_1e=1;
}
var _15=box[this._attrs.x]*_1e*_1d+_8.docScroll()[this._attrs.x];
var _16=box[this._attrs.w]*_1e*_1d;
_4.hitch(this,_13)(e);
if(e.target==this.touchBox){
this.set("value",_1f,true);
}
_1.forEach(_1b,_2.disconnect);
var _20=_5.doc.documentElement;
var _1b=[this.connect(_20,_14?"onmousemove":"ontouchmove",_19),this.connect(_20,_14?"onmouseup":"ontouchend",_1a)];
};
var _21,_22,_1f;
var _1c=this.domNode;
if(this.orientation=="auto"){
this.orientation=_1c.offsetHeight<=_1c.offsetWidth?"H":"V";
}
_6.add(this.domNode,_1.map(this.baseClass.split(" "),_4.hitch(this,function(c){
return c+this.orientation;
})));
var _23=this.orientation!="V";
var ltr=_23?this.isLeftToRight():false;
var _24=this.flip;
this._reversed=!(_23&&((ltr&&!_24)||(!ltr&&_24)))||(!_23&&!_24);
this._attrs=_23?{x:"x",w:"w",l:"l",r:"r",pageX:"pageX",clientX:"clientX",handleLeft:"left",left:this._reversed?"right":"left",width:"width"}:{x:"y",w:"h",l:"t",r:"b",pageX:"pageY",clientX:"clientY",handleLeft:"top",left:this._reversed?"bottom":"top",width:"height"};
this.progressBar.style[this._attrs.left]="0px";
this.connect(this.touchBox,"touchstart",_12);
this.connect(this.touchBox,"onmousedown",_12);
this.connect(this.handle,"touchstart",_12);
this.connect(this.handle,"onmousedown",_12);
this.startup();
this.set("value",this.value);
}});
});
