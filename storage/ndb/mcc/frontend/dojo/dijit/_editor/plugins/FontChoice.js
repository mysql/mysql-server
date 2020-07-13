//>>built
define("dijit/_editor/plugins/FontChoice",["require","dojo/_base/array","dojo/_base/declare","dojo/dom-construct","dojo/i18n","dojo/_base/lang","dojo/string","dojo/store/Memory","../../registry","../../_Widget","../../_TemplatedMixin","../../_WidgetsInTemplateMixin","../../form/FilteringSelect","../_Plugin","../range","dojo/i18n!../nls/FontChoice"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
var _10=_3("dijit._editor.plugins._FontDropDown",[_a,_b,_c],{label:"",plainText:false,templateString:"<span style='white-space: nowrap' class='dijit dijitReset dijitInline'>"+"<label class='dijitLeft dijitInline' for='${selectId}'>${label}</label>"+"<input data-dojo-type='../../form/FilteringSelect' required='false' "+"data-dojo-props='labelType:\"html\", labelAttr:\"label\", searchAttr:\"name\"' "+"class='${comboClass}' "+"tabIndex='-1' id='${selectId}' data-dojo-attach-point='select' value=''/>"+"</span>",contextRequire:_1,postMixInProperties:function(){
this.inherited(arguments);
this.strings=_5.getLocalization("dijit._editor","FontChoice");
this.label=this.strings[this.command];
this.id=_9.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.selectId=this.id+"_select";
this.inherited(arguments);
},postCreate:function(){
this.select.set("store",new _8({idProperty:"value",data:_2.map(this.values,function(_11){
var _12=this.strings[_11]||_11;
return {label:this.getLabel(_11,_12),name:_12,value:_11};
},this)}));
this.select.set("value","",false);
this.disabled=this.select.get("disabled");
},_setValueAttr:function(_13,_14){
_14=_14!==false;
this.select.set("value",_2.indexOf(this.values,_13)<0?"":_13,_14);
if(!_14){
this.select._lastValueReported=null;
}
},_getValueAttr:function(){
return this.select.get("value");
},focus:function(){
this.select.focus();
},_setDisabledAttr:function(_15){
this._set("disabled",_15);
this.select.set("disabled",_15);
}});
var _16=_3("dijit._editor.plugins._FontNameDropDown",_10,{generic:false,command:"fontName",comboClass:"dijitFontNameCombo",postMixInProperties:function(){
if(!this.values){
this.values=this.generic?["serif","sans-serif","monospace","cursive","fantasy"]:["Arial","Times New Roman","Comic Sans MS","Courier New"];
}
this.inherited(arguments);
},getLabel:function(_17,_18){
if(this.plainText){
return _18;
}else{
return "<div style='font-family: "+_17+"'>"+_18+"</div>";
}
},_normalizeFontName:function(_19){
var _1a=this.values;
if(!_19||!_1a){
return _19;
}
var _1b=_19.split(",");
if(_1b.length>1){
for(var i=0,l=_1b.length;i<l;i++){
var _1c=_7.trim(_1b[i]);
var pos=_2.indexOf(_1a,_1c);
if(pos>-1){
return _1c;
}
}
}
return _19;
},_setValueAttr:function(_1d,_1e){
_1e=_1e!==false;
_1d=this._normalizeFontName(_1d);
if(this.generic){
var map={"Arial":"sans-serif","Helvetica":"sans-serif","Myriad":"sans-serif","Times":"serif","Times New Roman":"serif","Comic Sans MS":"cursive","Apple Chancery":"cursive","Courier":"monospace","Courier New":"monospace","Papyrus":"fantasy","Estrangelo Edessa":"cursive","Gabriola":"fantasy"};
_1d=map[_1d]||_1d;
}
this.inherited(arguments,[_1d,_1e]);
}});
var _1f=_3("dijit._editor.plugins._FontSizeDropDown",_10,{command:"fontSize",comboClass:"dijitFontSizeCombo",values:[1,2,3,4,5,6,7],getLabel:function(_20,_21){
if(this.plainText){
return _21;
}else{
return "<font size="+_20+"'>"+_21+"</font>";
}
},_setValueAttr:function(_22,_23){
_23=_23!==false;
if(_22.indexOf&&_22.indexOf("px")!=-1){
var _24=parseInt(_22,10);
_22={10:1,13:2,16:3,18:4,24:5,32:6,48:7}[_24]||_22;
}
this.inherited(arguments,[_22,_23]);
}});
var _25=_3("dijit._editor.plugins._FormatBlockDropDown",_10,{command:"formatBlock",comboClass:"dijitFormatBlockCombo",values:["noFormat","p","h1","h2","h3","pre"],postCreate:function(){
this.inherited(arguments);
this.set("value","noFormat",false);
},getLabel:function(_26,_27){
if(this.plainText||_26=="noFormat"){
return _27;
}else{
return "<"+_26+">"+_27+"</"+_26+">";
}
},_execCommand:function(_28,_29,_2a){
if(_2a==="noFormat"){
var _2b;
var end;
var sel=_f.getSelection(_28.window);
if(sel&&sel.rangeCount>0){
var _2c=sel.getRangeAt(0);
var _2d,tag;
if(_2c){
_2b=_2c.startContainer;
end=_2c.endContainer;
while(_2b&&_2b!==_28.editNode&&_2b!==_28.document.body&&_2b.nodeType!==1){
_2b=_2b.parentNode;
}
while(end&&end!==_28.editNode&&end!==_28.document.body&&end.nodeType!==1){
end=end.parentNode;
}
var _2e=_6.hitch(this,function(_2f,ary){
if(_2f.childNodes&&_2f.childNodes.length){
var i;
for(i=0;i<_2f.childNodes.length;i++){
var c=_2f.childNodes[i];
if(c.nodeType==1){
if(_28.selection.inSelection(c)){
var tag=c.tagName?c.tagName.toLowerCase():"";
if(_2.indexOf(this.values,tag)!==-1){
ary.push(c);
}
_2e(c,ary);
}
}
}
}
});
var _30=_6.hitch(this,function(_31){
if(_31&&_31.length){
_28.beginEditing();
while(_31.length){
this._removeFormat(_28,_31.pop());
}
_28.endEditing();
}
});
var _32=[];
if(_2b==end){
var _33;
_2d=_2b;
while(_2d&&_2d!==_28.editNode&&_2d!==_28.document.body){
if(_2d.nodeType==1){
tag=_2d.tagName?_2d.tagName.toLowerCase():"";
if(_2.indexOf(this.values,tag)!==-1){
_33=_2d;
break;
}
}
_2d=_2d.parentNode;
}
_2e(_2b,_32);
if(_33){
_32=[_33].concat(_32);
}
_30(_32);
}else{
_2d=_2b;
while(_28.selection.inSelection(_2d)){
if(_2d.nodeType==1){
tag=_2d.tagName?_2d.tagName.toLowerCase():"";
if(_2.indexOf(this.values,tag)!==-1){
_32.push(_2d);
}
_2e(_2d,_32);
}
_2d=_2d.nextSibling;
}
_30(_32);
}
_28.onDisplayChanged();
}
}
}else{
_28.execCommand(_29,_2a);
}
},_removeFormat:function(_34,_35){
if(_34.customUndo){
while(_35.firstChild){
_4.place(_35.firstChild,_35,"before");
}
_35.parentNode.removeChild(_35);
}else{
_34.selection.selectElementChildren(_35);
var _36=_34.selection.getSelectedHtml();
_34.selection.selectElement(_35);
_34.execCommand("inserthtml",_36||"");
}
}});
var _37=_3("dijit._editor.plugins.FontChoice",_e,{useDefaultCommand:false,_initButton:function(){
var _38={fontName:_16,fontSize:_1f,formatBlock:_25}[this.command],_39=this.params;
if(this.params.custom){
_39.values=this.params.custom;
}
var _3a=this.editor;
this.button=new _38(_6.delegate({dir:_3a.dir,lang:_3a.lang},_39));
this.own(this.button.select.on("change",_6.hitch(this,function(_3b){
if(this.editor.focused){
this.editor.focus();
}
if(this.command=="fontName"&&_3b.indexOf(" ")!=-1){
_3b="'"+_3b+"'";
}
if(this.button._execCommand){
this.button._execCommand(this.editor,this.command,_3b);
}else{
this.editor.execCommand(this.command,_3b);
}
})));
},updateState:function(){
var _3c=this.editor;
var _3d=this.command;
if(!_3c||!_3c.isLoaded||!_3d.length){
return;
}
if(this.button){
var _3e=this.get("disabled");
this.button.set("disabled",_3e);
if(_3e){
return;
}
var _3f;
try{
_3f=_3c.queryCommandValue(_3d)||"";
}
catch(e){
_3f="";
}
var _40=_6.isString(_3f)&&(_3f.match(/'([^']*)'/)||_3f.match(/"([^"]*)"/));
if(_40){
_3f=_40[1];
}
if(_3d==="fontSize"&&!_3f){
_3f=3;
}
if(_3d==="formatBlock"){
if(!_3f||_3f=="p"){
_3f=null;
var _41;
var sel=_f.getSelection(this.editor.window);
if(sel&&sel.rangeCount>0){
var _42=sel.getRangeAt(0);
if(_42){
_41=_42.endContainer;
}
}
while(_41&&_41!==_3c.editNode&&_41!==_3c.document){
var tg=_41.tagName?_41.tagName.toLowerCase():"";
if(tg&&_2.indexOf(this.button.values,tg)>-1){
_3f=tg;
break;
}
_41=_41.parentNode;
}
if(!_3f){
_3f="noFormat";
}
}else{
if(_2.indexOf(this.button.values,_3f)<0){
_3f="noFormat";
}
}
}
if(_3f!==this.button.get("value")){
this.button.set("value",_3f,false);
}
}
}});
_2.forEach(["fontName","fontSize","formatBlock"],function(_43){
_e.registry[_43]=function(_44){
return new _37({command:_43,plainText:_44.plainText});
};
});
_37._FontDropDown=_10;
_37._FontNameDropDown=_16;
_37._FontSizeDropDown=_1f;
_37._FormatBlockDropDown=_25;
return _37;
});
