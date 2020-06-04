//>>built
require({cache:{"url:dojox/form/resources/UploaderFileList.html":"<div class=\"dojoxUploaderFileList\">\n\t<div data-dojo-attach-point=\"progressNode\" class=\"dojoxUploaderFileListProgress\">\n\t\t<div data-dojo-attach-point=\"percentBarNode\" class=\"dojoxUploaderFileListProgressBar\"></div>\n\t\t<div data-dojo-attach-point=\"percentTextNode\" class=\"dojoxUploaderFileListPercentText\">0%</div>\n\t</div>\n\t<table class=\"dojoxUploaderFileListTable\">\n\t\t<thead>\n\t\t\t<tr class=\"dojoxUploaderFileListHeader\">\n\t\t\t\t<th class=\"dojoxUploaderIndex\">${headerIndex}</th>\n\t\t\t\t<th class=\"dojoxUploaderIcon\">${headerType}</th>\n\t\t\t\t<th class=\"dojoxUploaderFileName\">${headerFilename}</th>\n\t\t\t\t<th class=\"dojoxUploaderFileSize\" data-dojo-attach-point=\"sizeHeader\">${headerFilesize}</th>\n\t\t\t</tr>\n\t\t</thead>\n\t\t<tbody class=\"dojoxUploaderFileListContent\" data-dojo-attach-point=\"listNode\"></tbody>\n\t</table>\n<div>"}});
define("dojox/form/uploader/FileList",["dojo/_base/fx","dojo/dom-style","dojo/dom-class","dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dijit/_base/manager","dojox/form/uploader/_Base","dojo/text!../resources/UploaderFileList.html"],function(fx,_1,_2,_3,_4,_5,_6,_7,_8){
return _3("dojox.form.uploader.FileList",_7,{uploaderId:"",uploader:null,headerIndex:"#",headerType:"Type",headerFilename:"File Name",headerFilesize:"Size",_upCheckCnt:0,rowAmt:0,templateString:_8,postCreate:function(){
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
},hideProgress:function(_9){
var o=_9?{ani:true,endDisp:"none",beg:15,end:0}:{endDisp:"none",ani:false};
this._hideShowProgress(o);
},showProgress:function(_a){
var o=_a?{ani:true,endDisp:"block",beg:0,end:15}:{endDisp:"block",ani:false};
this._hideShowProgress(o);
},_progress:function(_b){
this.percentTextNode.innerHTML=_b.percent;
_1.set(this.percentBarNode,"width",_b.percent);
},_hideShowProgress:function(o){
var _c=this.progressNode;
var _d=function(){
_1.set(_c,"display",o.endDisp);
};
if(o.ani){
_1.set(_c,"display","block");
fx.animateProperty({node:_c,properties:{height:{start:o.beg,end:o.end,units:"px"}},onEnd:_d}).play();
}else{
_d();
}
},_onUploaderChange:function(_e){
this.reset();
_5.forEach(_e,function(f,i){
this._addRow(i+1,this.getFileType(f.name),f.name,f.size);
},this);
},_addRow:function(_f,_10,_11,_12){
var c,r=this.listNode.insertRow(-1);
c=r.insertCell(-1);
_2.add(c,"dojoxUploaderIndex");
c.innerHTML=_f;
c=r.insertCell(-1);
_2.add(c,"dojoxUploaderIcon");
c.innerHTML=_10;
c=r.insertCell(-1);
_2.add(c,"dojoxUploaderFileName");
c.innerHTML=_11;
if(this._fileSizeAvail){
c=r.insertCell(-1);
_2.add(c,"dojoxUploaderSize");
c.innerHTML=this.convertBytes(_12).value;
}
this.rowAmt++;
}});
});
