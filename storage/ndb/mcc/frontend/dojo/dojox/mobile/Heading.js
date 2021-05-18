//>>built
define("dojox/mobile/Heading",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/dom-attr","dijit/registry","./common","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./ProgressIndicator","./ToolBarButton","./View","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/Heading"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,has,_12){
var dm=_4.getObject("dojox.mobile",true);
var _13=_3(has("dojo-bidi")?"dojox.mobile.NonBidiHeading":"dojox.mobile.Heading",[_e,_d,_c],{back:"",href:"",moveTo:"",transition:"slide",label:"",iconBase:"",tag:"h1",busy:false,progStyle:"mblProgWhite",baseClass:"mblHeading",buildRendering:function(){
if(!this.templateString){
this.domNode=this.containerNode=this.srcNodeRef||_5.doc.createElement(this.tag);
}
this.inherited(arguments);
if(!this.templateString){
if(!this.label){
_1.forEach(this.domNode.childNodes,function(n){
if(n.nodeType==3){
var v=_4.trim(n.nodeValue);
if(v){
this.label=v;
this.labelNode=_7.create("span",{innerHTML:v},n,"replace");
}
}
},this);
}
if(!this.labelNode){
this.labelNode=_7.create("span",null,this.domNode);
}
this.labelNode.className="mblHeadingSpanTitle";
this.labelDivNode=_7.create("div",{className:"mblHeadingDivTitle",innerHTML:this.labelNode.innerHTML},this.domNode);
}
if(this.labelDivNode){
_9.set(this.labelDivNode,"role","heading");
_9.set(this.labelDivNode,"aria-level","1");
}
_b.setSelectable(this.domNode,false);
},startup:function(){
if(this._started){
return;
}
var _14=this.getParent&&this.getParent();
if(!_14||!_14.resize){
var _15=this;
_15.defer(function(){
_15.resize();
});
}
this.inherited(arguments);
},resize:function(){
if(this.labelNode){
var _16,_17;
var _18=this.containerNode.childNodes;
for(var i=_18.length-1;i>=0;i--){
var c=_18[i];
if(c.nodeType===1&&_8.get(c,"display")!=="none"){
if(!_17&&_8.get(c,"float")==="right"){
_17=c;
}
if(!_16&&_8.get(c,"float")==="left"){
_16=c;
}
}
}
if(!this.labelNodeLen&&this.label){
this.labelNode.style.display="inline";
this.labelNodeLen=this.labelNode.offsetWidth;
this.labelNode.style.display="";
}
var bw=this.domNode.offsetWidth;
var rw=_17?bw-_17.offsetLeft+5:0;
var lw=_16?_16.offsetLeft+_16.offsetWidth+5:0;
var tw=this.labelNodeLen||0;
_6[bw-Math.max(rw,lw)*2>tw?"add":"remove"](this.domNode,"mblHeadingCenterTitle");
}
_1.forEach(this.getChildren(),function(_19){
if(_19.resize){
_19.resize();
}
});
},_setBackAttr:function(_1a){
this._set("back",_1a);
if(!this.backButton){
this.backButton=new _10({arrow:"left",label:_1a,moveTo:this.moveTo,back:!this.moveTo&&!this.href,href:this.href,transition:this.transition,transitionDir:-1,dir:this.isLeftToRight()?"ltr":"rtl"});
this.backButton.placeAt(this.domNode,"first");
}else{
this.backButton.set("label",_1a);
}
this.resize();
},_setMoveToAttr:function(_1b){
this._set("moveTo",_1b);
if(this.backButton){
this.backButton.set("moveTo",_1b);
this.backButton.set("back",!_1b&&!this.href);
}
},_setHrefAttr:function(_1c){
this._set("href",_1c);
if(this.backButton){
this.backButton.set("href",_1c);
this.backButton.set("back",!this.moveTo&&!_1c);
}
},_setTransitionAttr:function(_1d){
this._set("transition",_1d);
if(this.backButton){
this.backButton.set("transition",_1d);
}
},_setLabelAttr:function(_1e){
this._set("label",_1e);
this.labelNode.innerHTML=this.labelDivNode.innerHTML=this._cv?this._cv(_1e):_1e;
delete this.labelNodeLen;
},_setBusyAttr:function(_1f){
var _20=this._prog;
if(_1f){
if(!_20){
_20=this._prog=new _f({size:30,center:false});
_6.add(_20.domNode,this.progStyle);
}
_7.place(_20.domNode,this.domNode,"first");
_20.start();
}else{
if(_20){
_20.stop();
}
}
this._set("busy",_1f);
}});
return has("dojo-bidi")?_3("dojox.mobile.Heading",[_13,_12]):_13;
});
