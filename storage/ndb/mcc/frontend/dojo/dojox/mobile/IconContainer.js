//>>built
define("dojox/mobile/IconContainer",["dojo/_base/array","dojo/_base/declare","dojo/_base/window","dojo/dom-construct","dojo/dom-style","dijit/registry","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./IconItem","./Heading","./View"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
return _2("dojox.mobile.IconContainer",[_9,_8,_7],{defaultIcon:"",transition:"below",pressedIconOpacity:0.4,iconBase:"",iconPos:"",back:"Home",label:"My Application",single:false,buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_3.doc.createElement("UL");
this.domNode.className="mblIconContainer";
var t=this._terminator=_4.create("LI");
t.className="mblIconItemTerminator";
t.innerHTML="&nbsp;";
this.domNode.appendChild(t);
},_setupSubNodes:function(ul){
_1.forEach(this.getChildren(),function(w){
ul.appendChild(w.subNode);
});
},startup:function(){
if(this._started){
return;
}
if(this.transition==="below"){
this._setupSubNodes(this.domNode);
}else{
var _d=this.appView=new _c({id:this.id+"_mblApplView"});
var _e=this;
_d.onAfterTransitionIn=function(_f,dir,_10,_11,_12){
_e._opening._open_1();
};
_d.domNode.style.visibility="hidden";
var _13=_d._heading=new _b({back:this._cv?this._cv(this.back):this.back,label:this._cv?this._cv(this.label):this.label,moveTo:this.domNode.parentNode.id,transition:this.transition});
_d.addChild(_13);
var ul=_d._ul=_3.doc.createElement("UL");
ul.className="mblIconContainer";
ul.style.marginTop="0px";
this._setupSubNodes(ul);
_d.domNode.appendChild(ul);
var _14;
for(var w=this.getParent();w;w=w.getParent()){
if(w instanceof _c){
_14=w.domNode.parentNode;
break;
}
}
if(!_14){
_14=_3.body();
}
_14.appendChild(_d.domNode);
_d.startup();
}
this.inherited(arguments);
},closeAll:function(){
var len=this.domNode.childNodes.length,_15,w;
for(var i=0;i<len;i++){
var _15=this.domNode.childNodes[i];
if(_15.nodeType!==1){
continue;
}
if(_15===this._terminator){
break;
}
var w=_6.byNode(_15);
w.containerNode.parentNode.style.display="none";
_5.set(w.iconNode,"opacity",1);
}
},addChild:function(_16,_17){
var _18=this.getChildren();
if(typeof _17!=="number"||_17>_18.length){
_17=_18.length;
}
var idx=_17;
var _19=this.containerNode;
if(idx>0){
_19=_18[idx-1].domNode;
idx="after";
}
_4.place(_16.domNode,_19,idx);
_16.transition=this.transition;
if(this.transition==="below"){
for(var i=0,_19=this._terminator;i<_17;i++){
_19=_19.nextSibling;
}
_4.place(_16.subNode,_19,"after");
}else{
_4.place(_16.subNode,this.appView._ul,_17);
}
_16.inheritParams();
_16._setIconAttr(_16.icon);
if(this._started&&!_16._started){
_16.startup();
}
},removeChild:function(_1a){
if(typeof _1a==="number"){
_1a=this.getChildren()[_1a];
}
if(_1a){
this.inherited(arguments);
if(this.transition==="below"){
this.containerNode.removeChild(_1a.subNode);
}else{
this.appView._ul.removeChild(_1a.subNode);
}
}
}});
});
