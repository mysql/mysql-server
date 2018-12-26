//>>built
define("dojox/io/xhrMultiPart",["dojo/_base/kernel","dojo/_base/array","dojo/_base/xhr","dojo/query","dojox/uuid/generateRandomUuid"],function(_1,_2,_3,_4,_5){
_1.getObject("io.xhrMultiPart",true,dojox);
function _6(_7,_8){
if(!_7["name"]&&!_7["content"]){
throw new Error("Each part of a multi-part request requires 'name' and 'content'.");
}
var _9=[];
_9.push("--"+_8,"Content-Disposition: form-data; name=\""+_7.name+"\""+(_7["filename"]?"; filename=\""+_7.filename+"\"":""));
if(_7["contentType"]){
var ct="Content-Type: "+_7.contentType;
if(_7["charset"]){
ct+="; Charset="+_7.charset;
}
_9.push(ct);
}
if(_7["contentTransferEncoding"]){
_9.push("Content-Transfer-Encoding: "+_7.contentTransferEncoding);
}
_9.push("",_7.content);
return _9;
};
function _a(_b,_c){
var o=_1.formToObject(_b),_d=[];
for(var p in o){
if(_1.isArray(o[p])){
_1.forEach(o[p],function(_e){
_d=_d.concat(_6({name:p,content:_e},_c));
});
}else{
_d=_d.concat(_6({name:p,content:o[p]},_c));
}
}
return _d;
};
dojox.io.xhrMultiPart=function(_f){
if(!_f["file"]&&!_f["content"]&&!_f["form"]){
throw new Error("content, file or form must be provided to dojox.io.xhrMultiPart's arguments");
}
var _10=_5(),tmp=[],out="";
if(_f["file"]||_f["content"]){
var v=_f["file"]||_f["content"];
_1.forEach((_1.isArray(v)?v:[v]),function(_11){
tmp=tmp.concat(_6(_11,_10));
});
}else{
if(_f["form"]){
if(_4("input[type=file]",_f["form"]).length){
throw new Error("dojox.io.xhrMultiPart cannot post files that are values of an INPUT TYPE=FILE.  Use dojo.io.iframe.send() instead.");
}
tmp=_a(_f["form"],_10);
}
}
if(tmp.length){
tmp.push("--"+_10+"--","");
out=tmp.join("\r\n");
}
return _1.rawXhrPost(_1.mixin(_f,{contentType:"multipart/form-data; boundary="+_10,postData:out}));
};
return dojox.io.xhrMultiPart;
});
