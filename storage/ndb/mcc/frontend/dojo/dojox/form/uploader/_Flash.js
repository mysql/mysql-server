//>>built
define("dojox/form/uploader/_Flash",["dojo/dom-form","dojo/dom-style","dojo/dom-construct","dojo/dom-attr","dojo/_base/declare","dojo/_base/config","dojo/_base/connect","dojo/_base/lang","dojo/_base/array","dojox/embed/Flash"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _5("dojox.form.uploader._Flash",[],{swfPath:_6.uploaderPath||require.toUrl("dojox/form/resources/uploader.swf"),preventCache:true,skipServerCheck:true,serverTimeout:2000,isDebug:false,devMode:false,deferredUploading:0,postMixInProperties:function(){
if(this.uploadType==="flash"){
this._files=[];
this._fileMap={};
this._createInput=this._createFlashUploader;
this.getFileList=this.getFlashFileList;
this.reset=this.flashReset;
this.upload=this.uploadFlash;
this.fieldname="flashUploadFiles";
}
this.inherited(arguments);
},onReady:function(_b){
},onLoad:function(_c){
},onFileChange:function(_d){
},onFileProgress:function(_e){
},getFlashFileList:function(){
return this._files;
},flashReset:function(){
this.flashMovie.reset();
this._files=[];
this._fileMap={};
},uploadFlash:function(_f){
this.onBegin(this.getFileList());
_f=_f||{};
_f.returnType="F";
_f.uploadType=this.uploadType;
this.flashMovie.doUpload(_f);
},_change:function(_10){
this._files=this._files.concat(_10);
_9.forEach(_10,function(f){
f.bytesLoaded=0;
f.bytesTotal=f.size;
this._fileMap[f.name+"_"+f.size]=f;
},this);
this.onChange(this._files);
this.onFileChange(_10);
},_complete:function(_11){
var o=this._getCustomEvent();
o.type="load";
this.onComplete(_11);
},_progress:function(f){
this._fileMap[f.name+"_"+f.bytesTotal].bytesLoaded=f.bytesLoaded;
var o=this._getCustomEvent();
this.onFileProgress(f);
this.onProgress(o);
},_error:function(err){
this.onError(err);
},_onFlashBlur:function(_12){
},_getCustomEvent:function(){
var o={bytesLoaded:0,bytesTotal:0,type:"progress",timeStamp:new Date().getTime()};
for(var nm in this._fileMap){
o.bytesTotal+=this._fileMap[nm].bytesTotal;
o.bytesLoaded+=this._fileMap[nm].bytesLoaded;
}
o.decimal=o.bytesLoaded/o.bytesTotal;
o.percent=Math.ceil((o.bytesLoaded/o.bytesTotal)*100)+"%";
return o;
},_connectFlash:function(){
this._subs=[];
this._cons=[];
var _13=_8.hitch(this,function(s,_14){
this._subs.push(_7.subscribe(this.id+s,this,_14));
});
_13("/filesSelected","_change");
_13("/filesUploaded","_complete");
_13("/filesProgress","_progress");
_13("/filesError","_error");
_13("/filesCanceled","onCancel");
_13("/stageBlur","_onFlashBlur");
this.connect(this.domNode,"focus",function(){
this.flashMovie.focus();
this.flashMovie.doFocus();
});
if(this.tabIndex>=0){
_4.set(this.domNode,"tabIndex",this.tabIndex);
}
},_createFlashUploader:function(){
var w=this.btnSize.w;
var h=this.btnSize.h;
if(!w){
setTimeout(dojo.hitch(this,function(){
this._getButtonStyle(this.domNode);
this._createFlashUploader();
}),200);
return;
}
var url=this.getUrl();
if(url){
if(url.toLowerCase().indexOf("http")<0&&url.indexOf("/")!=0){
var loc=window.location.href.split("/");
loc.pop();
loc=loc.join("/")+"/";
url=loc+url;
}
}else{
console.warn("Warning: no uploadUrl provided.");
}
this.inputNode=_3.create("div",{className:"dojoxFlashNode"},this.domNode,"first");
_2.set(this.inputNode,{position:"absolute",top:"-2px",width:w+"px",height:h+"px",opacity:0});
var _15={expressInstall:true,path:(this.swfPath.uri||this.swfPath)+((this.preventCache)?("?cb_"+(new Date().getTime())):""),width:w,height:h,allowScriptAccess:"always",allowNetworking:"all",vars:{uploadDataFieldName:this.flashFieldName||this.name+"Flash",uploadUrl:url,uploadOnSelect:this.uploadOnSelect,deferredUploading:this.deferredUploading||0,selectMultipleFiles:this.multiple,id:this.id,isDebug:this.isDebug,noReturnCheck:this.skipServerCheck,serverTimeout:this.serverTimeout},params:{scale:"noscale",wmode:"opaque",allowScriptAccess:"always",allowNetworking:"all"}};
this.flashObject=new _a(_15,this.inputNode);
this.flashObject.onError=_8.hitch(function(msg){
console.error("Flash Error: "+msg);
});
this.flashObject.onReady=_8.hitch(this,function(){
this.onReady(this);
});
this.flashObject.onLoad=_8.hitch(this,function(mov){
this.flashMovie=mov;
this.flashReady=true;
this.onLoad(this);
});
this._connectFlash();
}});
});
