//>>built
define("dojox/mobile/app/ListSelector",["dojo","dijit","dojox","dojo/require!dojox/mobile/app/_Widget,dojo/fx"],function(_1,_2,_3){
_1.provide("dojox.mobile.app.ListSelector");
_1.experimental("dojox.mobile.app.ListSelector");
_1.require("dojox.mobile.app._Widget");
_1.require("dojo.fx");
_1.declare("dojox.mobile.app.ListSelector",_3.mobile.app._Widget,{data:null,controller:null,onChoose:null,destroyOnHide:false,_setDataAttr:function(_4){
this.data=_4;
if(this.data){
this.render();
}
},postCreate:function(){
_1.addClass(this.domNode,"listSelector");
var _5=this;
this.connect(this.domNode,"onclick",function(_6){
if(!_1.hasClass(_6.target,"listSelectorRow")){
return;
}
if(_5.onChoose){
_5.onChoose(_5.data[_6.target._idx].value);
}
_5.hide();
});
this.connect(this.domNode,"onmousedown",function(_7){
if(!_1.hasClass(_7.target,"listSelectorRow")){
return;
}
_1.addClass(_7.target,"listSelectorRow-selected");
});
this.connect(this.domNode,"onmouseup",function(_8){
if(!_1.hasClass(_8.target,"listSelectorRow")){
return;
}
_1.removeClass(_8.target,"listSelectorRow-selected");
});
this.connect(this.domNode,"onmouseout",function(_9){
if(!_1.hasClass(_9.target,"listSelectorRow")){
return;
}
_1.removeClass(_9.target,"listSelectorRow-selected");
});
var _a=this.controller.getWindowSize();
this.mask=_1.create("div",{"class":"dialogUnderlayWrapper",innerHTML:"<div class=\"dialogUnderlay\"></div>"},this.controller.assistant.domNode);
this.connect(this.mask,"onclick",function(){
_5.onChoose&&_5.onChoose();
_5.hide();
});
},show:function(_b){
var _c;
var _d=this.controller.getWindowSize();
var _e;
if(_b){
_e=_1._abs(_b);
_c=_e;
}else{
_c.x=_d.w/2;
_c.y=200;
}
_1.style(this.domNode,{opacity:0,display:"",width:Math.floor(_d.w*0.8)+"px"});
var _f=0;
_1.query(">",this.domNode).forEach(function(_10){
_1.style(_10,{"float":"left"});
_f=Math.max(_f,_1.marginBox(_10).w);
_1.style(_10,{"float":"none"});
});
_f=Math.min(_f,Math.round(_d.w*0.8))+_1.style(this.domNode,"paddingLeft")+_1.style(this.domNode,"paddingRight")+1;
_1.style(this.domNode,"width",_f+"px");
var _11=_1.marginBox(this.domNode).h;
var _12=this;
var _13=_e?Math.max(30,_e.y-_11-10):this.getScroll().y+30;
var _14=_1.animateProperty({node:this.domNode,duration:400,properties:{width:{start:1,end:_f},height:{start:1,end:_11},top:{start:_c.y,end:_13},left:{start:_c.x,end:(_d.w/2-_f/2)},opacity:{start:0,end:1},fontSize:{start:1}},onEnd:function(){
_1.style(_12.domNode,"width","inherit");
}});
var _15=_1.fadeIn({node:this.mask,duration:400});
_1.fx.combine([_14,_15]).play();
},hide:function(){
var _16=this;
var _17=_1.animateProperty({node:this.domNode,duration:500,properties:{width:{end:1},height:{end:1},opacity:{end:0},fontSize:{end:1}},onEnd:function(){
if(_16.get("destroyOnHide")){
_16.destroy();
}
}});
var _18=_1.fadeOut({node:this.mask,duration:400});
_1.fx.combine([_17,_18]).play();
},render:function(){
_1.empty(this.domNode);
_1.style(this.domNode,"opacity",0);
var row;
for(var i=0;i<this.data.length;i++){
row=_1.create("div",{"class":"listSelectorRow "+(this.data[i].className||""),innerHTML:this.data[i].label},this.domNode);
row._idx=i;
if(i==0){
_1.addClass(row,"first");
}
if(i==this.data.length-1){
_1.addClass(row,"last");
}
}
},destroy:function(){
this.inherited(arguments);
_1.destroy(this.mask);
}});
});
