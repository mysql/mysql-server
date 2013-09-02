/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/io/iframe",["../main","require"],function(_1,_2){
_1.getObject("io",true,_1);
_1.io.iframe={create:function(_3,_4,_5){
if(window[_3]){
return window[_3];
}
if(window.frames[_3]){
return window.frames[_3];
}
var _6=_5;
if(!_6){
if(_1.config["useXDomain"]&&!_1.config["dojoBlankHtmlUrl"]){
console.warn("dojo.io.iframe.create: When using cross-domain Dojo builds,"+" please save dojo/resources/blank.html to your domain and set djConfig.dojoBlankHtmlUrl"+" to the path on your domain to blank.html");
}
_6=(_1.config["dojoBlankHtmlUrl"]||_2.toUrl("../resources/blank.html"));
}
var _7=_1.place("<iframe id=\""+_3+"\" name=\""+_3+"\" src=\""+_6+"\" onload=\""+_4+"\" style=\"position: absolute; left: 1px; top: 1px; height: 1px; width: 1px; visibility: hidden\">",_1.body());
window[_3]=_7;
return _7;
},setSrc:function(_8,_9,_a){
try{
if(!_a){
if(_1.isWebKit){
_8.location=_9;
}else{
frames[_8.name].location=_9;
}
}else{
var _b;
if(_1.isIE||_1.isWebKit){
_b=_8.contentWindow.document;
}else{
_b=_8.contentWindow;
}
if(!_b){
_8.location=_9;
}else{
_b.location.replace(_9);
}
}
}
catch(e){
}
},doc:function(_c){
return _c.contentDocument||(((_c.name)&&(_c.document)&&(_1.doc.getElementsByTagName("iframe")[_c.name].contentWindow)&&(_1.doc.getElementsByTagName("iframe")[_c.name].contentWindow.document)))||((_c.name)&&(_1.doc.frames[_c.name])&&(_1.doc.frames[_c.name].document))||null;
},send:function(_d){
if(!this["_frame"]){
this._frame=this.create(this._iframeName,_1._scopeName+".io.iframe._iframeOnload();");
}
var _e=_1._ioSetArgs(_d,function(_f){
_f.canceled=true;
_f.ioArgs._callNext();
},function(dfd){
var _10=null;
try{
var _11=dfd.ioArgs;
var dii=_1.io.iframe;
var ifd=dii.doc(dii._frame);
var _12=_11.handleAs;
_10=ifd;
if(_12!="html"){
if(_12=="xml"){
if(_1.isIE<9||(_1.isIE&&_1.isQuirks)){
_1.query("a",dii._frame.contentWindow.document.documentElement).orphan();
var _13=(dii._frame.contentWindow.document).documentElement.innerText;
_13=_13.replace(/>\s+</g,"><");
_13=_1.trim(_13);
var _14={responseText:_13};
_10=_1._contentHandlers["xml"](_14);
}
}else{
_10=ifd.getElementsByTagName("textarea")[0].value;
if(_12=="json"){
_10=_1.fromJson(_10);
}else{
if(_12=="javascript"){
_10=_1.eval(_10);
}
}
}
}
}
catch(e){
_10=e;
}
finally{
_11._callNext();
}
return _10;
},function(_15,dfd){
dfd.ioArgs._hasError=true;
dfd.ioArgs._callNext();
return _15;
});
_e.ioArgs._callNext=function(){
if(!this["_calledNext"]){
this._calledNext=true;
_1.io.iframe._currentDfd=null;
_1.io.iframe._fireNextRequest();
}
};
this._dfdQueue.push(_e);
this._fireNextRequest();
_1._ioWatch(_e,function(dfd){
return !dfd.ioArgs["_hasError"];
},function(dfd){
return (!!dfd.ioArgs["_finished"]);
},function(dfd){
if(dfd.ioArgs._finished){
dfd.callback(dfd);
}else{
dfd.errback(new Error("Invalid dojo.io.iframe request state"));
}
});
return _e;
},_currentDfd:null,_dfdQueue:[],_iframeName:_1._scopeName+"IoIframe",_fireNextRequest:function(){
try{
if((this._currentDfd)||(this._dfdQueue.length==0)){
return;
}
do{
var dfd=this._currentDfd=this._dfdQueue.shift();
}while(dfd&&dfd.canceled&&this._dfdQueue.length);
if(!dfd||dfd.canceled){
this._currentDfd=null;
return;
}
var _16=dfd.ioArgs;
var _17=_16.args;
_16._contentToClean=[];
var fn=_1.byId(_17["form"]);
var _18=_17["content"]||{};
if(fn){
if(_18){
var _19=function(_1a,_1b){
_1.create("input",{type:"hidden",name:_1a,value:_1b},fn);
_16._contentToClean.push(_1a);
};
for(var x in _18){
var val=_18[x];
if(_1.isArray(val)&&val.length>1){
var i;
for(i=0;i<val.length;i++){
_19(x,val[i]);
}
}else{
if(!fn[x]){
_19(x,val);
}else{
fn[x].value=val;
}
}
}
}
var _1c=fn.getAttributeNode("action");
var _1d=fn.getAttributeNode("method");
var _1e=fn.getAttributeNode("target");
if(_17["url"]){
_16._originalAction=_1c?_1c.value:null;
if(_1c){
_1c.value=_17.url;
}else{
fn.setAttribute("action",_17.url);
}
}
if(!_1d||!_1d.value){
if(_1d){
_1d.value=(_17["method"])?_17["method"]:"post";
}else{
fn.setAttribute("method",(_17["method"])?_17["method"]:"post");
}
}
_16._originalTarget=_1e?_1e.value:null;
if(_1e){
_1e.value=this._iframeName;
}else{
fn.setAttribute("target",this._iframeName);
}
fn.target=this._iframeName;
_1._ioNotifyStart(dfd);
fn.submit();
}else{
var _1f=_17.url+(_17.url.indexOf("?")>-1?"&":"?")+_16.query;
_1._ioNotifyStart(dfd);
this.setSrc(this._frame,_1f,true);
}
}
catch(e){
dfd.errback(e);
}
},_iframeOnload:function(){
var dfd=this._currentDfd;
if(!dfd){
this._fireNextRequest();
return;
}
var _20=dfd.ioArgs;
var _21=_20.args;
var _22=_1.byId(_21.form);
if(_22){
var _23=_20._contentToClean;
for(var i=0;i<_23.length;i++){
var key=_23[i];
for(var j=0;j<_22.childNodes.length;j++){
var _24=_22.childNodes[j];
if(_24.name==key){
_1.destroy(_24);
break;
}
}
}
if(_20["_originalAction"]){
_22.setAttribute("action",_20._originalAction);
}
if(_20["_originalTarget"]){
_22.setAttribute("target",_20._originalTarget);
_22.target=_20._originalTarget;
}
}
_20._finished=true;
}};
return _1.io.iframe;
});
