//>>built
define("dojox/mobile/Heading",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dijit/registry","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./ProgressIndicator","./ToolBarButton","./View"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
var dm=_4.getObject("dojox.mobile",true);
return _3("dojox.mobile.Heading",[_d,_c,_b],{back:"",href:"",moveTo:"",transition:"slide",label:"",iconBase:"",tag:"h1",busy:false,progStyle:"mblProgWhite",baseClass:"mblHeading",buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_5.doc.createElement(this.tag);
this.inherited(arguments);
if(!this.label){
_1.forEach(this.domNode.childNodes,function(n){
if(n.nodeType==3){
var v=_4.trim(n.nodeValue);
if(v){
this.label=v;
this.labelNode=_8.create("span",{innerHTML:v},n,"replace");
}
}
},this);
}
if(!this.labelNode){
this.labelNode=_8.create("span",null,this.domNode);
}
this.labelNode.className="mblHeadingSpanTitle";
this.labelDivNode=_8.create("div",{className:"mblHeadingDivTitle",innerHTML:this.labelNode.innerHTML},this.domNode);
_6.setSelectable(this.domNode,false);
},startup:function(){
if(this._started){
return;
}
var _11=this.getParent&&this.getParent();
if(!_11||!_11.resize){
var _12=this;
setTimeout(function(){
_12.resize();
},0);
}
this.inherited(arguments);
},resize:function(){
if(this.labelNode){
var _13,_14;
var _15=this.containerNode.childNodes;
for(var i=_15.length-1;i>=0;i--){
var c=_15[i];
if(c.nodeType===1&&_9.get(c,"display")!=="none"){
if(!_14&&_9.get(c,"float")==="right"){
_14=c;
}
if(!_13&&_9.get(c,"float")==="left"){
_13=c;
}
}
}
if(!this.labelNodeLen&&this.label){
this.labelNode.style.display="inline";
this.labelNodeLen=this.labelNode.offsetWidth;
this.labelNode.style.display="";
}
var bw=this.domNode.offsetWidth;
var rw=_14?bw-_14.offsetLeft+5:0;
var lw=_13?_13.offsetLeft+_13.offsetWidth+5:0;
var tw=this.labelNodeLen||0;
_7[bw-Math.max(rw,lw)*2>tw?"add":"remove"](this.domNode,"mblHeadingCenterTitle");
}
_1.forEach(this.getChildren(),function(_16){
if(_16.resize){
_16.resize();
}
});
},_setBackAttr:function(_17){
this._set("back",_17);
if(!this.backButton){
this.backButton=new _f({arrow:"left",label:_17,moveTo:this.moveTo,back:!this.moveTo,href:this.href,transition:this.transition,transitionDir:-1});
this.backButton.placeAt(this.domNode,"first");
}else{
this.backButton.set("label",_17);
}
this.resize();
},_setMoveToAttr:function(_18){
this._set("moveTo",_18);
if(this.backButton){
this.backButton.set("moveTo",_18);
}
},_setHrefAttr:function(_19){
this._set("href",_19);
if(this.backButton){
this.backButton.set("href",_19);
}
},_setTransitionAttr:function(_1a){
this._set("transition",_1a);
if(this.backButton){
this.backButton.set("transition",_1a);
}
},_setLabelAttr:function(_1b){
this._set("label",_1b);
this.labelNode.innerHTML=this.labelDivNode.innerHTML=this._cv?this._cv(_1b):_1b;
},_setBusyAttr:function(_1c){
var _1d=this._prog;
if(_1c){
if(!_1d){
_1d=this._prog=new _e({size:30,center:false});
_7.add(_1d.domNode,this.progStyle);
}
_8.place(_1d.domNode,this.domNode,"first");
_1d.start();
}else{
_1d.stop();
}
this._set("busy",_1c);
}});
});
