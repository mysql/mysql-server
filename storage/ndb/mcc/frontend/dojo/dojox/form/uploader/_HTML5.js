//>>built
define("dojox/form/uploader/_HTML5",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo","dojo/request","dojo/has"],function(_1,_2,_3,_4,_5,_6){
return _1("dojox.form.uploader._HTML5",[],{errMsg:"Error uploading files. Try checking permissions",uploadType:"html5",postMixInProperties:function(){
this.inherited(arguments);
if(this.uploadType==="html5"){
}
},postCreate:function(){
this.connectForm();
this.inherited(arguments);
if(this.uploadOnSelect){
this.connect(this,"onChange",function(_7){
this.upload(_7[0]);
});
}
},_drop:function(e){
_4.stopEvent(e);
var dt=e.dataTransfer;
this._files=dt.files;
this.onChange(this.getFileList());
},upload:function(_8){
this.onBegin(this.getFileList());
this.uploadWithFormData(_8);
},addDropTarget:function(_9,_a){
if(!_a){
this.connect(_9,"dragenter",_4.stopEvent);
this.connect(_9,"dragover",_4.stopEvent);
this.connect(_9,"dragleave",_4.stopEvent);
}
this.connect(_9,"drop","_drop");
},uploadWithFormData:function(_b){
if(!this.getUrl()){
console.error("No upload url found.",this);
return;
}
var fd=new FormData(),_c=this._getFileFieldName();
_3.forEach(this._files,function(f,i){
fd.append(_c,f);
},this);
if(_b){
_b.uploadType=this.uploadType;
for(var nm in _b){
fd.append(nm,_b[nm]);
}
}
var _d=this;
var _e=_5(this.getUrl(),{method:"POST",data:fd,handleAs:"json",uploadProgress:true,headers:{Accept:"application/json"}},true);
_e.promise.response.otherwise(function(_f){
console.error(_f);
console.error(_f.response.text);
_d.onError(_f);
});
function _10(_11){
_d._xhrProgress(_11);
if(_11.type!=="load"){
return;
}
_d.onComplete(_e.response.data);
_e.response.xhr.removeEventListener("load",_10,false);
_e.response.xhr.upload.removeEventListener("progress",_10,false);
_e=null;
};
if(_6("native-xhr2")){
_e.response.xhr.addEventListener("load",_10,false);
_e.response.xhr.upload.addEventListener("progress",_10,false);
}else{
_e.promise.then(function(_12){
_d.onComplete(_12);
});
}
},_xhrProgress:function(evt){
if(evt.lengthComputable){
var o={bytesLoaded:evt.loaded,bytesTotal:evt.total,type:evt.type,timeStamp:evt.timeStamp};
if(evt.type=="load"){
o.percent="100%";
o.decimal=1;
}else{
o.decimal=evt.loaded/evt.total;
o.percent=Math.ceil((evt.loaded/evt.total)*100)+"%";
}
this.onProgress(o);
}
}});
});
