//>>built
define("dojox/form/uploader/FileList",["dojo/_base/fx","dojo/dom-style","dojo/dom-class","dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dijit/_base/manager","dojox/form/uploader/Base"],function(fx,_1,_2,_3,_4,_5,_6,_7){
return _3("dojox.form.uploader.FileList",[_7],{uploaderId:"",uploader:null,headerIndex:"#",headerType:"Type",headerFilename:"File Name",headerFilesize:"Size",_upCheckCnt:0,rowAmt:0,templateString:"<div class=\"dojoxUploaderFileList\">"+"<div dojoAttachPoint=\"progressNode\" class=\"dojoxUploaderFileListProgress\"><div dojoAttachPoint=\"percentBarNode\" class=\"dojoxUploaderFileListProgressBar\"></div><div dojoAttachPoint=\"percentTextNode\" class=\"dojoxUploaderFileListPercentText\">0%</div></div>"+"<table class=\"dojoxUploaderFileListTable\">"+"<thead><tr class=\"dojoxUploaderFileListHeader\"><th class=\"dojoxUploaderIndex\">${headerIndex}</th><th class=\"dojoxUploaderIcon\">${headerType}</th><th class=\"dojoxUploaderFileName\">${headerFilename}</th><th class=\"dojoxUploaderFileSize\" dojoAttachPoint=\"sizeHeader\">${headerFilesize}</th></tr></thead>"+"<tbody class=\"dojoxUploaderFileListContent\" dojoAttachPoint=\"listNode\">"+"</tbody>"+"</table>"+"<div>",postCreate:function(){
this.setUploader();
this.hideProgress();
},reset:function(){
for(var i=0;i<this.rowAmt;i++){
this.listNode.deleteRow(0);
}
this.rowAmt=0;
},setUploader:function(){
if(!this.uploaderId&&!this.uploader){
console.warn("uploaderId not passed to UploaderFileList");
}else{
if(this.uploaderId&&!this.uploader){
this.uploader=_6.byId(this.uploaderId);
}else{
if(this._upCheckCnt>4){
console.warn("uploader not found for ID ",this.uploaderId);
return;
}
}
}
if(this.uploader){
this.connect(this.uploader,"onChange","_onUploaderChange");
this.connect(this.uploader,"reset","reset");
this.connect(this.uploader,"onBegin",function(){
this.showProgress(true);
});
this.connect(this.uploader,"onProgress","_progress");
this.connect(this.uploader,"onComplete",function(){
setTimeout(_4.hitch(this,function(){
this.hideProgress(true);
}),1250);
});
if(!(this._fileSizeAvail={"html5":1,"flash":1}[this.uploader.uploadType])){
this.sizeHeader.style.display="none";
}
}else{
this._upCheckCnt++;
setTimeout(_4.hitch(this,"setUploader"),250);
}
},hideProgress:function(_8){
var o=_8?{ani:true,endDisp:"none",beg:15,end:0}:{endDisp:"none",ani:false};
this._hideShowProgress(o);
},showProgress:function(_9){
var o=_9?{ani:true,endDisp:"block",beg:0,end:15}:{endDisp:"block",ani:false};
this._hideShowProgress(o);
},_progress:function(_a){
this.percentTextNode.innerHTML=_a.percent;
_1.set(this.percentBarNode,"width",_a.percent);
},_hideShowProgress:function(o){
var _b=this.progressNode;
var _c=function(){
_1.set(_b,"display",o.endDisp);
};
if(o.ani){
_1.set(_b,"display","block");
fx.animateProperty({node:_b,properties:{height:{start:o.beg,end:o.end,units:"px"}},onEnd:_c}).play();
}else{
_c();
}
},_onUploaderChange:function(_d){
this.reset();
_5.forEach(_d,function(f,i){
this._addRow(i+1,this.getFileType(f.name),f.name,f.size);
},this);
},_addRow:function(_e,_f,_10,_11){
var c,r=this.listNode.insertRow(-1);
c=r.insertCell(-1);
_2.add(c,"dojoxUploaderIndex");
c.innerHTML=_e;
c=r.insertCell(-1);
_2.add(c,"dojoxUploaderIcon");
c.innerHTML=_f;
c=r.insertCell(-1);
_2.add(c,"dojoxUploaderFileName");
c.innerHTML=_10;
if(this._fileSizeAvail){
c=r.insertCell(-1);
_2.add(c,"dojoxUploaderSize");
c.innerHTML=this.convertBytes(_11).value;
}
this.rowAmt++;
}});
});
