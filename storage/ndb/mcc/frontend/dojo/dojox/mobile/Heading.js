//>>built
define("dojox/mobile/Heading",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dijit/registry","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./View"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
var dm=_4.getObject("dojox.mobile",true);
return _3("dojox.mobile.Heading",[_c,_b,_a],{back:"",href:"",moveTo:"",transition:"slide",label:"",iconBase:"",backProp:{className:"mblArrowButton"},tag:"H1",buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_5.doc.createElement(this.tag);
this.domNode.className="mblHeading";
if(!this.label){
_1.forEach(this.domNode.childNodes,function(n){
if(n.nodeType==3){
var v=_4.trim(n.nodeValue);
if(v){
this.label=v;
this.labelNode=_7.create("SPAN",{innerHTML:v},n,"replace");
}
}
},this);
}
if(!this.labelNode){
this.labelNode=_7.create("SPAN",null,this.domNode);
}
this.labelNode.className="mblHeadingSpanTitle";
this.labelDivNode=_7.create("DIV",{className:"mblHeadingDivTitle",innerHTML:this.labelNode.innerHTML},this.domNode);
},startup:function(){
if(this._started){
return;
}
var _e=this.getParent&&this.getParent();
if(!_e||!_e.resize){
var _f=this;
setTimeout(function(){
_f.resize();
},0);
}
this.inherited(arguments);
},resize:function(){
if(this._btn){
this._btn.style.width=this._body.offsetWidth+this._head.offsetWidth+"px";
}
if(this.labelNode){
var _10,_11;
var _12=this.containerNode.childNodes;
for(var i=_12.length-1;i>=0;i--){
var c=_12[i];
if(c.nodeType===1){
if(!_11&&_6.contains(c,"mblToolBarButton")&&_8.get(c,"float")==="right"){
_11=c;
}
if(!_10&&(_6.contains(c,"mblToolBarButton")&&_8.get(c,"float")==="left"||c===this._btn)){
_10=c;
}
}
}
if(!this.labelNodeLen&&this.label){
this.labelNode.style.display="inline";
this.labelNodeLen=this.labelNode.offsetWidth;
this.labelNode.style.display="";
}
var bw=this.domNode.offsetWidth;
var rw=_11?bw-_11.offsetLeft+5:0;
var lw=_10?_10.offsetLeft+_10.offsetWidth+5:0;
var tw=this.labelNodeLen||0;
_6[bw-Math.max(rw,lw)*2>tw?"add":"remove"](this.domNode,"mblHeadingCenterTitle");
}
_1.forEach(this.getChildren(),function(_13){
if(_13.resize){
_13.resize();
}
});
},_setBackAttr:function(_14){
if(!_14){
_7.destroy(this._btn);
this._btn=null;
this.back="";
}else{
if(!this._btn){
var btn=_7.create("DIV",this.backProp,this.domNode,"first");
var _15=_7.create("DIV",{className:"mblArrowButtonHead"},btn);
var _16=_7.create("DIV",{className:"mblArrowButtonBody mblArrowButtonText"},btn);
this._body=_16;
this._head=_15;
this._btn=btn;
this.backBtnNode=btn;
this.connect(_16,"onclick","onClick");
}
this.back=_14;
this._body.innerHTML=this._cv?this._cv(this.back):this.back;
}
this.resize();
},_setLabelAttr:function(_17){
this.label=_17;
this.labelNode.innerHTML=this.labelDivNode.innerHTML=this._cv?this._cv(_17):_17;
},findCurrentView:function(){
var w=this;
while(true){
w=w.getParent();
if(!w){
return null;
}
if(w instanceof _d){
break;
}
}
return w;
},onClick:function(e){
var h1=this.domNode;
_6.add(h1,"mblArrowButtonSelected");
setTimeout(function(){
_6.remove(h1,"mblArrowButtonSelected");
},1000);
if(this.back&&!this.moveTo&&!this.href&&history){
history.back();
return;
}
var _18=this.findCurrentView();
if(_18){
_18.clickedPosX=e.clientX;
_18.clickedPosY=e.clientY;
}
this.goTo(this.moveTo,this.href);
},goTo:function(_19,_1a){
var _1b=this.findCurrentView();
if(!_1b){
return;
}
if(_1a){
_1b.performTransition(null,-1,this.transition,this,function(){
location.href=_1a;
});
}else{
if(dm.app&&dm.app.STAGE_CONTROLLER_ACTIVE){
_2.publish("/dojox/mobile/app/goback");
}else{
var _1c=_9.byId(_1b.convertToId(_19));
if(_1c){
var _1d=_1c.getParent();
while(_1b){
var _1e=_1b.getParent();
if(_1d===_1e){
break;
}
_1b=_1e;
}
}
if(_1b){
_1b.performTransition(_19,-1,this.transition);
}
}
}
}});
});
