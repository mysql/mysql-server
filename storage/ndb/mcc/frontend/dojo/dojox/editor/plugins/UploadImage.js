//>>built
define("dojox/editor/plugins/UploadImage",["dojo","dijit","dojox","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare","dojox/form/FileUploader","dijit/_editor/_Plugin"],function(_1,_2,_3,_4){
_1.experimental("dojox.editor.plugins.UploadImage");
_1.declare("dojox.editor.plugins.UploadImage",_4,{tempImageUrl:"",iconClassPrefix:"editorIcon",useDefaultCommand:false,uploadUrl:"",button:null,label:"Upload",setToolbar:function(_5){
this.button.destroy();
this.createFileInput();
_5.addChild(this.button);
},_initButton:function(){
this.command="uploadImage";
this.editor.commands[this.command]="Upload Image";
this.inherited("_initButton",arguments);
delete this.command;
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},createFileInput:function(){
var _6=_1.create("span",{innerHTML:"."},document.body);
_1.style(_6,{width:"40px",height:"20px",paddingLeft:"8px",paddingRight:"8px"});
this.button=new _3.form.FileUploader({isDebug:true,uploadUrl:this.uploadUrl,uploadOnChange:true,selectMultipleFiles:false,baseClass:"dojoxEditorUploadNorm",hoverClass:"dojoxEditorUploadHover",activeClass:"dojoxEditorUploadActive",disabledClass:"dojoxEditorUploadDisabled"},_6);
this.connect(this.button,"onChange","insertTempImage");
this.connect(this.button,"onComplete","onComplete");
},onComplete:function(_7,_8,_9){
_7=_7[0];
var _a=_1.byId(this.currentImageId,this.editor.document);
var _b;
if(this.downloadPath){
_b=this.downloadPath+_7.name;
}else{
_b=_7.file;
}
_a.src=_b;
_1.attr(_a,"_djrealurl",_b);
if(_7.width){
_a.width=_7.width;
_a.height=_7.height;
}
},insertTempImage:function(){
this.currentImageId="img_"+(new Date().getTime());
var _c="<img id=\""+this.currentImageId+"\" src=\""+this.tempImageUrl+"\" width=\"32\" height=\"32\"/>";
this.editor.execCommand("inserthtml",_c);
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
switch(o.args.name){
case "uploadImage":
o.plugin=new _3.editor.plugins.UploadImage({url:o.args.url});
}
});
return _3.editor.plugins.UploadImage;
});
