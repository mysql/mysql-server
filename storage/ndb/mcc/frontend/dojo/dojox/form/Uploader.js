//>>built
require({cache:{"url:dojox/form/resources/Uploader.html":"<span class=\"dijit dijitReset dijitInline\"\n\t><span class=\"dijitReset dijitInline dijitButtonNode\"\n\t\tdojoAttachEvent=\"ondijitclick:_onClick\"\n\t\t><span class=\"dijitReset dijitStretch dijitButtonContents\"\n\t\t\tdojoAttachPoint=\"titleNode,focusNode\"\n\t\t\trole=\"button\" aria-labelledby=\"${id}_label\"\n\t\t\t><span class=\"dijitReset dijitInline dijitIcon\" dojoAttachPoint=\"iconNode\"></span\n\t\t\t><span class=\"dijitReset dijitToggleButtonIconChar\">&#x25CF;</span\n\t\t\t><span class=\"dijitReset dijitInline dijitButtonText\"\n\t\t\t\tid=\"${id}_label\"\n\t\t\t\tdojoAttachPoint=\"containerNode\"\n\t\t\t></span\n\t\t></span\n\t></span\n\t><!--no need to have this for Uploader \n\t<input ${!nameAttrSetting} type=\"${type}\" value=\"${value}\" class=\"dijitOffScreen\" tabIndex=\"-1\"\n\t\tdojoAttachPoint=\"valueNode\"\n/--></span>\n"}});
define("dojox/form/Uploader",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/connect","dojo/_base/window","dojo/dom-style","dojo/dom-geometry","dojo/dom-attr","dojo/dom-construct","dojo/dom-form","dijit","dijit/form/Button","dojox/form/uploader/Base","dojo/i18n!./nls/Uploader","dojo/text!./resources/Uploader.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
_1.experimental("dojox.form.Uploader");
_2("dojox.form.Uploader",[_e,_d],{uploadOnSelect:false,tabIndex:0,multiple:false,label:_f.label,url:"",name:"uploadedfile",flashFieldName:"",uploadType:"form",showInput:"",_nameIndex:0,templateString:_10,baseClass:"dijitUploader "+_d.prototype.baseClass,postMixInProperties:function(){
this._inputs=[];
this._cons=[];
this.inherited(arguments);
},buildRendering:function(){
console.warn("buildRendering",this.id);
this.inherited(arguments);
_7.set(this.domNode,{overflow:"hidden",position:"relative"});
this._buildDisplay();
_9.set(this.titleNode,"tabIndex",-1);
},_buildDisplay:function(){
if(this.showInput){
this.displayInput=dojo.create("input",{"class":"dijitUploadDisplayInput","tabIndex":-1,"autocomplete":"off"},this.containerNode,this.showInput);
this._attachPoints.push("displayInput");
this.connect(this,"onChange",function(_11){
var i=0,l=_11.length,f,r=[];
while((f=_11[i++])){
if(f&&f.name){
r.push(f.name);
}
}
this.displayInput.value=r.join(", ");
});
this.connect(this,"reset",function(){
this.displayInput.value="";
});
}
},startup:function(){
if(this._buildInitialized){
return;
}
this._buildInitialized=true;
this._getButtonStyle(this.domNode);
this._setButtonStyle();
this.inherited(arguments);
},onChange:function(_12){
},onBegin:function(_13){
},onProgress:function(_14){
},onComplete:function(_15){
this.reset();
},onCancel:function(){
},onAbort:function(){
},onError:function(_16){
},upload:function(_17){
},submit:function(_18){
_18=!!_18?_18.tagName?_18:this.getForm():this.getForm();
var _19=_b.toObject(_18);
this.upload(_19);
},reset:function(){
delete this._files;
this._disconnectButton();
_4.forEach(this._inputs,_a.destroy,dojo);
this._inputs=[];
this._nameIndex=0;
this._createInput();
},getFileList:function(){
var _1a=[];
if(this.supports("multiple")){
_4.forEach(this._files,function(f,i){
_1a.push({index:i,name:f.name,size:f.size,type:f.type});
},this);
}else{
_4.forEach(this._inputs,function(n,i){
if(n.value){
_1a.push({index:i,name:n.value.substring(n.value.lastIndexOf("\\")+1),size:0,type:n.value.substring(n.value.lastIndexOf(".")+1)});
}
},this);
}
return _1a;
},_getValueAttr:function(){
return this.getFileList();
},_setValueAttr:function(_1b){
console.error("Uploader value is read only");
},_setDisabledAttr:function(_1c){
if(this._disabled==_1c){
return;
}
this.inherited(arguments);
_7.set(this.inputNode,"display",_1c?"none":"");
},_getButtonStyle:function(_1d){
this.btnSize={w:_7.get(_1d,"width"),h:_7.get(_1d,"height")};
},_setButtonStyle:function(){
this.inputNodeFontSize=Math.max(2,Math.max(Math.ceil(this.btnSize.w/60),Math.ceil(this.btnSize.h/15)));
this._createInput();
},_createInput:function(){
if(this._inputs.length){
_7.set(this.inputNode,{top:"500px"});
this._disconnectButton();
this._nameIndex++;
}
var _1e;
if(this.supports("multiple")){
_1e=this.name+"s[]";
}else{
_1e=this.name+(this.multiple?this._nameIndex:"");
}
this.focusNode=this.inputNode=_a.create("input",{type:"file",name:_1e},this.domNode,"first");
if(this.supports("multiple")&&this.multiple){
_9.set(this.inputNode,"multiple",true);
}
this._inputs.push(this.inputNode);
_7.set(this.inputNode,{position:"absolute",fontSize:this.inputNodeFontSize+"em",top:"-3px",right:"-3px",opacity:0});
this._connectButton();
},_connectButton:function(){
this._cons.push(_5.connect(this.inputNode,"change",this,function(evt){
this._files=this.inputNode.files;
this.onChange(this.getFileList(evt));
if(!this.supports("multiple")&&this.multiple){
this._createInput();
}
}));
if(this.tabIndex>-1){
this.inputNode.tabIndex=this.tabIndex;
this._cons.push(_5.connect(this.inputNode,"focus",this,function(){
this.titleNode.style.outline="1px dashed #ccc";
}));
this._cons.push(_5.connect(this.inputNode,"blur",this,function(){
this.titleNode.style.outline="";
}));
}
},_disconnectButton:function(){
_4.forEach(this._cons,_5.disconnect);
this._cons.splice(0,this._cons.length);
}});
dojox.form.UploaderOrg=dojox.form.Uploader;
var _1f=[dojox.form.UploaderOrg];
dojox.form.addUploaderPlugin=function(_20){
_1f.push(_20);
_2("dojox.form.Uploader",_1f,{});
};
return dojox.form.Uploader;
});
