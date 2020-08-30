//>>built
define("dojox/mobile/ProgressIndicator",["dojo/_base/config","dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/has","dijit/_Contained","dijit/_WidgetBase","./_css3","dojo/has!dojo-bidi?dojox/mobile/bidi/ProgressIndicator"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var _d=_2("dojox.mobile.ProgressIndicator",[_a,_9],{interval:100,size:40,removeOnStop:true,startSpinning:false,center:true,colors:null,baseClass:"mblProgressIndicator",constructor:function(){
this.colors=[];
this._bars=[];
},buildRendering:function(){
this.inherited(arguments);
if(this.center){
_4.add(this.domNode,"mblProgressIndicatorCenter");
}
this.containerNode=_5.create("div",{className:"mblProgContainer"},this.domNode);
this.spinnerNode=_5.create("div",null,this.containerNode);
for(var i=0;i<12;i++){
var _e=_5.create("div",{className:"mblProg mblProg"+i},this.spinnerNode);
this._bars.push(_e);
}
this.scale(this.size);
if(this.startSpinning){
this.start();
}
},scale:function(_f){
var _10=_f/40;
_7.set(this.containerNode,_b.add({},{transform:"scale("+_10+")",transformOrigin:"0 0"}));
_6.setMarginBox(this.domNode,{w:_f,h:_f});
_6.setMarginBox(this.containerNode,{w:_f/_10,h:_f/_10});
},start:function(){
if(this.imageNode){
var img=this.imageNode;
var l=Math.round((this.containerNode.offsetWidth-img.offsetWidth)/2);
var t=Math.round((this.containerNode.offsetHeight-img.offsetHeight)/2);
img.style.margin=t+"px "+l+"px";
return;
}
var _11=0;
var _12=this;
var n=12;
this.timer=setInterval(function(){
_11--;
_11=_11<0?n-1:_11;
var c=_12.colors;
for(var i=0;i<n;i++){
var idx=(_11+i)%n;
if(c[idx]){
_12._bars[i].style.backgroundColor=c[idx];
}else{
_4.replace(_12._bars[i],"mblProg"+idx+"Color","mblProg"+(idx===n-1?0:idx+1)+"Color");
}
}
},this.interval);
},stop:function(){
if(this.timer){
clearInterval(this.timer);
}
this.timer=null;
if(this.removeOnStop&&this.domNode&&this.domNode.parentNode){
this.domNode.parentNode.removeChild(this.domNode);
}
},setImage:function(_13){
if(_13){
this.imageNode=_5.create("img",{src:_13},this.containerNode);
this.spinnerNode.style.display="none";
}else{
if(this.imageNode){
this.containerNode.removeChild(this.imageNode);
this.imageNode=null;
}
this.spinnerNode.style.display="";
}
},destroy:function(){
this.inherited(arguments);
if(this===_d._instance){
_d._instance=null;
}
}});
_d=_8("dojo-bidi")?_2("dojox.mobile.ProgressIndicator",[_d,_c]):_d;
_d._instance=null;
_d.getInstance=function(_14){
if(!_d._instance){
_d._instance=new _d(_14);
}
return _d._instance;
};
return _d;
});
