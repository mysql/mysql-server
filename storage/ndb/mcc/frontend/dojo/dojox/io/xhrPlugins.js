//>>built
define("dojox/io/xhrPlugins",["dojo/_base/kernel","dojo/_base/xhr","dojo/AdapterRegistry"],function(_1,_2,_3){
_1.getObject("io.xhrPlugins",true,dojox);
var _4;
var _5;
function _6(){
return _5=dojox.io.xhrPlugins.plainXhr=_5||_1._defaultXhr||_2;
};
dojox.io.xhrPlugins.register=function(){
var _7=_6();
if(!_4){
_4=new _3();
_1[_1._defaultXhr?"_defaultXhr":"xhr"]=function(_8,_9,_a){
return _4.match.apply(_4,arguments);
};
_4.register("xhr",function(_b,_c){
if(!_c.url.match(/^\w*:\/\//)){
return true;
}
var _d=window.location.href.match(/^.*?\/\/.*?\//)[0];
return _c.url.substring(0,_d.length)==_d;
},_7);
}
return _4.register.apply(_4,arguments);
};
dojox.io.xhrPlugins.addProxy=function(_e){
var _f=_6();
dojox.io.xhrPlugins.register("proxy",function(_10,_11){
return true;
},function(_12,_13,_14){
_13.url=_e+encodeURIComponent(_13.url);
return _f.call(_1,_12,_13,_14);
});
};
var _15;
dojox.io.xhrPlugins.addCrossSiteXhr=function(url,_16){
var _17=_6();
if(_15===undefined&&window.XMLHttpRequest){
try{
var xhr=new XMLHttpRequest();
xhr.open("GET","http://testing-cross-domain-capability.com",true);
_15=true;
_1.config.noRequestedWithHeaders=true;
}
catch(e){
_15=false;
}
}
dojox.io.xhrPlugins.register("cs-xhr",function(_18,_19){
return (_15||(window.XDomainRequest&&_19.sync!==true&&(_18=="GET"||_18=="POST"||_16)))&&(_19.url.substring(0,url.length)==url);
},_15?_17:function(){
var _1a=_1._xhrObj;
_1._xhrObj=function(){
var xdr=new XDomainRequest();
xdr.readyState=1;
xdr.setRequestHeader=function(){
};
xdr.getResponseHeader=function(_1b){
return _1b=="Content-Type"?xdr.contentType:null;
};
function _1c(_1d,_1e){
return function(){
xdr.readyState=_1e;
xdr.status=_1d;
};
};
xdr.onload=_1c(200,4);
xdr.onprogress=_1c(200,3);
xdr.onerror=_1c(404,4);
return xdr;
};
var dfd=(_16?_16(_6()):_6()).apply(_1,arguments);
_1._xhrObj=_1a;
return dfd;
});
};
dojox.io.xhrPlugins.fullHttpAdapter=function(_1f,_20){
return function(_21,_22,_23){
var _24={};
var _25={};
if(_21!="GET"){
_25["http-method"]=_21;
if(_22.putData&&_20){
_24["http-content"]=_22.putData;
delete _22.putData;
_23=false;
}
if(_22.postData&&_20){
_24["http-content"]=_22.postData;
delete _22.postData;
_23=false;
}
_21="POST";
}
for(var i in _22.headers){
var _26=i.match(/^X-/)?i.substring(2).replace(/-/g,"_").toLowerCase():("http-"+i);
_25[_26]=_22.headers[i];
}
_22.query=_1.objectToQuery(_25);
_1._ioAddQueryToUrl(_22);
_22.content=_1.mixin(_22.content||{},_24);
return _1f.call(_1,_21,_22,_23);
};
};
return dojox.io.xhrPlugins;
});
