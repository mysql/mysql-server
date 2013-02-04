//>>built
define("dojox/io/windowName",["dojo/_base/kernel","dojo/_base/window","dojo/_base/xhr","dojo/_base/sniff","dojo/_base/url","dojo/domReady!"],function(_1){
_1.getObject("io.windowName",true,dojox);
dojox.io.windowName={send:function(_2,_3){
_3.url+=(_3.url.match(/\?/)?"&":"?")+"windowname="+(_3.authElement?"auth":true);
var _4=_3.authElement;
var _5=function(_6){
try{
var _7=_8.ioArgs.frame.contentWindow.document;
_7.write(" ");
_7.close();
}
catch(e){
}
(_4||_1.body()).removeChild(_8.ioArgs.outerFrame);
return _6;
};
var _8=_1._ioSetArgs(_3,_5,_5,_5);
if(_3.timeout){
setTimeout(function(){
if(_8.fired==-1){
_8.callback(new Error("Timeout"));
}
},_3.timeout);
}
dojox.io.windowName._send(_8,_2,_4,_3.onAuthLoad);
return _8;
},_send:function(_9,_a,_b,_c){
var _d=_9.ioArgs;
var _e=dojox.io.windowName._frameNum++;
var _f=(_1.config.dojoBlankHtmlUrl||_1.config.dojoCallbackUrl||_1.moduleUrl("dojo","resources/blank.html"))+"#"+_e;
var _10=new _1._Url(window.location,_f);
var doc=_1.doc;
var _11=_b||_1.body();
function _12(_13){
_13.style.width="100%";
_13.style.height="100%";
_13.style.border="0px";
};
if(_1.isMoz&&![].reduce){
var _14=doc.createElement("iframe");
_12(_14);
if(!_b){
_14.style.display="none";
}
_11.appendChild(_14);
var _15=_14.contentWindow;
doc=_15.document;
doc.write("<html><body margin='0px'><iframe style='width:100%;height:100%;border:0px' name='protectedFrame'></iframe></body></html>");
doc.close();
var _16=_15[0];
_15.__defineGetter__(0,function(){
});
_15.__defineGetter__("protectedFrame",function(){
});
doc=_16.document;
doc.write("<html><body margin='0px'></body></html>");
doc.close();
_11=doc.body;
}
var _17;
if(_1.isIE){
var div=doc.createElement("div");
div.innerHTML="<iframe name=\""+_10+"\" onload=\"dojox.io.windowName["+_e+"]()\">";
_17=div.firstChild;
}else{
_17=doc.createElement("iframe");
}
_d.frame=_17;
_12(_17);
_d.outerFrame=_14=_14||_17;
if(!_b){
_14.style.display="none";
}
var _18=0;
function _19(){
var _1a=_17.contentWindow.name;
if(typeof _1a=="string"){
if(_1a!=_10){
_18=2;
_9.ioArgs.hash=_17.contentWindow.location.hash;
_9.callback(_1a);
}
}
};
dojox.io.windowName[_e]=_17.onload=function(){
try{
if(!_1.isMoz&&_17.contentWindow.location=="about:blank"){
return;
}
}
catch(e){
}
if(!_18){
_18=1;
if(_b){
if(_c){
_c();
}
}else{
_17.contentWindow.location=_f;
}
}
try{
if(_18<2){
_19();
}
}
catch(e){
}
};
_17.name=_10;
if(_a.match(/GET/i)){
_1._ioAddQueryToUrl(_d);
_17.src=_d.url;
_11.appendChild(_17);
if(_17.contentWindow){
_17.contentWindow.location.replace(_d.url);
}
}else{
if(_a.match(/POST/i)){
_11.appendChild(_17);
var _1b=_1.doc.createElement("form");
_1.body().appendChild(_1b);
var _1c=_1.queryToObject(_d.query);
for(var i in _1c){
var _1d=_1c[i];
_1d=_1d instanceof Array?_1d:[_1d];
for(var j=0;j<_1d.length;j++){
var _1e=doc.createElement("input");
_1e.type="hidden";
_1e.name=i;
_1e.value=_1d[j];
_1b.appendChild(_1e);
}
}
_1b.method="POST";
_1b.action=_d.url;
_1b.target=_10;
_1b.submit();
_1b.parentNode.removeChild(_1b);
}else{
throw new Error("Method "+_a+" not supported with the windowName transport");
}
}
if(_17.contentWindow){
_17.contentWindow.name=_10;
}
},_frameNum:0};
return dojox.io.windowName;
});
