/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/hash",["./_base/kernel","require","./_base/connect","./_base/lang","./ready","./_base/sniff"],function(_1,_2,_3,_4,_5,_6){
_1.hash=function(_7,_8){
if(!arguments.length){
return _9();
}
if(_7.charAt(0)=="#"){
_7=_7.substring(1);
}
if(_8){
_a(_7);
}else{
location.href="#"+_7;
}
return _7;
};
var _b,_c,_d,_e=_1.config.hashPollFrequency||100;
function _f(str,_10){
var i=str.indexOf(_10);
return (i>=0)?str.substring(i+1):"";
};
function _9(){
return _f(location.href,"#");
};
function _11(){
_3.publish("/dojo/hashchange",[_9()]);
};
function _12(){
if(_9()===_b){
return;
}
_b=_9();
_11();
};
function _a(_13){
if(_c){
if(_c.isTransitioning()){
setTimeout(_4.hitch(null,_a,_13),_e);
return;
}
var _14=_c.iframe.location.href;
var _15=_14.indexOf("?");
_c.iframe.location.replace(_14.substring(0,_15)+"?"+_13);
return;
}
location.replace("#"+_13);
!_d&&_12();
};
function _16(){
var ifr=document.createElement("iframe"),_17="dojo-hash-iframe",_18=_1.config.dojoBlankHtmlUrl||_2.toUrl("./resources/blank.html");
if(_1.config.useXDomain&&!_1.config.dojoBlankHtmlUrl){
console.warn("dojo.hash: When using cross-domain Dojo builds,"+" please save dojo/resources/blank.html to your domain and set djConfig.dojoBlankHtmlUrl"+" to the path on your domain to blank.html");
}
ifr.id=_17;
ifr.src=_18+"?"+_9();
ifr.style.display="none";
document.body.appendChild(ifr);
this.iframe=_1.global[_17];
var _19,_1a,_1b,_1c,_1d,_1e=this.iframe.location;
function _1f(){
_b=_9();
_19=_1d?_b:_f(_1e.href,"?");
_1a=false;
_1b=null;
};
this.isTransitioning=function(){
return _1a;
};
this.pollLocation=function(){
if(!_1d){
try{
var _20=_f(_1e.href,"?");
if(document.title!=_1c){
_1c=this.iframe.document.title=document.title;
}
}
catch(e){
_1d=true;
console.error("dojo.hash: Error adding history entry. Server unreachable.");
}
}
var _21=_9();
if(_1a&&_b===_21){
if(_1d||_20===_1b){
_1f();
_11();
}else{
setTimeout(_4.hitch(this,this.pollLocation),0);
return;
}
}else{
if(_b===_21&&(_1d||_19===_20)){
}else{
if(_b!==_21){
_b=_21;
_1a=true;
_1b=_21;
ifr.src=_18+"?"+_1b;
_1d=false;
setTimeout(_4.hitch(this,this.pollLocation),0);
return;
}else{
if(!_1d){
location.href="#"+_1e.search.substring(1);
_1f();
_11();
}
}
}
}
setTimeout(_4.hitch(this,this.pollLocation),_e);
};
_1f();
setTimeout(_4.hitch(this,this.pollLocation),_e);
};
_5(function(){
if("onhashchange" in _1.global&&(!_6("ie")||(_6("ie")>=8&&document.compatMode!="BackCompat"))){
_d=_3.connect(_1.global,"onhashchange",_11);
}else{
if(document.addEventListener){
_b=_9();
setInterval(_12,_e);
}else{
if(document.attachEvent){
_c=new _16();
}
}
}
});
return _1.hash;
});
