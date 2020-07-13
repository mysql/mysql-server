/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/request/xhr",["../errors/RequestError","./watch","./handlers","./util","../has"],function(_1,_2,_3,_4,_5){
_5.add("native-xhr",function(){
return typeof XMLHttpRequest!=="undefined";
});
_5.add("dojo-force-activex-xhr",function(){
return _5("activex")&&window.location.protocol==="file:";
});
_5.add("native-xhr2",function(){
if(!_5("native-xhr")||_5("dojo-force-activex-xhr")){
return;
}
var x=new XMLHttpRequest();
return typeof x["addEventListener"]!=="undefined"&&(typeof opera==="undefined"||typeof x["upload"]!=="undefined");
});
_5.add("native-formdata",function(){
return typeof FormData!=="undefined";
});
_5.add("native-blob",function(){
return typeof Blob!=="undefined";
});
_5.add("native-arraybuffer",function(){
return typeof ArrayBuffer!=="undefined";
});
_5.add("native-response-type",function(){
return _5("native-xhr")&&typeof new XMLHttpRequest().responseType!=="undefined";
});
_5.add("native-xhr2-blob",function(){
if(!_5("native-response-type")){
return;
}
var x=new XMLHttpRequest();
x.open("GET","https://dojotoolkit.org/",true);
x.responseType="blob";
var _6=x.responseType;
x.abort();
return _6==="blob";
});
var _7={"blob":_5("native-xhr2-blob")?"blob":"arraybuffer","document":"document","arraybuffer":"arraybuffer"};
function _8(_9,_a){
var _b=_9.xhr;
_9.status=_9.xhr.status;
try{
_9.text=_b.responseText;
}
catch(e){
}
if(_9.options.handleAs==="xml"){
_9.data=_b.responseXML;
}
var _c;
if(_a){
this.reject(_a);
}else{
try{
_3(_9);
}
catch(e){
_c=e;
}
if(_4.checkStatus(_b.status)){
if(!_c){
this.resolve(_9);
}else{
this.reject(_c);
}
}else{
if(!_c){
_a=new _1("Unable to load "+_9.url+" status: "+_b.status,_9);
this.reject(_a);
}else{
_a=new _1("Unable to load "+_9.url+" status: "+_b.status+" and an error in handleAs: transformation of response",_9);
this.reject(_a);
}
}
}
};
var _d,_e,_f,_10;
if(_5("native-xhr2")){
_d=function(_11){
return !this.isFulfilled();
};
_10=function(dfd,_12){
_12.xhr.abort();
};
_f=function(_13,dfd,_14,_15){
function _16(evt){
dfd.handleResponse(_14);
};
function _17(evt){
var _18=evt.target;
var _19=new _1("Unable to load "+_14.url+" status: "+_18.status,_14);
dfd.handleResponse(_14,_19);
};
function _1a(_1b,evt){
_14.transferType=_1b;
if(evt.lengthComputable){
_14.loaded=evt.loaded;
_14.total=evt.total;
dfd.progress(_14);
}else{
if(_14.xhr.readyState===3){
_14.loaded=("loaded" in evt)?evt.loaded:evt.position;
dfd.progress(_14);
}
}
};
function _1c(evt){
return _1a("download",evt);
};
function _1d(evt){
return _1a("upload",evt);
};
_13.addEventListener("load",_16,false);
_13.addEventListener("error",_17,false);
_13.addEventListener("progress",_1c,false);
if(_15&&_13.upload){
_13.upload.addEventListener("progress",_1d,false);
}
return function(){
_13.removeEventListener("load",_16,false);
_13.removeEventListener("error",_17,false);
_13.removeEventListener("progress",_1c,false);
_13.upload.removeEventListener("progress",_1d,false);
_13=null;
};
};
}else{
_d=function(_1e){
return _1e.xhr.readyState;
};
_e=function(_1f){
return 4===_1f.xhr.readyState;
};
_10=function(dfd,_20){
var xhr=_20.xhr;
var _21=typeof xhr.abort;
if(_21==="function"||_21==="object"||_21==="unknown"){
xhr.abort();
}
};
}
function _22(_23){
return this.xhr.getResponseHeader(_23);
};
var _24,_25={data:null,query:null,sync:false,method:"GET"};
function xhr(url,_26,_27){
var _28=_5("native-formdata")&&_26&&_26.data&&_26.data instanceof FormData;
var _29=_4.parseArgs(url,_4.deepCreate(_25,_26),_28);
url=_29.url;
_26=_29.options;
var _2a=!_26.data&&_26.method!=="POST"&&_26.method!=="PUT";
if(_5("ie")<=10){
url=url.split("#")[0];
}
var _2b,_2c=function(){
_2b&&_2b();
};
var dfd=_4.deferred(_29,_10,_d,_e,_8,_2c);
var _2d=_29.xhr=xhr._create();
if(!_2d){
dfd.cancel(new _1("XHR was not created"));
return _27?dfd:dfd.promise;
}
_29.getHeader=_22;
if(_f){
_2b=_f(_2d,dfd,_29,_26.uploadProgress);
}
var _2e=typeof (_26.data)==="undefined"?null:_26.data,_2f=!_26.sync,_30=_26.method;
try{
_2d.open(_30,url,_2f,_26.user||_24,_26.password||_24);
if(_26.withCredentials){
_2d.withCredentials=_26.withCredentials;
}
if(_5("native-response-type")&&_26.handleAs in _7){
_2d.responseType=_7[_26.handleAs];
}
var _31=_26.headers,_32=(_28||_2a)?false:"application/x-www-form-urlencoded";
if(_31){
for(var hdr in _31){
if(hdr.toLowerCase()==="content-type"){
_32=_31[hdr];
}else{
if(_31[hdr]){
_2d.setRequestHeader(hdr,_31[hdr]);
}
}
}
}
if(_32&&_32!==false){
_2d.setRequestHeader("Content-Type",_32);
}
if(!_31||!("X-Requested-With" in _31)){
_2d.setRequestHeader("X-Requested-With","XMLHttpRequest");
}
if(_4.notify){
_4.notify.emit("send",_29,dfd.promise.cancel);
}
_2d.send(_2e);
}
catch(e){
dfd.reject(e);
}
_2(dfd);
_2d=null;
return _27?dfd:dfd.promise;
};
xhr._create=function(){
throw new Error("XMLHTTP not available");
};
if(_5("native-xhr")&&!_5("dojo-force-activex-xhr")){
xhr._create=function(){
return new XMLHttpRequest();
};
}else{
if(_5("activex")){
try{
new ActiveXObject("Msxml2.XMLHTTP");
xhr._create=function(){
return new ActiveXObject("Msxml2.XMLHTTP");
};
}
catch(e){
try{
new ActiveXObject("Microsoft.XMLHTTP");
xhr._create=function(){
return new ActiveXObject("Microsoft.XMLHTTP");
};
}
catch(e){
}
}
}
}
_4.addCommonMethods(xhr);
return xhr;
});
