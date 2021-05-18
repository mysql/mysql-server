//>>built
define("dojox/mobile/ViewController",["dojo/_base/kernel","dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/_base/Deferred","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/on","dojo/ready","dijit/registry","./ProgressIndicator","./TransitionEvent","./viewRegistry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,on,_b,_c,_d,_e,_f){
var _10=_4("dojox.mobile.ViewController",null,{dataHandlerClass:"dojox/mobile/dh/DataHandler",dataSourceClass:"dojox/mobile/dh/UrlDataSource",fileTypeMapClass:"dojox/mobile/dh/SuffixFileTypeMap",constructor:function(){
this.viewMap={};
_b(_5.hitch(this,function(){
on(_6.body(),"startTransition",_5.hitch(this,"onStartTransition"));
}));
},findTransitionViews:function(_11){
if(!_11){
return [];
}
_11.match(/^#?([^&?]+)(.*)/);
var _12=RegExp.$2;
var _13=_c.byId(RegExp.$1);
if(!_13){
return [];
}
for(var v=_13.getParent();v;v=v.getParent()){
if(v.isVisible&&!v.isVisible()){
var sv=_13.getShowingView();
if(sv&&sv.id!==_13.id){
_13.show();
}
_13=v;
}
}
return [_13.getShowingView(),_13,_12];
},openExternalView:function(_14,_15){
var d=new _7();
var id=this.viewMap[_14.url];
if(id){
_14.moveTo=id;
if(_14.noTransition){
_c.byId(id).hide();
}else{
new _e(_6.body(),_14).dispatch();
}
d.resolve(true);
return d;
}
var _16=null;
for(var i=_15.childNodes.length-1;i>=0;i--){
var c=_15.childNodes[i];
if(c.nodeType===1){
var _17=c.getAttribute("fixed")||c.getAttribute("data-mobile-fixed")||(_c.byNode(c)&&_c.byNode(c).fixed);
if(_17==="bottom"){
_16=c;
break;
}
}
}
var dh=_14.dataHandlerClass||this.dataHandlerClass;
var ds=_14.dataSourceClass||this.dataSourceClass;
var ft=_14.fileTypeMapClass||this.fileTypeMapClass;
require([dh,ds,ft],_5.hitch(this,function(_18,_19,_1a){
var _1b=new _18(new _19(_14.data||_14.url),_15,_16);
var _1c=_14.contentType||_1a.getContentType(_14.url)||"html";
_1b.processData(_1c,_5.hitch(this,function(id){
if(id){
this.viewMap[_14.url]=_14.moveTo=id;
if(_14.noTransition){
_c.byId(id).hide();
}else{
new _e(_6.body(),_14).dispatch();
}
d.resolve(true);
}else{
d.reject("Failed to load "+_14.url);
}
}));
}));
return d;
},onStartTransition:function(evt){
evt.preventDefault();
if(!evt.detail){
return;
}
var _1d=evt.detail;
if(!_1d.moveTo&&!_1d.href&&!_1d.url&&!_1d.scene){
return;
}
if(_1d.url&&!_1d.moveTo){
var _1e=_1d.urlTarget;
var w=_c.byId(_1e);
var _1f=w&&w.containerNode||_8.byId(_1e);
if(!_1f){
w=_f.getEnclosingView(evt.target);
_1f=w&&w.domNode.parentNode||_6.body();
}
var src=_c.getEnclosingWidget(evt.target);
if(src&&src.callback){
_1d.context=src;
_1d.method=src.callback;
}
this.openExternalView(_1d,_1f);
return;
}else{
if(_1d.href){
if(_1d.hrefTarget&&_1d.hrefTarget!="_self"){
_6.global.open(_1d.href,_1d.hrefTarget);
}else{
var _20;
for(var v=_f.getEnclosingView(evt.target);v;v=_f.getParentView(v)){
_20=v;
}
if(_20){
_20.performTransition(null,_1d.transitionDir,_1d.transition,evt.target,function(){
location.href=_1d.href;
});
}
}
return;
}else{
if(_1d.scene){
_3.publish("/dojox/mobile/app/pushScene",[_1d.scene]);
return;
}
}
}
var arr=this.findTransitionViews(_1d.moveTo),_21=arr[0],_22=arr[1],_23=arr[2];
if(!location.hash&&!_1d.hashchange){
_f.initialView=_21;
}
if(_1d.moveTo&&_22){
_1d.moveTo=(_1d.moveTo.charAt(0)==="#"?"#"+_22.id:_22.id)+_23;
}
if(!_21||(_1d.moveTo&&_21===_c.byId(_1d.moveTo.replace(/^#?([^&?]+).*/,"$1")))){
return;
}
src=_c.getEnclosingWidget(evt.target);
if(src&&src.callback){
_1d.context=src;
_1d.method=src.callback;
}
_21.performTransition(_1d);
}});
_10._instance=new _10();
_10.getInstance=function(){
return _10._instance;
};
return _10;
});
