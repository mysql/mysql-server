/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/hash",["./_base/kernel","require","./_base/config","./_base/connect","./_base/lang","./ready","./sniff"],function(_1,_2,_3,_4,_5,_6,_7){
_1.hash=function(_8,_9){
if(!arguments.length){
return _a();
}
if(_8.charAt(0)=="#"){
_8=_8.substring(1);
}
if(_9){
_b(_8);
}else{
location.href="#"+_8;
}
return _8;
};
var _c,_d,_e,_f=_3.hashPollFrequency||100;
function _10(str,_11){
var i=str.indexOf(_11);
return (i>=0)?str.substring(i+1):"";
};
function _a(){
return _10(location.href,"#");
};
function _12(){
_4.publish("/dojo/hashchange",[_a()]);
};
function _13(){
if(_a()===_c){
return;
}
_c=_a();
_12();
};
function _b(_14){
if(_d){
if(_d.isTransitioning()){
setTimeout(_5.hitch(null,_b,_14),_f);
return;
}
var _15=_d.iframe.location.href;
var _16=_15.indexOf("?");
_d.iframe.location.replace(_15.substring(0,_16)+"?"+_14);
return;
}
location.replace("#"+_14);
!_e&&_13();
};
function _17(){
var ifr=document.createElement("iframe"),_18="dojo-hash-iframe",_19=_3.dojoBlankHtmlUrl||_2.toUrl("./resources/blank.html");
if(_3.useXDomain&&!_3.dojoBlankHtmlUrl){
console.warn("dojo.hash: When using cross-domain Dojo builds,"+" please save dojo/resources/blank.html to your domain and set djConfig.dojoBlankHtmlUrl"+" to the path on your domain to blank.html");
}
ifr.id=_18;
ifr.src=_19+"?"+_a();
ifr.style.display="none";
document.body.appendChild(ifr);
this.iframe=_1.global[_18];
var _1a,_1b,_1c,_1d,_1e,_1f=this.iframe.location;
function _20(){
_c=_a();
_1a=_1e?_c:_10(_1f.href,"?");
_1b=false;
_1c=null;
};
this.isTransitioning=function(){
return _1b;
};
this.pollLocation=function(){
if(!_1e){
try{
var _21=_10(_1f.href,"?");
if(document.title!=_1d){
_1d=this.iframe.document.title=document.title;
}
}
catch(e){
_1e=true;
console.error("dojo.hash: Error adding history entry. Server unreachable.");
}
}
var _22=_a();
if(_1b&&_c===_22){
if(_1e||_21===_1c){
_20();
_12();
}else{
setTimeout(_5.hitch(this,this.pollLocation),0);
return;
}
}else{
if(_c===_22&&(_1e||_1a===_21)){
}else{
if(_c!==_22){
_c=_22;
_1b=true;
_1c=_22;
ifr.src=_19+"?"+_1c;
_1e=false;
setTimeout(_5.hitch(this,this.pollLocation),0);
return;
}else{
if(!_1e){
location.href="#"+_1f.search.substring(1);
_20();
_12();
}
}
}
}
setTimeout(_5.hitch(this,this.pollLocation),_f);
};
_20();
setTimeout(_5.hitch(this,this.pollLocation),_f);
};
_6(function(){
if("onhashchange" in _1.global&&(!_7("ie")||(_7("ie")>=8&&document.compatMode!="BackCompat"))){
_e=_4.connect(_1.global,"onhashchange",_12);
}else{
if(document.addEventListener){
_c=_a();
setInterval(_13,_f);
}else{
if(document.attachEvent){
_d=new _17();
}
}
}
});
return _1.hash;
});
