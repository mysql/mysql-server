//>>built
define("dojox/form/uploader/Base",["dojo/dom-form","dojo/dom-style","dojo/dom-construct","dojo/dom-attr","dojo/has","dojo/_base/declare","dojo/_base/event","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
_5.add("FormData",function(){
return !!window.FormData;
});
_5.add("xhr-sendAsBinary",function(){
var _b=window.XMLHttpRequest&&new window.XMLHttpRequest();
return _b&&!!_b.sendAsBinary;
});
_5.add("file-multiple",function(){
return !!({"true":1,"false":1}[_4.get(document.createElement("input",{type:"file"}),"multiple")]);
});
return _6("dojox.form.uploader.Base",[_8,_9,_a],{getForm:function(){
if(!this.form){
var n=this.domNode;
while(n&&n.tagName&&n!==document.body){
if(n.tagName.toLowerCase()=="form"){
this.form=n;
break;
}
n=n.parentNode;
}
}
return this.form;
},getUrl:function(){
if(this.uploadUrl){
this.url=this.uploadUrl;
}
if(this.url){
return this.url;
}
if(this.getForm()){
this.url=this.form.action;
}
return this.url;
},connectForm:function(){
this.url=this.getUrl();
if(!this._fcon&&!!this.getForm()){
this._fcon=true;
this.connect(this.form,"onsubmit",function(_c){
_7.stop(_c);
this.submit(this.form);
});
}
},supports:function(_d){
switch(_d){
case "multiple":
if(this.force=="flash"||this.force=="iframe"){
return false;
}
return _5("file-multiple");
case "FormData":
return _5(_d);
case "sendAsBinary":
return _5("xhr-sendAsBinary");
}
return false;
},getMimeType:function(){
return "application/octet-stream";
},getFileType:function(_e){
return _e.substring(_e.lastIndexOf(".")+1).toUpperCase();
},convertBytes:function(_f){
var kb=Math.round(_f/1024*100000)/100000;
var mb=Math.round(_f/1048576*100000)/100000;
var gb=Math.round(_f/1073741824*100000)/100000;
var _10=_f;
if(kb>1){
_10=kb.toFixed(1)+" kb";
}
if(mb>1){
_10=mb.toFixed(1)+" mb";
}
if(gb>1){
_10=gb.toFixed(1)+" gb";
}
return {kb:kb,mb:mb,gb:gb,bytes:_f,value:_10};
}});
});
