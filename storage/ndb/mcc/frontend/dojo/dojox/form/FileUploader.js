//>>built
define("dojox/form/FileUploader",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/connect","dojo/_base/window","dojo/_base/sniff","dojo/query","dojo/dom","dojo/dom-style","dojo/dom-geometry","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-form","dojo/_base/config","dijit/_base/manager","dojo/io/iframe","dojo/_base/Color","dojo/_base/unload","dijit/_Widget","dijit/_TemplatedMixin","dijit/_Contained","dojox/embed/Flash","dojox/embed/flashVars","dojox/html/styles"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1a){
_1.deprecated("dojox.form.FileUploader","Use dojox.form.Uploader","2.0");
return _2("dojox.form.FileUploader",[_15,_16,_17],{swfPath:_10.uploaderPath||require.toUrl("dojox/form/resources/fileuploader.swf"),templateString:"<div><div dojoAttachPoint=\"progNode\"><div dojoAttachPoint=\"progTextNode\"></div></div><div dojoAttachPoint=\"insideNode\" class=\"uploaderInsideNode\"></div></div>",uploadUrl:"",isDebug:false,devMode:false,baseClass:"dojoxUploaderNorm",hoverClass:"dojoxUploaderHover",activeClass:"dojoxUploaderActive",disabledClass:"dojoxUploaderDisabled",force:"",uploaderType:"",flashObject:null,flashMovie:null,insideNode:null,deferredUploading:1,fileListId:"",uploadOnChange:false,selectMultipleFiles:true,htmlFieldName:"uploadedfile",flashFieldName:"flashUploadFiles",fileMask:null,minFlashVersion:9,tabIndex:-1,showProgress:false,progressMessage:"Loading",progressBackgroundUrl:require.toUrl("dijit/themes/tundra/images/buttonActive.png"),progressBackgroundColor:"#ededed",progressWidgetId:"",skipServerCheck:false,serverTimeout:5000,log:function(){
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
this.uploaderType=((_18.available>=this.minFlashVersion||this.force=="flash")&&this.force!="html")?"flash":"html";
this.deferredUploading=this.deferredUploading===true?1:this.deferredUploading;
this._refNode=this.srcNodeRef;
this.getButtonStyle();
},startup:function(){
},postCreate:function(){
this.inherited(arguments);
this.setButtonStyle();
var _1b;
if(this.uploaderType=="flash"){
_1b="createFlashUploader";
}else{
this.uploaderType="html";
_1b="createHtmlUploader";
}
this[_1b]();
if(this.fileListId){
this.connect(_9.byId(this.fileListId),"click",function(evt){
var p=evt.target.parentNode.parentNode.parentNode;
if(p.id&&p.id.indexOf("file_")>-1){
this.removeFile(p.id.split("file_")[1]);
}
});
}
_14.addOnUnload(this,this.destroy);
},getHiddenNode:function(_1c){
if(!_1c){
return null;
}
var _1d=null;
var p=_1c.parentNode;
while(p&&p.tagName.toLowerCase()!="body"){
var d=_a.get(p,"display");
if(d=="none"){
_1d=p;
break;
}
p=p.parentNode;
}
return _1d;
},getButtonStyle:function(){
var _1e=this.srcNodeRef;
this._hiddenNode=this.getHiddenNode(_1e);
if(this._hiddenNode){
_a.set(this._hiddenNode,"display","block");
}
if(!_1e&&this.button&&this.button.domNode){
var _1f=true;
var cls=this.button.domNode.className+" dijitButtonNode";
var txt=this.getText(_8(".dijitButtonText",this.button.domNode)[0]);
var _20="<button id=\""+this.button.id+"\" class=\""+cls+"\">"+txt+"</button>";
_1e=_e.place(_20,this.button.domNode,"after");
this.srcNodeRef=_1e;
this.button.destroy();
this.baseClass="dijitButton";
this.hoverClass="dijitButtonHover";
this.pressClass="dijitButtonActive";
this.disabledClass="dijitButtonDisabled";
}else{
if(!this.srcNodeRef&&this.button){
_1e=this.button;
}
}
if(_c.get(_1e,"class")){
this.baseClass+=" "+_c.get(_1e,"class");
}
_c.set(_1e,"class",this.baseClass);
this.norm=this.getStyle(_1e);
this.width=this.norm.w;
this.height=this.norm.h;
if(this.uploaderType=="flash"){
this.over=this.getTempNodeStyle(_1e,this.baseClass+" "+this.hoverClass,_1f);
this.down=this.getTempNodeStyle(_1e,this.baseClass+" "+this.activeClass,_1f);
this.dsbl=this.getTempNodeStyle(_1e,this.baseClass+" "+this.disabledClass,_1f);
this.fhtml={cn:this.getText(_1e),nr:this.norm,ov:this.over,dn:this.down,ds:this.dsbl};
}else{
this.fhtml={cn:this.getText(_1e),nr:this.norm};
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
_a.set(this.domNode,{width:this.fhtml.nr.w+"px",height:(this.fhtml.nr.h)+"px",padding:"0px",lineHeight:"normal",position:"relative"});
if(this.uploaderType=="html"&&this.norm.va=="middle"){
_a.set(this.domNode,"lineHeight",this.norm.lh+"px");
}
if(this.showProgress){
this.progTextNode.innerHTML=this.progressMessage;
_a.set(this.progTextNode,{width:this.fhtml.nr.w+"px",height:(this.fhtml.nr.h+0)+"px",padding:"0px",margin:"0px",left:"0px",lineHeight:(this.fhtml.nr.h+0)+"px",position:"absolute"});
_a.set(this.progNode,{width:this.fhtml.nr.w+"px",height:(this.fhtml.nr.h+0)+"px",padding:"0px",margin:"0px",left:"0px",position:"absolute",display:"none",backgroundImage:"url("+this.progressBackgroundUrl+")",backgroundPosition:"bottom",backgroundRepeat:"repeat-x",backgroundColor:this.progressBackgroundColor});
}else{
_e.destroy(this.progNode);
}
_a.set(this.insideNode,{position:"absolute",top:"0px",left:"0px",display:""});
_d.add(this.domNode,this.srcNodeRef.className);
if(this.fhtml.nr.d.indexOf("inline")>-1){
_d.add(this.domNode,"dijitInline");
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
_a.set(this._hiddenNode,"display","none");
}
},onChange:function(_21){
},onProgress:function(_22){
},onComplete:function(_23){
},onCancel:function(){
},onError:function(_24){
},onReady:function(_25){
},onLoad:function(_26){
},submit:function(_27){
var _28=_27?_f.toObject(_27):null;
this.upload(_28);
return false;
},upload:function(_29){
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
var _2a=_11.byId(this.progressWidgetId).domNode;
if(_a.get(_2a,"display")=="none"){
this.restoreProgDisplay="none";
_a.set(_2a,"display","block");
}
if(_a.get(_2a,"visibility")=="hidden"){
this.restoreProgDisplay="hidden";
_a.set(_2a,"visibility","visible");
}
}
if(_29&&!_29.target){
this.postData=_29;
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
},removeFile:function(_2b,_2c){
var i;
for(i=0;i<this.fileList.length;i++){
if(this.fileList[i].name==_2b){
if(!_2c){
this.fileList.splice(i,1);
}
break;
}
}
if(this.uploaderType=="flash"){
this.flashMovie.removeFile(_2b);
}else{
if(!_2c){
_e.destroy(this.fileInputs[i]);
this.fileInputs.splice(i,1);
this._renumberInputs();
}
}
if(this.fileListId){
_e.destroy("file_"+_2b);
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
_e.destroy(this._fileInput);
_e.destroy(this._formNode);
}
this.inherited(arguments);
},_displayProgress:function(_2d){
if(_2d===true){
if(this.uploaderType=="flash"){
_a.set(this.insideNode,"top","-2500px");
}else{
_a.set(this.insideNode,"display","none");
}
_a.set(this.progNode,"display","");
}else{
if(_2d===false){
_a.set(this.insideNode,{display:"",top:"0"});
_a.set(this.progNode,"display","none");
}else{
var w=_2d*this.fhtml.nr.w;
_a.set(this.progNode,"width",w+"px");
}
}
},_animateProgress:function(){
this._displayProgress(true);
var _2e=false;
var c=_5.connect(this,"_complete",function(){
_5.disconnect(c);
_2e=true;
});
var w=0;
var _2f=setInterval(_3.hitch(this,function(){
w+=5;
if(w>this.fhtml.nr.w){
w=0;
_2e=true;
}
this._displayProgress(w/this.fhtml.nr.w);
if(_2e){
clearInterval(_2f);
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
_9.byId(this.fileListId).innerHTML=str;
}
},_change:function(_30){
if(_7("ie")){
_4.forEach(_30,function(f){
f.name=f.name.split("\\")[f.name.split("\\").length-1];
});
}
if(this.selectMultipleFiles){
this.fileList=this.fileList.concat(_30);
}else{
if(this.fileList[0]){
this.removeFile(this.fileList[0].name,true);
}
this.fileList=_30;
}
this._addToFileList();
this.onChange(_30);
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
},_complete:function(_31){
_31=_3.isArray(_31)?_31:[_31];
_4.forEach(_31,function(f){
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
this.onComplete(_31);
this.fileList=[];
this._resetHTML();
this.set("disabled",false);
if(this.restoreProgDisplay){
setTimeout(_3.hitch(this,function(){
_a.set(_11.byId(this.progressWidgetId).domNode,this.restoreProgDisplay=="none"?"display":"visibility",this.restoreProgDisplay);
}),500);
}
},_progress:function(_32){
var _33=0;
var _34=0;
for(var i=0;i<this.fileList.length;i++){
var f=this.fileList[i];
if(f.name==_32.name){
f.bytesLoaded=_32.bytesLoaded;
f.bytesTotal=_32.bytesTotal;
f.percent=Math.ceil(f.bytesLoaded/f.bytesTotal*100);
this.log(f.name,"percent:",f.percent);
}
_34+=Math.ceil(0.001*f.bytesLoaded);
_33+=Math.ceil(0.001*f.bytesTotal);
}
var _35=Math.ceil(_34/_33*100);
if(this.progressWidgetId){
_11.byId(this.progressWidgetId).update({progress:_35+"%"});
}
if(this.showProgress){
this._displayProgress(_35*0.01);
}
this.onProgress(this.fileList);
},_getDisabledAttr:function(){
return this._disabled;
},_setDisabledAttr:function(_36){
if(this._disabled==_36){
return;
}
if(this.uploaderType=="flash"){
if(!this.flashReady){
var _37=_5.connect(this,"onLoad",this,function(){
_5.disconnect(_37);
this._setDisabledAttr(_36);
});
return;
}
this._disabled=_36;
this.flashMovie.doDisable(_36);
}else{
this._disabled=_36;
_a.set(this._fileInput,"display",this._disabled?"none":"");
}
_d.toggle(this.domNode,this.disabledClass,_36);
},_onFlashBlur:function(){
this.flashMovie.blur();
if(!this.nextFocusObject&&this.tabIndex){
var _38=_8("[tabIndex]");
for(var i=0;i<_38.length;i++){
if(_38[i].tabIndex>=Number(this.tabIndex)+1){
this.nextFocusObject=_38[i];
break;
}
}
}
this.nextFocusObject.focus();
},_disconnect:function(){
_4.forEach(this._cons,_5.disconnect,dojo);
},uploadHTML:function(){
if(this.selectMultipleFiles){
_e.destroy(this._fileInput);
}
this._setHtmlPostData();
if(this.showProgress){
this._animateProgress();
}
var dfd=_12.send({url:this.uploadUrl.toString(),form:this._formNode,handleAs:"json",error:_3.hitch(this,function(err){
this._error("HTML Upload Error:"+err.message);
}),load:_3.hitch(this,function(_39,_3a,_3b){
this._complete(_39);
})});
},createHtmlUploader:function(){
this._buildForm();
this._setFormStyle();
this._buildFileInput();
this._connectInput();
this._styleContent();
_a.set(this.insideNode,"visibility","visible");
this.onReady();
},_connectInput:function(){
this._disconnect();
this._cons.push(_5.connect(this._fileInput,"mouseover",this,function(evt){
_d.add(this.domNode,this.hoverClass);
this.onMouseOver(evt);
}));
this._cons.push(_5.connect(this._fileInput,"mouseout",this,function(evt){
setTimeout(_3.hitch(this,function(){
_d.remove(this.domNode,this.activeClass);
_d.remove(this.domNode,this.hoverClass);
this.onMouseOut(evt);
this._checkHtmlCancel("off");
}),0);
}));
this._cons.push(_5.connect(this._fileInput,"mousedown",this,function(evt){
_d.add(this.domNode,this.activeClass);
_d.remove(this.domNode,this.hoverClass);
this.onMouseDown(evt);
}));
this._cons.push(_5.connect(this._fileInput,"mouseup",this,function(evt){
_d.remove(this.domNode,this.activeClass);
this.onMouseUp(evt);
this.onClick(evt);
this._checkHtmlCancel("up");
}));
this._cons.push(_5.connect(this._fileInput,"change",this,function(){
this._checkHtmlCancel("change");
this._change([{name:this._fileInput.value,type:"",size:0}]);
}));
if(this.tabIndex>=0){
_c.set(this.domNode,"tabIndex",this.tabIndex);
}
},_checkHtmlCancel:function(_3c){
if(_3c=="change"){
this.dialogIsOpen=false;
}
if(_3c=="up"){
this.dialogIsOpen=true;
}
if(_3c=="off"){
if(this.dialogIsOpen){
this.onCancel();
}
this.dialogIsOpen=false;
}
},_styleContent:function(){
var o=this.fhtml.nr;
_a.set(this.insideNode,{width:o.w+"px",height:o.va=="middle"?o.h+"px":"auto",textAlign:o.ta,paddingTop:o.p[0]+"px",paddingRight:o.p[1]+"px",paddingBottom:o.p[2]+"px",paddingLeft:o.p[3]+"px"});
try{
_a.set(this.insideNode,"lineHeight","inherit");
}
catch(e){
}
},_resetHTML:function(){
if(this.uploaderType=="html"&&this._formNode){
this.fileInputs=[];
_8("*",this._formNode).forEach(function(n){
_e.destroy(n);
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
this._formNode.id=_11.getUniqueId("FileUploaderForm");
this.domNode.appendChild(this._formNode);
}else{
this._formNode=_e.create("form",{enctype:"multipart/form-data",method:"post",id:_11.getUniqueId("FileUploaderForm")},this.domNode);
}
},_buildFileInput:function(){
if(this._fileInput){
this._disconnect();
this._fileInput.id=this._fileInput.id+this.fileCount;
_a.set(this._fileInput,"display","none");
}
this._fileInput=document.createElement("input");
this.fileInputs.push(this._fileInput);
var nm=this.htmlFieldName;
var _3d=this.id;
if(this.selectMultipleFiles){
nm+=this.fileCount;
_3d+=this.fileCount;
this.fileCount++;
}
_c.set(this._fileInput,{id:this.id,name:nm,type:"file"});
_d.add(this._fileInput,"dijitFileInputReal");
this._formNode.appendChild(this._fileInput);
var _3e=_b.getMarginBox(this._fileInput);
_a.set(this._fileInput,{position:"relative",left:(this.fhtml.nr.w-_3e.w)+"px",opacity:0});
},_renumberInputs:function(){
if(!this.selectMultipleFiles){
return;
}
var nm;
this.fileCount=0;
_4.forEach(this.fileInputs,function(inp){
nm=this.htmlFieldName+this.fileCount;
this.fileCount++;
_c.set(inp,"name",nm);
},this);
},_setFormStyle:function(){
var _3f=Math.max(2,Math.max(Math.ceil(this.fhtml.nr.w/60),Math.ceil(this.fhtml.nr.h/15)));
_1a.insertCssRule("#"+this._formNode.id+" input","font-size:"+_3f+"em");
_a.set(this.domNode,{overflow:"hidden",position:"relative"});
_a.set(this.insideNode,"position","absolute");
},_setHtmlPostData:function(){
if(this.postData){
for(var nm in this.postData){
_e.create("input",{type:"hidden",name:nm,value:this.postData[nm]},this._formNode);
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
var _40={expressInstall:true,path:this.swfPath.uri||this.swfPath,width:w,height:h,allowScriptAccess:"always",allowNetworking:"all",vars:{uploadDataFieldName:this.flashFieldName,uploadUrl:this.uploadUrl,uploadOnSelect:this.uploadOnChange,deferredUploading:this.deferredUploading||0,selectMultipleFiles:this.selectMultipleFiles,id:this.id,isDebug:this.isDebug,devMode:this.devMode,flashButton:_19.serialize("fh",this.fhtml),fileMask:_19.serialize("fm",this.fileMask),noReturnCheck:this.skipServerCheck,serverTimeout:this.serverTimeout},params:{scale:"noscale",wmode:"opaque",allowScriptAccess:"always",allowNetworking:"all"}};
this.flashObject=new _18(_40,this.insideNode);
this.flashObject.onError=_3.hitch(function(msg){
this._error("Flash Error: "+msg);
});
this.flashObject.onReady=_3.hitch(this,function(){
_a.set(this.insideNode,"visibility","visible");
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
_c.set(this.domNode,"tabIndex",this.tabIndex);
}
},_doSub:function(_41,_42){
this._subs.push(_5.subscribe(this.id+_41,this,_42));
},urlencode:function(url){
if(!url||url=="none"){
return false;
}
return url.replace(/:/g,"||").replace(/\./g,"^^").replace("url(","").replace(")","").replace(/'/g,"").replace(/"/g,"");
},isButton:function(_43){
var tn=_43.tagName.toLowerCase();
return tn=="button"||tn=="input";
},getTextStyle:function(_44){
var o={};
o.ff=_a.get(_44,"fontFamily");
if(o.ff){
o.ff=o.ff.replace(", ",",");
o.ff=o.ff.replace(/\"|\'/g,"");
o.ff=o.ff=="sans-serif"?"Arial":o.ff;
o.fw=_a.get(_44,"fontWeight");
o.fi=_a.get(_44,"fontStyle");
o.fs=parseInt(_a.get(_44,"fontSize"),10);
if(_a.get(_44,"fontSize").indexOf("%")>-1){
var n=_44;
while(n.tagName){
if(_a.get(n,"fontSize").indexOf("%")==-1){
o.fs=parseInt(_a.get(n,"fontSize"),10);
break;
}
if(n.tagName.toLowerCase()=="body"){
o.fs=16*0.01*parseInt(_a.get(n,"fontSize"),10);
}
n=n.parentNode;
}
}
o.fc=new _13(_a.get(_44,"color")).toHex();
o.fc=parseInt(o.fc.substring(1,Infinity),16);
}
o.lh=_a.get(_44,"lineHeight");
o.ta=_a.get(_44,"textAlign");
o.ta=o.ta=="start"||!o.ta?"left":o.ta;
o.va=this.isButton(_44)?"middle":o.lh==o.h?"middle":_a.get(_44,"verticalAlign");
return o;
},getText:function(_45){
var cn=_3.trim(_45.innerHTML);
if(cn.indexOf("<")>-1){
cn=escape(cn);
}
return cn;
},getStyle:function(_46){
var o={};
var dim=_b.getContentBox(_46);
var pad=_b.getPadExtents(_46);
o.p=[pad.t,pad.w-pad.l,pad.h-pad.t,pad.l];
o.w=dim.w+pad.w;
o.h=dim.h+pad.h;
o.d=_a.get(_46,"display");
var clr=new _13(_a.get(_46,"backgroundColor"));
o.bc=clr.a==0?"#ffffff":clr.toHex();
o.bc=parseInt(o.bc.substring(1,Infinity),16);
var url=this.urlencode(_a.get(_46,"backgroundImage"));
if(url){
o.bi={url:url,rp:_a.get(_46,"backgroundRepeat"),pos:escape(_a.get(_46,"backgroundPosition"))};
if(!o.bi.pos){
var rx=_a.get(_46,"backgroundPositionX");
var ry=_a.get(_46,"backgroundPositionY");
rx=(rx=="left")?"0%":(rx=="right")?"100%":rx;
ry=(ry=="top")?"0%":(ry=="bottom")?"100%":ry;
o.bi.pos=escape(rx+" "+ry);
}
}
return _3.mixin(o,this.getTextStyle(_46));
},getTempNodeStyle:function(_47,_48,_49){
var _4a,_4b;
if(_49){
_4a=_e.place("<"+_47.tagName+"><span>"+_47.innerHTML+"</span></"+_47.tagName+">",_47.parentNode);
var _4c=_4a.firstChild;
_d.add(_4c,_47.className);
_d.add(_4a,_48);
_4b=this.getStyle(_4c);
}else{
_4a=_e.place("<"+_47.tagName+">"+_47.innerHTML+"</"+_47.tagName+">",_47.parentNode);
_d.add(_4a,_47.className);
_d.add(_4a,_48);
_4a.id=_47.id;
_4b=this.getStyle(_4a);
}
_e.destroy(_4a);
return _4b;
}});
});
