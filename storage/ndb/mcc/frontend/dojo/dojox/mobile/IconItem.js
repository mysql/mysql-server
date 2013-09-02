//>>built
define("dojox/mobile/IconItem",["dojo/_base/kernel","dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dijit/registry","./common","./_ItemBase","./TransitionEvent"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e){
return _3("dojox.mobile.IconItem",_d,{lazy:false,requires:"",timeout:10,closeBtnClass:"mblDomButtonBlueMinus",closeBtnProp:null,templateString:"<div class=\"mblIconArea\" dojoAttachPoint=\"iconDivNode\">"+"<div><img src=\"${icon}\" dojoAttachPoint=\"iconNode\"></div><span dojoAttachPoint=\"labelNode1\"></span>"+"</div>",templateStringSub:"<li class=\"mblIconItemSub\" lazy=\"${lazy}\" style=\"display:none;\" dojoAttachPoint=\"contentNode\">"+"<h2 class=\"mblIconContentHeading\" dojoAttachPoint=\"closeNode\">"+"<div class=\"${closeBtnClass}\" style=\"position:absolute;left:4px;top:2px;\" dojoAttachPoint=\"closeIconNode\"></div><span dojoAttachPoint=\"labelNode2\"></span>"+"</h2>"+"<div class=\"mblContent\" dojoAttachPoint=\"containerNode\"></div>"+"</li>",createTemplate:function(s){
_2.forEach(["lazy","icon","closeBtnClass"],function(v){
while(s.indexOf("${"+v+"}")!=-1){
s=s.replace("${"+v+"}",this[v]);
}
},this);
var _f=_6.doc.createElement("DIV");
_f.innerHTML=s;
var _10=_f.getElementsByTagName("*");
var i,len,s1;
len=_10.length;
for(i=0;i<len;i++){
s1=_10[i].getAttribute("dojoAttachPoint");
if(s1){
this[s1]=_10[i];
}
}
if(this.closeIconNode&&this.closeBtnProp){
_7.set(this.closeIconNode,this.closeBtnProp);
}
var _11=_f.removeChild(_f.firstChild);
_f=null;
return _11;
},buildRendering:function(){
this.inheritParams();
var _12=this.createTemplate(this.templateString);
this.subNode=this.createTemplate(this.templateStringSub);
this.subNode._parentNode=this.domNode;
this.domNode=this.srcNodeRef||_9.create("LI");
_8.add(this.domNode,"mblIconItem");
if(this.srcNodeRef){
for(var i=0,len=this.srcNodeRef.childNodes.length;i<len;i++){
this.containerNode.appendChild(this.srcNodeRef.firstChild);
}
}
this.domNode.appendChild(_12);
},postCreate:function(){
_c.createDomButton(this.closeIconNode,{top:"-2px",left:"1px"});
this.connect(this.iconNode,"onmousedown","onMouseDownIcon");
this.connect(this.iconNode,"onclick","iconClicked");
this.connect(this.closeIconNode,"onclick","closeIconClicked");
this.connect(this.iconNode,"onerror","onError");
},highlight:function(){
_8.add(this.iconDivNode,"mblVibrate");
if(this.timeout>0){
var _13=this;
setTimeout(function(){
_13.unhighlight();
},this.timeout*1000);
}
},unhighlight:function(){
_8.remove(this.iconDivNode,"mblVibrate");
},instantiateWidget:function(e){
var _14=this.containerNode.getElementsByTagName("*");
var len=_14.length;
var s;
for(var i=0;i<len;i++){
s=_14[i].getAttribute("dojoType");
if(s){
_1["require"](s);
}
}
if(len>0){
_1.parser.parse(this.containerNode);
}
this.lazy=false;
},isOpen:function(e){
return this.containerNode.style.display!="none";
},onMouseDownIcon:function(e){
_a.set(this.iconNode,"opacity",this.getParent().pressedIconOpacity);
},iconClicked:function(e){
if(e){
this.setTransitionPos(e);
setTimeout(_4.hitch(this,function(d){
this.iconClicked();
}),0);
return;
}
if(this.href&&this.hrefTarget){
_c.openWindow(this.href,this.hrefTarget);
_1.style(this.iconNode,"opacity",1);
return;
}
var _15;
if(this.moveTo||this.href||this.url||this.scene){
_15={moveTo:this.moveTo,href:this.href,url:this.url,scene:this.scene,transitionDir:this.transitionDir,transition:this.transition};
}else{
if(this.transitionOptions){
_15=this.transitionOptions;
}
}
if(_15){
setTimeout(_4.hitch(this,function(d){
_a.set(this.iconNode,"opacity",1);
}),1500);
}else{
return this.open(e);
}
if(_15){
return new _e(this.domNode,_15,e).dispatch();
}
},closeIconClicked:function(e){
if(e){
setTimeout(_4.hitch(this,function(d){
this.closeIconClicked();
}),0);
return;
}
this.close();
},open:function(e){
var _16=this.getParent();
if(this.transition=="below"){
if(_16.single){
_16.closeAll();
_a.set(this.iconNode,"opacity",this.getParent().pressedIconOpacity);
}
this._open_1();
}else{
_16._opening=this;
if(_16.single){
this.closeNode.style.display="none";
_16.closeAll();
var _17=_b.byId(_16.id+"_mblApplView");
_17._heading._setLabelAttr(this.label);
}
var _18=this.transitionOptions||{transition:this.transition,transitionDir:this.transitionDir,moveTo:_16.id+"_mblApplView"};
new _e(this.domNode,_18,e).dispatch();
}
},_open_1:function(){
this.contentNode.style.display="";
this.unhighlight();
if(this.lazy){
if(this.requires){
_2.forEach(this.requires.split(/,/),function(c){
_1["require"](c);
});
}
this.instantiateWidget();
}
this.contentNode.scrollIntoView();
this.onOpen();
},close:function(){
if(_5("webkit")){
var t=this.domNode.parentNode.offsetWidth/8;
var y=this.iconNode.offsetLeft;
var pos=0;
for(var i=1;i<=3;i++){
if(t*(2*i-1)<y&&y<=t*(2*(i+1)-1)){
pos=i;
break;
}
}
_8.add(this.containerNode.parentNode,"mblCloseContent mblShrink"+pos);
}else{
this.containerNode.parentNode.style.display="none";
}
_a.set(this.iconNode,"opacity",1);
this.onClose();
},onOpen:function(){
},onClose:function(){
},onError:function(){
var _19=this.getParent().defaultIcon;
if(_19){
this.iconNode.src=_19;
}
},_setIconAttr:function(_1a){
if(!this.getParent()){
return;
}
this.icon=_1a;
_c.createIcon(_1a,this.iconPos,this.iconNode,this.alt);
if(this.iconPos){
_8.add(this.iconNode,"mblIconItemSpriteIcon");
var arr=this.iconPos.split(/[ ,]/);
var p=this.iconNode.parentNode;
_a.set(p,{width:arr[2]+"px",top:Math.round((p.offsetHeight-arr[3])/2)+1+"px",margin:"auto"});
}
},_setLabelAttr:function(_1b){
this.label=_1b;
var s=this._cv?this._cv(_1b):_1b;
this.labelNode1.innerHTML=s;
this.labelNode2.innerHTML=s;
}});
});
