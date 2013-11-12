//>>built
define("dojox/form/uploader/plugins/HTML5",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo"],function(_1,_2,_3,_4){
var _5=_1("dojox.form.uploader.plugins.HTML5",[],{errMsg:"Error uploading files. Try checking permissions",uploadType:"html5",postCreate:function(){
this.connectForm();
this.inherited(arguments);
if(this.uploadOnSelect){
this.connect(this,"onChange",function(_6){
this.upload(_6[0]);
});
}
},_drop:function(e){
_4.stopEvent(e);
var dt=e.dataTransfer;
this._files=dt.files;
this.onChange(this.getFileList());
},upload:function(_7){
this.onBegin(this.getFileList());
if(this.supports("FormData")){
this.uploadWithFormData(_7);
}else{
if(this.supports("sendAsBinary")){
this.sendAsBinary(_7);
}
}
},addDropTarget:function(_8,_9){
if(!_9){
this.connect(_8,"dragenter",_4.stopEvent);
this.connect(_8,"dragover",_4.stopEvent);
this.connect(_8,"dragleave",_4.stopEvent);
}
this.connect(_8,"drop","_drop");
},sendAsBinary:function(_a){
if(!this.getUrl()){
console.error("No upload url found.",this);
return;
}
var _b="---------------------------"+(new Date).getTime();
var _c=this.createXhr();
_c.setRequestHeader("Content-Type","multipart/form-data; boundary="+_b);
var _d=this._buildRequestBody(_a,_b);
if(!_d){
this.onError(this.errMsg);
}else{
_c.sendAsBinary(_d);
}
},uploadWithFormData:function(_e){
if(!this.getUrl()){
console.error("No upload url found.",this);
return;
}
var fd=new FormData();
_3.forEach(this._files,function(f,i){
fd.append(this.name+"s[]",f);
},this);
if(_e){
for(var nm in _e){
fd.append(nm,_e[nm]);
}
}
var _f=this.createXhr();
_f.send(fd);
},_xhrProgress:function(evt){
if(evt.lengthComputable){
var o={bytesLoaded:evt.loaded,bytesTotal:evt.total,type:evt.type,timeStamp:evt.timeStamp};
if(evt.type=="load"){
o.percent="100%",o.decimal=1;
}else{
o.decimal=evt.loaded/evt.total;
o.percent=Math.ceil((evt.loaded/evt.total)*100)+"%";
}
this.onProgress(o);
}
},createXhr:function(){
var xhr=new XMLHttpRequest();
var _10;
xhr.upload.addEventListener("progress",_2.hitch(this,"_xhrProgress"),false);
xhr.addEventListener("load",_2.hitch(this,"_xhrProgress"),false);
xhr.addEventListener("error",_2.hitch(this,function(evt){
this.onError(evt);
clearInterval(_10);
}),false);
xhr.addEventListener("abort",_2.hitch(this,function(evt){
this.onAbort(evt);
clearInterval(_10);
}),false);
xhr.onreadystatechange=_2.hitch(this,function(){
if(xhr.readyState===4){
clearInterval(_10);
this.onComplete(JSON.parse(xhr.responseText.replace(/^\{\}&&/,"")));
}
});
xhr.open("POST",this.getUrl());
_10=setInterval(_2.hitch(this,function(){
try{
if(typeof (xhr.statusText)){
}
}
catch(e){
clearInterval(_10);
}
}),250);
return xhr;
},_buildRequestBody:function(_11,_12){
var EOL="\r\n";
var _13="";
_12="--"+_12;
var _14=[],_15=this._files;
_3.forEach(_15,function(f,i){
var _16=this.name+"s[]";
var _17=f.fileName;
var _18;
try{
_18=f.getAsBinary()+EOL;
_13+=_12+EOL;
_13+="Content-Disposition: form-data; ";
_13+="name=\""+_16+"\"; ";
_13+="filename=\""+_17+"\""+EOL;
_13+="Content-Type: "+this.getMimeType()+EOL+EOL;
_13+=_18;
}
catch(e){
_14.push({index:i,name:_17});
}
},this);
if(_14.length){
if(_14.length>=_15.length){
this.onError({message:this.errMsg,filesInError:_14});
_13=false;
}
}
if(!_13){
return false;
}
if(_11){
for(var nm in _11){
_13+=_12+EOL;
_13+="Content-Disposition: form-data; ";
_13+="name=\""+nm+"\""+EOL+EOL;
_13+=_11[nm]+EOL;
}
}
_13+=_12+"--"+EOL;
return _13;
}});
dojox.form.addUploaderPlugin(_5);
return _5;
});
