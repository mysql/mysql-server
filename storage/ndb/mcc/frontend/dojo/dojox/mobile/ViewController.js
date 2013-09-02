//>>built
define("dojox/mobile/ViewController",["dojo/_base/kernel","dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/on","dojo/ready","dijit/registry","./ProgressIndicator","./TransitionEvent"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,on,_a,_b,_c,_d){
var dm=_5.getObject("dojox.mobile",true);
var _e=_4("dojox.mobile.ViewController",null,{constructor:function(){
this.viewMap={};
this.currentView=null;
this.defaultView=null;
_a(_5.hitch(this,function(){
on(_6.body(),"startTransition",_5.hitch(this,"onStartTransition"));
}));
},findCurrentView:function(_f,src){
if(_f){
var w=_b.byId(_f);
if(w&&w.getShowingView){
return w.getShowingView();
}
}
if(dm.currentView){
return dm.currentView;
}
w=src;
while(true){
w=w.getParent();
if(!w){
return null;
}
if(_8.contains(w.domNode,"mblView")){
break;
}
}
return w;
},onStartTransition:function(evt){
evt.preventDefault();
if(!evt.detail||(evt.detail&&!evt.detail.moveTo&&!evt.detail.href&&!evt.detail.url&&!evt.detail.scene)){
return;
}
var w=this.findCurrentView(evt.detail.moveTo,(evt.target&&evt.target.id)?_b.byId(evt.target.id):_b.byId(evt.target));
if(!w||(evt.detail&&evt.detail.moveTo&&w===_b.byId(evt.detail.moveTo))){
return;
}
if(evt.detail.href){
var t=_b.byId(evt.target.id).hrefTarget;
if(t){
dm.openWindow(evt.detail.href,t);
}else{
w.performTransition(null,evt.detail.transitionDir,evt.detail.transition,evt.target,function(){
location.href=evt.detail.href;
});
}
return;
}else{
if(evt.detail.scene){
_3.publish("/dojox/mobile/app/pushScene",[evt.detail.scene]);
return;
}
}
var _10=evt.detail.moveTo;
if(evt.detail.url){
var id;
if(dm._viewMap&&dm._viewMap[evt.detail.url]){
id=dm._viewMap[evt.detail.url];
}else{
var _11=this._text;
if(!_11){
if(_b.byId(evt.target.id).sync){
_1.xhrGet({url:evt.detail.url,sync:true,load:function(_12){
_11=_5.trim(_12);
}});
}else{
var s="dojo/_base/xhr";
require([s],_5.hitch(this,function(xhr){
var _13=_c.getInstance();
_6.body().appendChild(_13.domNode);
_13.start();
var obj=xhr.get({url:evt.detail.url,handleAs:"text"});
obj.addCallback(_5.hitch(this,function(_14,_15){
_13.stop();
if(_14){
this._text=_14;
new _d(evt.target,{transition:evt.detail.transition,transitionDir:evt.detail.transitionDir,moveTo:_10,href:evt.detail.href,url:evt.detail.url,scene:evt.detail.scene},evt.detail).dispatch();
}
}));
obj.addErrback(function(_16){
_13.stop();
});
}));
return;
}
}
this._text=null;
id=this._parse(_11,_b.byId(evt.target.id).urlTarget);
if(!dm._viewMap){
dm._viewMap=[];
}
dm._viewMap[evt.detail.url]=id;
}
_10=id;
w=this.findCurrentView(_10,_b.byId(evt.target.id))||w;
}
w.performTransition(_10,evt.detail.transitionDir,evt.detail.transition,null,null);
},_parse:function(_17,id){
var _18,_19,i,j,len;
var _1a=this.findCurrentView();
var _1b=_b.byId(id)&&_b.byId(id).containerNode||_7.byId(id)||_1a&&_1a.domNode.parentNode||_6.body();
var _1c=null;
for(j=_1b.childNodes.length-1;j>=0;j--){
var c=_1b.childNodes[j];
if(c.nodeType===1){
if(c.getAttribute("fixed")==="bottom"){
_1c=c;
}
break;
}
}
if(_17.charAt(0)==="<"){
_18=_9.create("DIV",{innerHTML:_17});
for(i=0;i<_18.childNodes.length;i++){
var n=_18.childNodes[i];
if(n.nodeType===1){
_19=n;
break;
}
}
if(!_19){
return;
}
_19.style.visibility="hidden";
_1b.insertBefore(_18,_1c);
var ws=_1.parser.parse(_18);
_2.forEach(ws,function(w){
if(w&&!w._started&&w.startup){
w.startup();
}
});
for(i=0,len=_18.childNodes.length;i<len;i++){
_1b.insertBefore(_18.firstChild,_1c);
}
_1b.removeChild(_18);
_b.byNode(_19)._visible=true;
}else{
if(_17.charAt(0)==="{"){
_18=_9.create("DIV");
_1b.insertBefore(_18,_1c);
this._ws=[];
_19=this._instantiate(eval("("+_17+")"),_18);
for(i=0;i<this._ws.length;i++){
var w=this._ws[i];
w.startup&&!w._started&&(!w.getParent||!w.getParent())&&w.startup();
}
this._ws=null;
}
}
_19.style.display="none";
_19.style.visibility="visible";
return _1.hash?"#"+_19.id:_19.id;
},_instantiate:function(obj,_1d,_1e){
var _1f;
for(var key in obj){
if(key.charAt(0)=="@"){
continue;
}
var cls=_5.getObject(key);
if(!cls){
continue;
}
var _20={};
var _21=cls.prototype;
var _22=_5.isArray(obj[key])?obj[key]:[obj[key]];
for(var i=0;i<_22.length;i++){
for(var _23 in _22[i]){
if(_23.charAt(0)=="@"){
var val=_22[i][_23];
_23=_23.substring(1);
if(typeof _21[_23]=="string"){
_20[_23]=val;
}else{
if(typeof _21[_23]=="number"){
_20[_23]=val-0;
}else{
if(typeof _21[_23]=="boolean"){
_20[_23]=(val!="false");
}else{
if(typeof _21[_23]=="object"){
_20[_23]=eval("("+val+")");
}
}
}
}
}
}
_1f=new cls(_20,_1d);
if(_1d){
_1f._visible=true;
this._ws.push(_1f);
}
if(_1e&&_1e.addChild){
_1e.addChild(_1f);
}
this._instantiate(_22[i],null,_1f);
}
}
return _1f&&_1f.domNode;
}});
new _e();
return _e;
});
