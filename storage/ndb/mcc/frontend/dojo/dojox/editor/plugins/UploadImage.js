//>>built
define("dojox/editor/plugins/UploadImage",["dojo","dijit","dojox","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare","dojox/form/FileUploader","dijit/_editor/_Plugin"],function(_1,_2,_3){
_1.experimental("dojox.editor.plugins.UploadImage");
_1.declare("dojox.editor.plugins.UploadImage",_2._editor._Plugin,{tempImageUrl:"",iconClassPrefix:"editorIcon",useDefaultCommand:false,uploadUrl:"",button:null,label:"Upload",setToolbar:function(_4){
this.button.destroy();
this.createFileInput();
_4.addChild(this.button);
},_initButton:function(){
this.command="uploadImage";
this.editor.commands[this.command]="Upload Image";
this.inherited("_initButton",arguments);
delete this.command;
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},createFileInput:function(){
var _5=_1.create("span",{innerHTML:"."},document.body);
_1.style(_5,{width:"40px",height:"20px",paddingLeft:"8px",paddingRight:"8px"});
this.button=new _3.form.FileUploader({isDebug:true,uploadUrl:this.uploadUrl,uploadOnChange:true,selectMultipleFiles:false,baseClass:"dojoxEditorUploadNorm",hoverClass:"dojoxEditorUploadHover",activeClass:"dojoxEditorUploadActive",disabledClass:"dojoxEditorUploadDisabled"},_5);
this.connect(this.button,"onChange","insertTempImage");
this.connect(this.button,"onComplete","onComplete");
},onComplete:function(_6,_7,_8){
_6=_6[0];
var _9=_1.withGlobal(this.editor.window,"byId",_1,[this.currentImageId]);
var _a;
if(this.downloadPath){
_a=this.downloadPath+_6.name;
}else{
_a=_6.file;
}
_9.src=_a;
_1.attr(_9,"_djrealurl",_a);
if(_6.width){
_9.width=_6.width;
_9.height=_6.height;
}
},insertTempImage:function(){
this.currentImageId="img_"+(new Date().getTime());
var _b="<img id=\""+this.currentImageId+"\" src=\""+this.tempImageUrl+"\" width=\"32\" height=\"32\"/>";
this.editor.execCommand("inserthtml",_b);
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
