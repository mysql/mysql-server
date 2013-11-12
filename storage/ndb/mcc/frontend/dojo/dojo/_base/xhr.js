/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/xhr",["./kernel","./sniff","require","../io-query","../dom","../dom-form","./Deferred","./json","./lang","./array","../on"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,on){
_2.add("native-xhr",function(){
return typeof XMLHttpRequest!=="undefined";
});
if(1){
_1._xhrObj=_3.getXhr;
}else{
if(_2("native-xhr")){
_1._xhrObj=function(){
try{
return new XMLHttpRequest();
}
catch(e){
throw new Error("XMLHTTP not available: "+e);
}
};
}else{
for(var _b=["Msxml2.XMLHTTP","Microsoft.XMLHTTP","Msxml2.XMLHTTP.4.0"],_c,i=0;i<3;){
try{
_c=_b[i++];
if(new ActiveXObject(_c)){
break;
}
}
catch(e){
}
}
_1._xhrObj=function(){
return new ActiveXObject(_c);
};
}
}
var _d=_1.config;
_1.objectToQuery=_4.objectToQuery;
_1.queryToObject=_4.queryToObject;
_1.fieldToObject=_6.fieldToObject;
_1.formToObject=_6.toObject;
_1.formToQuery=_6.toQuery;
_1.formToJson=_6.toJson;
_1._blockAsync=false;
var _e=_1._contentHandlers=_1.contentHandlers={"text":function(_f){
return _f.responseText;
},"json":function(xhr){
return _8.fromJson(xhr.responseText||null);
},"json-comment-filtered":function(xhr){
if(!_1.config.useCommentedJson){
console.warn("Consider using the standard mimetype:application/json."+" json-commenting can introduce security issues. To"+" decrease the chances of hijacking, use the standard the 'json' handler and"+" prefix your json with: {}&&\n"+"Use djConfig.useCommentedJson=true to turn off this message.");
}
var _10=xhr.responseText;
var _11=_10.indexOf("/*");
var _12=_10.lastIndexOf("*/");
if(_11==-1||_12==-1){
throw new Error("JSON was not comment filtered");
}
return _8.fromJson(_10.substring(_11+2,_12));
},"javascript":function(xhr){
return _1.eval(xhr.responseText);
},"xml":function(xhr){
var _13=xhr.responseXML;
if(_2("ie")){
if((!_13||!_13.documentElement)){
var ms=function(n){
return "MSXML"+n+".DOMDocument";
};
var dp=["Microsoft.XMLDOM",ms(6),ms(4),ms(3),ms(2)];
_a.some(dp,function(p){
try{
var dom=new ActiveXObject(p);
dom.async=false;
dom.loadXML(xhr.responseText);
_13=dom;
}
catch(e){
return false;
}
return true;
});
}
}
return _13;
},"json-comment-optional":function(xhr){
if(xhr.responseText&&/^[^{\[]*\/\*/.test(xhr.responseText)){
return _e["json-comment-filtered"](xhr);
}else{
return _e["json"](xhr);
}
}};
_1._ioSetArgs=function(_14,_15,_16,_17){
var _18={args:_14,url:_14.url};
var _19=null;
if(_14.form){
var _1a=_5.byId(_14.form);
var _1b=_1a.getAttributeNode("action");
_18.url=_18.url||(_1b?_1b.value:null);
_19=_6.toObject(_1a);
}
var _1c=[{}];
if(_19){
_1c.push(_19);
}
if(_14.content){
_1c.push(_14.content);
}
if(_14.preventCache){
_1c.push({"dojo.preventCache":new Date().valueOf()});
}
_18.query=_4.objectToQuery(_9.mixin.apply(null,_1c));
_18.handleAs=_14.handleAs||"text";
var d=new _7(_15);
d.addCallbacks(_16,function(_1d){
return _17(_1d,d);
});
var ld=_14.load;
if(ld&&_9.isFunction(ld)){
d.addCallback(function(_1e){
return ld.call(_14,_1e,_18);
});
}
var err=_14.error;
if(err&&_9.isFunction(err)){
d.addErrback(function(_1f){
return err.call(_14,_1f,_18);
});
}
var _20=_14.handle;
if(_20&&_9.isFunction(_20)){
d.addBoth(function(_21){
return _20.call(_14,_21,_18);
});
}
if(_d.ioPublish&&_1.publish&&_18.args.ioPublish!==false){
d.addCallbacks(function(res){
_1.publish("/dojo/io/load",[d,res]);
return res;
},function(res){
_1.publish("/dojo/io/error",[d,res]);
return res;
});
d.addBoth(function(res){
_1.publish("/dojo/io/done",[d,res]);
return res;
});
}
d.ioArgs=_18;
return d;
};
var _22=function(dfd){
dfd.canceled=true;
var xhr=dfd.ioArgs.xhr;
var _23=typeof xhr.abort;
if(_23=="function"||_23=="object"||_23=="unknown"){
xhr.abort();
}
var err=dfd.ioArgs.error;
if(!err){
err=new Error("xhr cancelled");
err.dojoType="cancel";
}
return err;
};
var _24=function(dfd){
var ret=_e[dfd.ioArgs.handleAs](dfd.ioArgs.xhr);
return ret===undefined?null:ret;
};
var _25=function(_26,dfd){
if(!dfd.ioArgs.args.failOk){
console.error(_26);
}
return _26;
};
var _27=null;
var _28=[];
var _29=0;
var _2a=function(dfd){
if(_29<=0){
_29=0;
if(_d.ioPublish&&_1.publish&&(!dfd||dfd&&dfd.ioArgs.args.ioPublish!==false)){
_1.publish("/dojo/io/stop");
}
}
};
var _2b=function(){
var now=(new Date()).getTime();
if(!_1._blockAsync){
for(var i=0,tif;i<_28.length&&(tif=_28[i]);i++){
var dfd=tif.dfd;
var _2c=function(){
if(!dfd||dfd.canceled||!tif.validCheck(dfd)){
_28.splice(i--,1);
_29-=1;
}else{
if(tif.ioCheck(dfd)){
_28.splice(i--,1);
tif.resHandle(dfd);
_29-=1;
}else{
if(dfd.startTime){
if(dfd.startTime+(dfd.ioArgs.args.timeout||0)<now){
_28.splice(i--,1);
var err=new Error("timeout exceeded");
err.dojoType="timeout";
dfd.errback(err);
dfd.cancel();
_29-=1;
}
}
}
}
};
if(_1.config.debugAtAllCosts){
_2c.call(this);
}else{
_2c.call(this);
}
}
}
_2a(dfd);
if(!_28.length){
clearInterval(_27);
_27=null;
}
};
_1._ioCancelAll=function(){
try{
_a.forEach(_28,function(i){
try{
i.dfd.cancel();
}
catch(e){
}
});
}
catch(e){
}
};
if(_2("ie")){
on(window,"unload",_1._ioCancelAll);
}
_1._ioNotifyStart=function(dfd){
if(_d.ioPublish&&_1.publish&&dfd.ioArgs.args.ioPublish!==false){
if(!_29){
_1.publish("/dojo/io/start");
}
_29+=1;
_1.publish("/dojo/io/send",[dfd]);
}
};
_1._ioWatch=function(dfd,_2d,_2e,_2f){
var _30=dfd.ioArgs.args;
if(_30.timeout){
dfd.startTime=(new Date()).getTime();
}
_28.push({dfd:dfd,validCheck:_2d,ioCheck:_2e,resHandle:_2f});
if(!_27){
_27=setInterval(_2b,50);
}
if(_30.sync){
_2b();
}
};
var _31="application/x-www-form-urlencoded";
var _32=function(dfd){
return dfd.ioArgs.xhr.readyState;
};
var _33=function(dfd){
return 4==dfd.ioArgs.xhr.readyState;
};
var _34=function(dfd){
var xhr=dfd.ioArgs.xhr;
if(_1._isDocumentOk(xhr)){
dfd.callback(dfd);
}else{
var err=new Error("Unable to load "+dfd.ioArgs.url+" status:"+xhr.status);
err.status=xhr.status;
err.responseText=xhr.responseText;
err.xhr=xhr;
dfd.errback(err);
}
};
_1._ioAddQueryToUrl=function(_35){
if(_35.query.length){
_35.url+=(_35.url.indexOf("?")==-1?"?":"&")+_35.query;
_35.query=null;
}
};
_1.xhr=function(_36,_37,_38){
var dfd=_1._ioSetArgs(_37,_22,_24,_25);
var _39=dfd.ioArgs;
var xhr=_39.xhr=_1._xhrObj(_39.args);
if(!xhr){
dfd.cancel();
return dfd;
}
if("postData" in _37){
_39.query=_37.postData;
}else{
if("putData" in _37){
_39.query=_37.putData;
}else{
if("rawBody" in _37){
_39.query=_37.rawBody;
}else{
if((arguments.length>2&&!_38)||"POST|PUT".indexOf(_36.toUpperCase())==-1){
_1._ioAddQueryToUrl(_39);
}
}
}
}
xhr.open(_36,_39.url,_37.sync!==true,_37.user||undefined,_37.password||undefined);
if(_37.headers){
for(var hdr in _37.headers){
if(hdr.toLowerCase()==="content-type"&&!_37.contentType){
_37.contentType=_37.headers[hdr];
}else{
if(_37.headers[hdr]){
xhr.setRequestHeader(hdr,_37.headers[hdr]);
}
}
}
}
if(_37.contentType!==false){
xhr.setRequestHeader("Content-Type",_37.contentType||_31);
}
if(!_37.headers||!("X-Requested-With" in _37.headers)){
xhr.setRequestHeader("X-Requested-With","XMLHttpRequest");
}
_1._ioNotifyStart(dfd);
if(_1.config.debugAtAllCosts){
xhr.send(_39.query);
}else{
try{
xhr.send(_39.query);
}
catch(e){
_39.error=e;
dfd.cancel();
}
}
_1._ioWatch(dfd,_32,_33,_34);
xhr=null;
return dfd;
};
_1.xhrGet=function(_3a){
return _1.xhr("GET",_3a);
};
_1.rawXhrPost=_1.xhrPost=function(_3b){
return _1.xhr("POST",_3b,true);
};
_1.rawXhrPut=_1.xhrPut=function(_3c){
return _1.xhr("PUT",_3c,true);
};
_1.xhrDelete=function(_3d){
return _1.xhr("DELETE",_3d);
};
_1._isDocumentOk=function(_3e){
var _3f=_3e.status||0;
_3f=(_3f>=200&&_3f<300)||_3f==304||_3f==1223||!_3f;
return _3f;
};
_1._getText=function(url){
var _40;
_1.xhrGet({url:url,sync:true,load:function(_41){
_40=_41;
}});
return _40;
};
_9.mixin(_1.xhr,{_xhrObj:_1._xhrObj,fieldToObject:_6.fieldToObject,formToObject:_6.toObject,objectToQuery:_4.objectToQuery,formToQuery:_6.toQuery,formToJson:_6.toJson,queryToObject:_4.queryToObject,contentHandlers:_e,_ioSetArgs:_1._ioSetArgs,_ioCancelAll:_1._ioCancelAll,_ioNotifyStart:_1._ioNotifyStart,_ioWatch:_1._ioWatch,_ioAddQueryToUrl:_1._ioAddQueryToUrl,_isDocumentOk:_1._isDocumentOk,_getText:_1._getText,get:_1.xhrGet,post:_1.xhrPost,put:_1.xhrPut,del:_1.xhrDelete});
return _1.xhr;
});
