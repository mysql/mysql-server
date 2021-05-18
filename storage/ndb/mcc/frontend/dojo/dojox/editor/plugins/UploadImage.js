//>>built
define("dojox/editor/plugins/UploadImage",["dojo","dijit","dojox","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare","dojox/form/FileUploader","dijit/_editor/_Plugin"],function(_1,_2,_3,_4){
_1.experimental("dojox.editor.plugins.UploadImage");
var _5=_1.declare("dojox.editor.plugins.UploadImage",_4,{tempImageUrl:"",iconClassPrefix:"editorIcon",useDefaultCommand:false,uploadUrl:"",button:null,label:"Upload",setToolbar:function(_6){
this.button.destroy();
this.createFileInput();
_6.addChild(this.button);
},_initButton:function(){
this.command="uploadImage";
this.editor.commands[this.command]="Upload Image";
this.inherited("_initButton",arguments);
delete this.command;
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},createFileInput:function(){
var _7=_1.create("span",{innerHTML:"."},document.body);
_1.style(_7,{width:"40px",height:"20px",paddingLeft:"8px",paddingRight:"8px"});
this.button=new _3.form.FileUploader({isDebug:true,uploadUrl:this.uploadUrl,uploadOnChange:true,selectMultipleFiles:false,baseClass:"dojoxEditorUploadNorm",hoverClass:"dojoxEditorUploadHover",activeClass:"dojoxEditorUploadActive",disabledClass:"dojoxEditorUploadDisabled"},_7);
this.connect(this.button,"onChange","insertTempImage");
this.connect(this.button,"onComplete","onComplete");
},onComplete:function(_8,_9,_a){
_8=_8[0];
var _b=_1.byId(this.currentImageId,this.editor.document);
var _c;
if(this.downloadPath){
_c=this.downloadPath+_8.name;
}else{
_c=_8.file;
}
_b.src=_c;
_1.attr(_b,"_djrealurl",_c);
if(_8.width){
_b.width=_8.width;
_b.height=_8.height;
}
},insertTempImage:function(){
this.currentImageId="img_"+(new Date().getTime());
var _d="<img id=\""+this.currentImageId+"\" src=\""+this.tempImageUrl+"\" width=\"32\" height=\"32\"/>";
this.editor.execCommand("inserthtml",_d);
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
switch(o.args.name){
case "uploadImage":
o.plugin=new _5({url:o.args.url});
}
});
return _5;
});
