//>>built
define("dojox/io/OAuth",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/xhr","dojo/dom","dojox/encoding/digests/SHA1",],function(_1,_2,_3,_4,_5,_6){
_1.getObject("io.OAuth",true,dojox);
dojox.io.OAuth=new (function(){
var _7=this.encode=function(s){
if(!(""+s).length){
return "";
}
return encodeURIComponent(s).replace(/\!/g,"%21").replace(/\*/g,"%2A").replace(/\'/g,"%27").replace(/\(/g,"%28").replace(/\)/g,"%29");
};
var _8=this.decode=function(_9){
var a=[],_a=_9.split("&");
for(var i=0,l=_a.length;i<l;i++){
var _b=_a[i];
if(_a[i]==""){
continue;
}
if(_a[i].indexOf("=")>-1){
var _c=_a[i].split("=");
a.push([decodeURIComponent(_c[0]),decodeURIComponent(_c[1])]);
}else{
a.push([decodeURIComponent(_a[i]),null]);
}
}
return a;
};
function _d(_e){
var _f=["source","protocol","authority","userInfo","user","password","host","port","relative","path","directory","file","query","anchor"],_10=/^(?:([^:\/?#]+):)?(?:\/\/((?:(([^:@]*):?([^:@]*))?@)?([^:\/?#]*)(?::(\d*))?))?((((?:[^?#\/]*\/)*)([^?#]*))(?:\?([^#]*))?(?:#(.*))?)/,_11=_10.exec(_e),map={},i=_f.length;
while(i--){
map[_f[i]]=_11[i]||"";
}
var p=map.protocol.toLowerCase(),a=map.authority.toLowerCase(),b=(p=="http"&&map.port==80)||(p=="https"&&map.port==443);
if(b){
if(a.lastIndexOf(":")>-1){
a=a.substring(0,a.lastIndexOf(":"));
}
}
var _12=map.path||"/";
map.url=p+"://"+a+_12;
return map;
};
var tab="0123456789ABCDEFGHIJKLMNOPQRSTUVWXTZabcdefghiklmnopqrstuvwxyz";
function _13(_14){
var s="",tl=tab.length;
for(var i=0;i<_14;i++){
s+=tab.charAt(Math.floor(Math.random()*tl));
}
return s;
};
function _15(){
return Math.floor(new Date().valueOf()/1000)-2;
};
function _16(_17,key,_18){
if(_18&&_18!="PLAINTEXT"&&_18!="HMAC-SHA1"){
throw new Error("dojox.io.OAuth: the only supported signature encodings are PLAINTEXT and HMAC-SHA1.");
}
if(_18=="PLAINTEXT"){
return key;
}else{
return _6._hmac(_17,key);
}
};
function key(_19){
return _7(_19.consumer.secret)+"&"+(_19.token&&_19.token.secret?_7(_19.token.secret):"");
};
function _1a(_1b,oaa){
var o={oauth_consumer_key:oaa.consumer.key,oauth_nonce:_13(16),oauth_signature_method:oaa.sig_method||"HMAC-SHA1",oauth_timestamp:_15(),oauth_version:"1.0"};
if(oaa.token){
o.oauth_token=oaa.token.key;
}
_1b.content=_1.mixin(_1b.content||{},o);
};
function _1c(_1d){
var _1e=[{}],_1f;
if(_1d.form){
if(!_1d.content){
_1d.content={};
}
var _20=_1.byId(_1d.form);
var _21=_20.getAttributeNode("action");
_1d.url=_1d.url||(_21?_21.value:null);
_1f=_1.formToObject(_20);
delete _1d.form;
}
if(_1f){
_1e.push(_1f);
}
if(_1d.content){
_1e.push(_1d.content);
}
var map=_d(_1d.url);
if(map.query){
var tmp=_1.queryToObject(map.query);
for(var p in tmp){
tmp[p]=encodeURIComponent(tmp[p]);
}
_1e.push(tmp);
}
_1d._url=map.url;
var a=[];
for(var i=0,l=_1e.length;i<l;i++){
var _22=_1e[i];
for(var p in _22){
if(_1.isArray(_22[p])){
for(var j=0,jl=_22.length;j<jl;j++){
a.push([p,_22[j]]);
}
}else{
a.push([p,_22[p]]);
}
}
}
_1d._parameters=a;
return _1d;
};
function _23(_24,_25,oaa){
_1a(_25,oaa);
_1c(_25);
var a=_25._parameters;
a.sort(function(a,b){
if(a[0]>b[0]){
return 1;
}
if(a[0]<b[0]){
return -1;
}
if(a[1]>b[1]){
return 1;
}
if(a[1]<b[1]){
return -1;
}
return 0;
});
var s=_1.map(a,function(_26){
return _7(_26[0])+"="+_7((""+_26[1]).length?_26[1]:"");
}).join("&");
var _27=_24.toUpperCase()+"&"+_7(_25._url)+"&"+_7(s);
return _27;
};
function _28(_29,_2a,oaa){
var k=key(oaa),_2b=_23(_29,_2a,oaa),s=_16(_2b,k,oaa.sig_method||"HMAC-SHA1");
_2a.content["oauth_signature"]=s;
return _2a;
};
this.sign=function(_2c,_2d,oaa){
return _28(_2c,_2d,oaa);
};
this.xhr=function(_2e,_2f,oaa,_30){
_28(_2e,_2f,oaa);
return _4(_2e,_2f,_30);
};
this.xhrGet=function(_31,oaa){
return this.xhr("GET",_31,oaa);
};
this.xhrPost=this.xhrRawPost=function(_32,oaa){
return this.xhr("POST",_32,oaa,true);
};
this.xhrPut=this.xhrRawPut=function(_33,oaa){
return this.xhr("PUT",_33,oaa,true);
};
this.xhrDelete=function(_34,oaa){
return this.xhr("DELETE",_34,oaa);
};
})();
return dojox.io.OAuth;
});
