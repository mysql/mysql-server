//>>built
define("dojox/form/FileUploader",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/connect","dojo/_base/window","dojo/_base/sniff","dojo/query","dojo/dom-style","dojo/dom-geometry","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-form","dojo/_base/config","dijit/_base/manager","dojo/io/iframe","dojo/_base/Color","dojo/_base/unload","dijit/_Widget","dijit/_TemplatedMixin","dijit/_Contained","dojox/embed/Flash","dojox/embed/flashVars","dojox/html/styles"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19){
_1.deprecated("dojox.form.FileUploader","Use dojox.form.Uploader","2.0");
_2("dojox.form.FileUploader",[_14,_15,_16],{swfPath:_f.uploaderPath||require.toUrl("dojox/form/resources/fileuploader.swf"),templateString:"<div><div dojoAttachPoint=\"progNode\"><div dojoAttachPoint=\"progTextNode\"></div></div><div dojoAttachPoint=\"insideNode\" class=\"uploaderInsideNode\"></div></div>",uploadUrl:"",isDebug:false,devMode:false,baseClass:"dojoxUploaderNorm",hoverClass:"dojoxUploaderHover",activeClass:"dojoxUploaderActive",disabledClass:"dojoxUploaderDisabled",force:"",uploaderType:"",flashObject:null,flashMovie:null,insideNode:null,deferredUploading:1,fileListId:"",uploadOnChange:false,selectMultipleFiles:true,htmlFieldName:"uploadedfile",flashFieldName:"flashUploadFiles",fileMask:null,minFlashVersion:9,tabIndex:-1,showProgress:false,progressMessage:"Loading",progressBackgroundUrl:require.toUrl("dijit/themes/tundra/images/buttonActive.png"),progressBackgroundColor:"#ededed",progressWidgetId:"",skipServerCheck:false,serverTimeout:5000,log:function(){
if(this.isDebug){
console["log"](Array.prototype.slice.call(arguments).join(" "));
}
},constructor:function(){
this._subs=[];
},postMixInProperties:function(){
this.fileList=[];
this._cons=[];
this.fileMask=this.fileMask||[];
this.fileInputs=[];
this.fileCount=0;
this.flashReady=false;
this._disabled=false;
this.force=this.force.toLowerCase();
this.uploaderType=((_17.available>=this.minFlashVersion||this.force=="flash")&&this.force!="html")?"flash":"html";
this.deferredUploading=this.deferredUploading===true?1:this.deferredUploading;
this._refNode=this.srcNodeRef;
this.getButtonStyle();
},startup:function(){
},postCreate:function(){
this.inherited(arguments);
this.setButtonStyle();
var _1a;
if(this.uploaderType=="flash"){
_1a="createFlashUploader";
}else{
this.uploaderType="html";
_1a="createHtmlUploader";
}
this[_1a]();
if(this.fileListId){
this.connect(dom.byId(this.fileListId),"click",function(evt){
var p=evt.target.parentNode.parentNode.parentNode;
if(p.id&&p.id.indexOf("file_")>-1){
this.removeFile(p.id.split("file_")[1]);
}
});
}
_13.addOnUnload(this,this.destroy);
},getHiddenNode:function(_1b){
if(!_1b){
return null;
}
var _1c=null;
var p=_1b.parentNode;
while(p&&p.tagName.toLowerCase()!="body"){
var d=_9.get(p,"display");
if(d=="none"){
_1c=p;
break;
}
p=p.parentNode;
}
return _1c;
},getButtonStyle:function(){
var _1d=this.srcNodeRef;
this._hiddenNode=this.getHiddenNode(_1d);
if(this._hiddenNode){
_9.set(this._hiddenNode,"display","block");
}
if(!_1d&&this.button&&this.button.domNode){
var _1e=true;
var cls=this.button.domNode.className+" dijitButtonNode";
var txt=this.getText(_8(".dijitButtonText",this.button.domNode)[0]);
var _1f="<button id=\""+this.button.id+"\" class=\""+cls+"\">"+txt+"</button>";
_1d=_d.place(_1f,this.button.domNode,"after");
this.srcNodeRef=_1d;
this.button.destroy();
this.baseClass="dijitButton";
this.hoverClass="dijitButtonHover";
this.pressClass="dijitButtonActive";
this.disabledClass="dijitButtonDisabled";
}else{
if(!this.srcNodeRef&&this.button){
_1d=this.button;
}
}
if(_b.get(_1d,"class")){
this.baseClass+=" "+_b.get(_1d,"class");
}
_b.set(_1d,"class",this.baseClass);
this.norm=this.getStyle(_1d);
this.width=this.norm.w;
this.height=this.norm.h;
if(this.uploaderType=="flash"){
this.over=this.getTempNodeStyle(_1d,this.baseClass+" "+this.hoverClass,_1e);
this.down=this.getTempNodeStyle(_1d,this.baseClass+" "+this.activeClass,_1e);
this.dsbl=this.getTempNodeStyle(_1d,this.baseClass+" "+this.disabledClass,_1e);
this.fhtml={cn:this.getText(_1d),nr:this.norm,ov:this.over,dn:this.down,ds:this.dsbl};
}else{
this.fhtml={cn:this.getText(_1d),nr:this.norm};
if(this.norm.va=="middle"){
this.norm.lh=this.norm.h;
}
}
if(this.devMode){
this.log("classes - base:",this.baseClass," hover:",this.hoverClass,"active:",this.activeClass);
this.log("fhtml:",this.fhtml);
this.log("norm:",this.norm);
this.log("over:",this.over);
this.log("down:",this.down);
}
},setButtonStyle:function(){
_9.set(this.domNode,{width:this.fhtml.nr.w+"px",height:(this.fhtml.nr.h)+"px",padding:"0px",lineHeight:"normal",position:"relative"});
if(this.uploaderType=="html"&&this.norm.va=="middle"){
_9.set(this.domNode,"lineHeight",this.norm.lh+"px");
}
if(this.showProgress){
this.progTextNode.innerHTML=this.progressMessage;
_9.set(this.progTextNode,{width:this.fhtml.nr.w+"px",height:(this.fhtml.nr.h+0)+"px",padding:"0px",margin:"0px",left:"0px",lineHeight:(this.fhtml.nr.h+0)+"px",position:"absolute"});
_9.set(this.progNode,{width:this.fhtml.nr.w+"px",height:(this.fhtml.nr.h+0)+"px",padding:"0px",margin:"0px",left:"0px",position:"absolute",display:"none",backgroundImage:"url("+this.progressBackgroundUrl+")",backgroundPosition:"bottom",backgroundRepeat:"repeat-x",backgroundColor:this.progressBackgroundColor});
}else{
_d.destroy(this.progNode);
}
_9.set(this.insideNode,{position:"absolute",top:"0px",left:"0px",display:""});
_c.add(this.domNode,this.srcNodeRef.className);
if(this.fhtml.nr.d.indexOf("inline")>-1){
_c.add(this.domNode,"dijitInline");
}
try{
this.insideNode.innerHTML=this.fhtml.cn;
}
catch(e){
if(this.uploaderType=="flash"){
this.insideNode=this.insideNode.parentNode.removeChild(this.insideNode);
_6.body().appendChild(this.insideNode);
this.insideNode.innerHTML=this.fhtml.cn;
var c=_5.connect(this,"onReady",this,function(){
_5.disconnect(c);
this.insideNode=this.insideNode.parentNode.removeChild(this.insideNode);
this.domNode.appendChild(this.insideNode);
});
}else{
this.insideNode.appendChild(document.createTextNode(this.fhtml.cn));
}
}
if(this._hiddenNode){
_9.set(this._hiddenNode,"display","none");
}
},onChange:function(_20){
},onProgress:function(_21){
},onComplete:function(_22){
},onCancel:function(){
},onError:function(_23){
},onReady:function(_24){
},onLoad:function(_25){
},submit:function(_26){
var _27=_26?_e.toObject(_26):null;
this.upload(_27);
return false;
},upload:function(_28){
if(!this.fileList.length){
return false;
}
if(!this.uploadUrl){
console.warn("uploadUrl not provided. Aborting.");
return false;
}
if(!this.showProgress){
this.set("disabled",true);
}
if(this.progressWidgetId){
var _29=_10.byId(this.progressWidgetId).domNode;
if(_9.get(_29,"display")=="none"){
this.restoreProgDisplay="none";
_9.set(_29,"display","block");
}
if(_9.get(_29,"visibility")=="hidden"){
this.restoreProgDisplay="hidden";
_9.set(_29,"visibility","visible");
}
}
if(_28&&!_28.target){
this.postData=_28;
}
this.log("upload type:",this.uploaderType," - postData:",this.postData);
for(var i=0;i<this.fileList.length;i++){
var f=this.fileList[i];
f.bytesLoaded=0;
f.bytesTotal=f.size||100000;
f.percent=0;
}
if(this.uploaderType=="flash"){
this.uploadFlash();
}else{
this.uploadHTML();
}
return false;
},removeFile:function(_2a,_2b){
var i;
for(i=0;i<this.fileList.length;i++){
if(this.fileList[i].name==_2a){
if(!_2b){
this.fileList.splice(i,1);
}
break;
}
}
if(this.uploaderType=="flash"){
this.flashMovie.removeFile(_2a);
}else{
if(!_2b){
_d.destroy(this.fileInputs[i]);
this.fileInputs.splice(i,1);
this._renumberInputs();
}
}
if(this.fileListId){
_d.destroy("file_"+_2a);
}
},destroy:function(){
if(this.uploaderType=="flash"&&!this.flashMovie){
this._cons.push(_5.connect(this,"onLoad",this,"destroy"));
return;
}
_4.forEach(this._subs,_5.unsubscribe,dojo);
_4.forEach(this._cons,_5.disconnect,dojo);
if(this.scrollConnect){
_5.disconnect(this.scrollConnect);
}
if(this.uploaderType=="flash"){
this.flashObject.destroy();
delete this.flashObject;
}else{
_d.destroy(this._fileInput);
_d.destroy(this._formNode);
}
this.inherited(arguments);
},_displayProgress:function(_2c){
if(_2c===true){
if(this.uploaderType=="flash"){
_9.set(this.insideNode,"top","-2500px");
}else{
_9.set(this.insideNode,"display","none");
}
_9.set(this.progNode,"display","");
}else{
if(_2c===false){
_9.set(this.insideNode,{display:"",top:"0"});
_9.set(this.progNode,"display","none");
}else{
var w=_2c*this.fhtml.nr.w;
_9.set(this.progNode,"width",w+"px");
}
}
},_animateProgress:function(){
this._displayProgress(true);
var _2d=false;
var c=_5.connect(this,"_complete",function(){
_5.disconnect(c);
_2d=true;
});
var w=0;
var _2e=setInterval(_3.hitch(this,function(){
w+=5;
if(w>this.fhtml.nr.w){
w=0;
_2d=true;
}
this._displayProgress(w/this.fhtml.nr.w);
if(_2d){
clearInterval(_2e);
setTimeout(_3.hitch(this,function(){
this._displayProgress(false);
}),500);
}
}),50);
},_error:function(evt){
if(typeof (evt)=="string"){
evt=new Error(evt);
}
this.onError(evt);
},_addToFileList:function(){
if(this.fileListId){
var str="";
_4.forEach(this.fileList,function(d){
str+="<table id=\"file_"+d.name+"\" class=\"fileToUpload\"><tr><td class=\"fileToUploadClose\"></td><td class=\"fileToUploadName\">"+d.name+"</td><td class=\"fileToUploadSize\">"+(d.size?Math.ceil(d.size*0.001)+"kb":"")+"</td></tr></table>";
},this);
dom.byId(this.fileListId).innerHTML=str;
}
},_change:function(_2f){
if(_7("ie")){
_4.forEach(_2f,function(f){
f.name=f.name.split("\\")[f.name.split("\\").length-1];
});
}
if(this.selectMultipleFiles){
this.fileList=this.fileList.concat(_2f);
}else{
if(this.fileList[0]){
this.removeFile(this.fileList[0].name,true);
}
this.fileList=_2f;
}
this._addToFileList();
this.onChange(_2f);
if(this.uploadOnChange){
if(this.uploaderType=="html"){
this._buildFileInput();
}
this.upload();
}else{
if(this.uploaderType=="html"&&this.selectMultipleFiles){
this._buildFileInput();
this._connectInput();
}
}
},_complete:function(_30){
_30=_3.isArray(_30)?_30:[_30];
_4.forEach(_30,function(f){
if(f.ERROR){
this._error(f.ERROR);
}
},this);
_4.forEach(this.fileList,function(f){
f.bytesLoaded=1;
f.bytesTotal=1;
f.percent=100;
this._progress(f);
},this);
_4.forEach(this.fileList,function(f){
this.removeFile(f.name,true);
},this);
this.onComplete(_30);
this.fileList=[];
this._resetHTML();
this.set("disabled",false);
if(this.restoreProgDisplay){
setTimeout(_3.hitch(this,function(){
_9.set(_10.byId(this.progressWidgetId).domNode,this.restoreProgDisplay=="none"?"display":"visibility",this.restoreProgDisplay);
}),500);
}
},_progress:function(_31){
var _32=0;
var _33=0;
for(var i=0;i<this.fileList.length;i++){
var f=this.fileList[i];
if(f.name==_31.name){
f.bytesLoaded=_31.bytesLoaded;
f.bytesTotal=_31.bytesTotal;
f.percent=Math.ceil(f.bytesLoaded/f.bytesTotal*100);
this.log(f.name,"percent:",f.percent);
}
_33+=Math.ceil(0.001*f.bytesLoaded);
_32+=Math.ceil(0.001*f.bytesTotal);
}
var _34=Math.ceil(_33/_32*100);
if(this.progressWidgetId){
_10.byId(this.progressWidgetId).update({progress:_34+"%"});
}
if(this.showProgress){
this._displayProgress(_34*0.01);
}
this.onProgress(this.fileList);
},_getDisabledAttr:function(){
return this._disabled;
},_setDisabledAttr:function(_35){
if(this._disabled==_35){
return;
}
if(this.uploaderType=="flash"){
if(!this.flashReady){
var _36=_5.connect(this,"onLoad",this,function(){
_5.disconnect(_36);
this._setDisabledAttr(_35);
});
return;
}
this._disabled=_35;
this.flashMovie.doDisable(_35);
}else{
this._disabled=_35;
_9.set(this._fileInput,"display",this._disabled?"none":"");
}
_c.toggle(this.domNode,this.disabledClass,_35);
},_onFlashBlur:function(){
this.flashMovie.blur();
if(!this.nextFocusObject&&this.tabIndex){
var _37=_8("[tabIndex]");
for(var i=0;i<_37.length;i++){
if(_37[i].tabIndex>=Number(this.tabIndex)+1){
this.nextFocusObject=_37[i];
break;
}
}
}
this.nextFocusObject.focus();
},_disconnect:function(){
_4.forEach(this._cons,_5.disconnect,dojo);
},uploadHTML:function(){
if(this.selectMultipleFiles){
_d.destroy(this._fileInput);
}
this._setHtmlPostData();
if(this.showProgress){
this._animateProgress();
}
var dfd=_11.send({url:this.uploadUrl.toString(),form:this._formNode,handleAs:"json",error:_3.hitch(this,function(err){
this._error("HTML Upload Error:"+err.message);
}),load:_3.hitch(this,function(_38,_39,_3a){
this._complete(_38);
})});
},createHtmlUploader:function(){
this._buildForm();
this._setFormStyle();
this._buildFileInput();
this._connectInput();
this._styleContent();
_9.set(this.insideNode,"visibility","visible");
this.onReady();
},_connectInput:function(){
this._disconnect();
this._cons.push(_5.connect(this._fileInput,"mouseover",this,function(evt){
_c.add(this.domNode,this.hoverClass);
this.onMouseOver(evt);
}));
this._cons.push(_5.connect(this._fileInput,"mouseout",this,function(evt){
setTimeout(_3.hitch(this,function(){
_c.remove(this.domNode,this.activeClass);
_c.remove(this.domNode,this.hoverClass);
this.onMouseOut(evt);
this._checkHtmlCancel("off");
}),0);
}));
this._cons.push(_5.connect(this._fileInput,"mousedown",this,function(evt){
_c.add(this.domNode,this.activeClass);
_c.remove(this.domNode,this.hoverClass);
this.onMouseDown(evt);
}));
this._cons.push(_5.connect(this._fileInput,"mouseup",this,function(evt){
_c.remove(this.domNode,this.activeClass);
this.onMouseUp(evt);
this.onClick(evt);
this._checkHtmlCancel("up");
}));
this._cons.push(_5.connect(this._fileInput,"change",this,function(){
this._checkHtmlCancel("change");
this._change([{name:this._fileInput.value,type:"",size:0}]);
}));
if(this.tabIndex>=0){
_b.set(this.domNode,"tabIndex",this.tabIndex);
}
},_checkHtmlCancel:function(_3b){
if(_3b=="change"){
this.dialogIsOpen=false;
}
if(_3b=="up"){
this.dialogIsOpen=true;
}
if(_3b=="off"){
if(this.dialogIsOpen){
this.onCancel();
}
this.dialogIsOpen=false;
}
},_styleContent:function(){
var o=this.fhtml.nr;
_9.set(this.insideNode,{width:o.w+"px",height:o.va=="middle"?o.h+"px":"auto",textAlign:o.ta,paddingTop:o.p[0]+"px",paddingRight:o.p[1]+"px",paddingBottom:o.p[2]+"px",paddingLeft:o.p[3]+"px"});
try{
_9.set(this.insideNode,"lineHeight","inherit");
}
catch(e){
}
},_resetHTML:function(){
if(this.uploaderType=="html"&&this._formNode){
this.fileInputs=[];
_8("*",this._formNode).forEach(function(n){
_d.destroy(n);
});
this.fileCount=0;
this._buildFileInput();
this._connectInput();
}
},_buildForm:function(){
if(this._formNode){
return;
}
if(_7("ie")<9||(_7("ie")&&_7("quirks"))){
this._formNode=document.createElement("<form enctype=\"multipart/form-data\" method=\"post\">");
this._formNode.encoding="multipart/form-data";
this._formNode.id=_10.getUniqueId("FileUploaderForm");
this.domNode.appendChild(this._formNode);
}else{
this._formNode=_d.create("form",{enctype:"multipart/form-data",method:"post",id:_10.getUniqueId("FileUploaderForm")},this.domNode);
}
},_buildFileInput:function(){
if(this._fileInput){
this._disconnect();
this._fileInput.id=this._fileInput.id+this.fileCount;
_9.set(this._fileInput,"display","none");
}
this._fileInput=document.createElement("input");
this.fileInputs.push(this._fileInput);
var nm=this.htmlFieldName;
var _3c=this.id;
if(this.selectMultipleFiles){
nm+=this.fileCount;
_3c+=this.fileCount;
this.fileCount++;
}
_b.set(this._fileInput,{id:this.id,name:nm,type:"file"});
_c.add(this._fileInput,"dijitFileInputReal");
this._formNode.appendChild(this._fileInput);
var _3d=_a.getMarginBox(this._fileInput);
_9.set(this._fileInput,{position:"relative",left:(this.fhtml.nr.w-_3d.w)+"px",opacity:0});
},_renumberInputs:function(){
if(!this.selectMultipleFiles){
return;
}
var nm;
this.fileCount=0;
_4.forEach(this.fileInputs,function(inp){
nm=this.htmlFieldName+this.fileCount;
this.fileCount++;
_b.set(inp,"name",nm);
},this);
},_setFormStyle:function(){
var _3e=Math.max(2,Math.max(Math.ceil(this.fhtml.nr.w/60),Math.ceil(this.fhtml.nr.h/15)));
_19.insertCssRule("#"+this._formNode.id+" input","font-size:"+_3e+"em");
_9.set(this.domNode,{overflow:"hidden",position:"relative"});
_9.set(this.insideNode,"position","absolute");
},_setHtmlPostData:function(){
if(this.postData){
for(var nm in this.postData){
_d.create("input",{type:"hidden",name:nm,value:this.postData[nm]},this._formNode);
}
}
},uploadFlash:function(){
try{
if(this.showProgress){
this._displayProgress(true);
var c=_5.connect(this,"_complete",this,function(){
_5.disconnect(c);
this._displayProgress(false);
});
}
var o={};
for(var nm in this.postData){
o[nm]=this.postData[nm];
}
this.flashMovie.doUpload(o);
}
catch(err){
this._error("FileUploader - Sorry, the SWF failed to initialize."+err);
}
},createFlashUploader:function(){
this.uploadUrl=this.uploadUrl.toString();
if(this.uploadUrl){
if(this.uploadUrl.toLowerCase().indexOf("http")<0&&this.uploadUrl.indexOf("/")!=0){
var loc=window.location.href.split("/");
loc.pop();
loc=loc.join("/")+"/";
this.uploadUrl=loc+this.uploadUrl;
this.log("SWF Fixed - Relative loc:",loc," abs loc:",this.uploadUrl);
}else{
this.log("SWF URL unmodified:",this.uploadUrl);
}
}else{
console.warn("Warning: no uploadUrl provided.");
}
var w=this.fhtml.nr.w;
var h=this.fhtml.nr.h;
var _3f={expressInstall:true,path:this.swfPath.uri||this.swfPath,width:w,height:h,allowScriptAccess:"always",allowNetworking:"all",vars:{uploadDataFieldName:this.flashFieldName,uploadUrl:this.uploadUrl,uploadOnSelect:this.uploadOnChange,deferredUploading:this.deferredUploading||0,selectMultipleFiles:this.selectMultipleFiles,id:this.id,isDebug:this.isDebug,devMode:this.devMode,flashButton:_18.serialize("fh",this.fhtml),fileMask:_18.serialize("fm",this.fileMask),noReturnCheck:this.skipServerCheck,serverTimeout:this.serverTimeout},params:{scale:"noscale",wmode:"opaque",allowScriptAccess:"always",allowNetworking:"all"}};
this.flashObject=new _17(_3f,this.insideNode);
this.flashObject.onError=_3.hitch(function(msg){
this._error("Flash Error: "+msg);
});
this.flashObject.onReady=_3.hitch(this,function(){
_9.set(this.insideNode,"visibility","visible");
this.log("FileUploader flash object ready");
this.onReady(this);
});
this.flashObject.onLoad=_3.hitch(this,function(mov){
this.flashMovie=mov;
this.flashReady=true;
this.onLoad(this);
});
this._connectFlash();
},_connectFlash:function(){
this._doSub("/filesSelected","_change");
this._doSub("/filesUploaded","_complete");
this._doSub("/filesProgress","_progress");
this._doSub("/filesError","_error");
this._doSub("/filesCanceled","onCancel");
this._doSub("/stageBlur","_onFlashBlur");
this._doSub("/up","onMouseUp");
this._doSub("/down","onMouseDown");
this._doSub("/over","onMouseOver");
this._doSub("/out","onMouseOut");
this.connect(this.domNode,"focus",function(){
this.flashMovie.focus();
this.flashMovie.doFocus();
});
if(this.tabIndex>=0){
_b.set(this.domNode,"tabIndex",this.tabIndex);
}
},_doSub:function(_40,_41){
this._subs.push(_5.subscribe(this.id+_40,this,_41));
},urlencode:function(url){
if(!url||url=="none"){
return false;
}
return url.replace(/:/g,"||").replace(/\./g,"^^").replace("url(","").replace(")","").replace(/'/g,"").replace(/"/g,"");
},isButton:function(_42){
var tn=_42.tagName.toLowerCase();
return tn=="button"||tn=="input";
},getTextStyle:function(_43){
var o={};
o.ff=_9.get(_43,"fontFamily");
if(o.ff){
o.ff=o.ff.replace(", ",",");
o.ff=o.ff.replace(/\"|\'/g,"");
o.ff=o.ff=="sans-serif"?"Arial":o.ff;
o.fw=_9.get(_43,"fontWeight");
o.fi=_9.get(_43,"fontStyle");
o.fs=parseInt(_9.get(_43,"fontSize"),10);
if(_9.get(_43,"fontSize").indexOf("%")>-1){
var n=_43;
while(n.tagName){
if(_9.get(n,"fontSize").indexOf("%")==-1){
o.fs=parseInt(_9.get(n,"fontSize"),10);
break;
}
if(n.tagName.toLowerCase()=="body"){
o.fs=16*0.01*parseInt(_9.get(n,"fontSize"),10);
}
n=n.parentNode;
}
}
o.fc=new _12(_9.get(_43,"color")).toHex();
o.fc=parseInt(o.fc.substring(1,Infinity),16);
}
o.lh=_9.get(_43,"lineHeight");
o.ta=_9.get(_43,"textAlign");
o.ta=o.ta=="start"||!o.ta?"left":o.ta;
o.va=this.isButton(_43)?"middle":o.lh==o.h?"middle":_9.get(_43,"verticalAlign");
return o;
},getText:function(_44){
var cn=_3.trim(_44.innerHTML);
if(cn.indexOf("<")>-1){
cn=escape(cn);
}
return cn;
},getStyle:function(_45){
var o={};
var dim=_a.getContentBox(_45);
var pad=_a.getPadExtents(_45);
o.p=[pad.t,pad.w-pad.l,pad.h-pad.t,pad.l];
o.w=dim.w+pad.w;
o.h=dim.h+pad.h;
o.d=_9.get(_45,"display");
var clr=new _12(_9.get(_45,"backgroundColor"));
o.bc=clr.a==0?"#ffffff":clr.toHex();
o.bc=parseInt(o.bc.substring(1,Infinity),16);
var url=this.urlencode(_9.get(_45,"backgroundImage"));
if(url){
o.bi={url:url,rp:_9.get(_45,"backgroundRepeat"),pos:escape(_9.get(_45,"backgroundPosition"))};
if(!o.bi.pos){
var rx=_9.get(_45,"backgroundPositionX");
var ry=_9.get(_45,"backgroundPositionY");
rx=(rx=="left")?"0%":(rx=="right")?"100%":rx;
ry=(ry=="top")?"0%":(ry=="bottom")?"100%":ry;
o.bi.pos=escape(rx+" "+ry);
}
}
return _3.mixin(o,this.getTextStyle(_45));
},getTempNodeStyle:function(_46,_47,_48){
var _49,_4a;
if(_48){
_49=_d.place("<"+_46.tagName+"><span>"+_46.innerHTML+"</span></"+_46.tagName+">",_46.parentNode);
var _4b=_49.firstChild;
_c.add(_4b,_46.className);
_c.add(_49,_47);
_4a=this.getStyle(_4b);
}else{
_49=_d.place("<"+_46.tagName+">"+_46.innerHTML+"</"+_46.tagName+">",_46.parentNode);
_c.add(_49,_46.className);
_c.add(_49,_47);
_49.id=_46.id;
_4a=this.getStyle(_49);
}
_d.destroy(_49);
return _4a;
}});
return dojox.form.FileUploader;
});
