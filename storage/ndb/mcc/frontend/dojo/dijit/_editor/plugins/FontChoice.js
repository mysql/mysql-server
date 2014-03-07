//>>built
define("dijit/_editor/plugins/FontChoice",["dojo/_base/array","dojo/_base/declare","dojo/dom-construct","dojo/i18n","dojo/_base/lang","dojo/store/Memory","dojo/_base/window","../../registry","../../_Widget","../../_TemplatedMixin","../../_WidgetsInTemplateMixin","../../form/FilteringSelect","../_Plugin","../range","../selection","dojo/i18n!../nls/FontChoice"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
var _10=_2("dijit._editor.plugins._FontDropDown",[_9,_a,_b],{label:"",plainText:false,templateString:"<span style='white-space: nowrap' class='dijit dijitReset dijitInline'>"+"<label class='dijitLeft dijitInline' for='${selectId}'>${label}</label>"+"<input data-dojo-type='dijit.form.FilteringSelect' required='false' "+"data-dojo-props='labelType:\"html\", labelAttr:\"label\", searchAttr:\"name\"' "+"tabIndex='-1' id='${selectId}' data-dojo-attach-point='select' value=''/>"+"</span>",postMixInProperties:function(){
this.inherited(arguments);
this.strings=_4.getLocalization("dijit._editor","FontChoice");
this.label=this.strings[this.command];
this.id=_8.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.selectId=this.id+"_select";
this.inherited(arguments);
},postCreate:function(){
this.select.set("store",new _6({idProperty:"value",data:_1.map(this.values,function(_11){
var _12=this.strings[_11]||_11;
return {label:this.getLabel(_11,_12),name:_12,value:_11};
},this)}));
this.select.set("value","",false);
this.disabled=this.select.get("disabled");
},_setValueAttr:function(_13,_14){
_14=_14!==false;
this.select.set("value",_1.indexOf(this.values,_13)<0?"":_13,_14);
if(!_14){
this.select._lastValueReported=null;
}
},_getValueAttr:function(){
return this.select.get("value");
},focus:function(){
this.select.focus();
},_setDisabledAttr:function(_15){
this.disabled=_15;
this.select.set("disabled",_15);
}});
var _16=_2("dijit._editor.plugins._FontNameDropDown",_10,{generic:false,command:"fontName",postMixInProperties:function(){
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
},_setValueAttr:function(_19,_1a){
_1a=_1a!==false;
if(this.generic){
var map={"Arial":"sans-serif","Helvetica":"sans-serif","Myriad":"sans-serif","Times":"serif","Times New Roman":"serif","Comic Sans MS":"cursive","Apple Chancery":"cursive","Courier":"monospace","Courier New":"monospace","Papyrus":"fantasy","Estrangelo Edessa":"cursive","Gabriola":"fantasy"};
_19=map[_19]||_19;
}
this.inherited(arguments,[_19,_1a]);
}});
var _1b=_2("dijit._editor.plugins._FontSizeDropDown",_10,{command:"fontSize",values:[1,2,3,4,5,6,7],getLabel:function(_1c,_1d){
if(this.plainText){
return _1d;
}else{
return "<font size="+_1c+"'>"+_1d+"</font>";
}
},_setValueAttr:function(_1e,_1f){
_1f=_1f!==false;
if(_1e.indexOf&&_1e.indexOf("px")!=-1){
var _20=parseInt(_1e,10);
_1e={10:1,13:2,16:3,18:4,24:5,32:6,48:7}[_20]||_1e;
}
this.inherited(arguments,[_1e,_1f]);
}});
var _21=_2("dijit._editor.plugins._FormatBlockDropDown",_10,{command:"formatBlock",values:["noFormat","p","h1","h2","h3","pre"],postCreate:function(){
this.inherited(arguments);
this.set("value","noFormat",false);
},getLabel:function(_22,_23){
if(this.plainText||_22=="noFormat"){
return _23;
}else{
return "<"+_22+">"+_23+"</"+_22+">";
}
},_execCommand:function(_24,_25,_26){
if(_26==="noFormat"){
var _27;
var end;
var sel=_e.getSelection(_24.window);
if(sel&&sel.rangeCount>0){
var _28=sel.getRangeAt(0);
var _29,tag;
if(_28){
_27=_28.startContainer;
end=_28.endContainer;
while(_27&&_27!==_24.editNode&&_27!==_24.document.body&&_27.nodeType!==1){
_27=_27.parentNode;
}
while(end&&end!==_24.editNode&&end!==_24.document.body&&end.nodeType!==1){
end=end.parentNode;
}
var _2a=_5.hitch(this,function(_2b,ary){
if(_2b.childNodes&&_2b.childNodes.length){
var i;
for(i=0;i<_2b.childNodes.length;i++){
var c=_2b.childNodes[i];
if(c.nodeType==1){
if(_7.withGlobal(_24.window,"inSelection",_f,[c])){
var tag=c.tagName?c.tagName.toLowerCase():"";
if(_1.indexOf(this.values,tag)!==-1){
ary.push(c);
}
_2a(c,ary);
}
}
}
}
});
var _2c=_5.hitch(this,function(_2d){
if(_2d&&_2d.length){
_24.beginEditing();
while(_2d.length){
this._removeFormat(_24,_2d.pop());
}
_24.endEditing();
}
});
var _2e=[];
if(_27==end){
var _2f;
_29=_27;
while(_29&&_29!==_24.editNode&&_29!==_24.document.body){
if(_29.nodeType==1){
tag=_29.tagName?_29.tagName.toLowerCase():"";
if(_1.indexOf(this.values,tag)!==-1){
_2f=_29;
break;
}
}
_29=_29.parentNode;
}
_2a(_27,_2e);
if(_2f){
_2e=[_2f].concat(_2e);
}
_2c(_2e);
}else{
_29=_27;
while(_7.withGlobal(_24.window,"inSelection",_f,[_29])){
if(_29.nodeType==1){
tag=_29.tagName?_29.tagName.toLowerCase():"";
if(_1.indexOf(this.values,tag)!==-1){
_2e.push(_29);
}
_2a(_29,_2e);
}
_29=_29.nextSibling;
}
_2c(_2e);
}
_24.onDisplayChanged();
}
}
}else{
_24.execCommand(_25,_26);
}
},_removeFormat:function(_30,_31){
if(_30.customUndo){
while(_31.firstChild){
_3.place(_31.firstChild,_31,"before");
}
_31.parentNode.removeChild(_31);
}else{
_7.withGlobal(_30.window,"selectElementChildren",_f,[_31]);
var _32=_7.withGlobal(_30.window,"getSelectedHtml",_f,[null]);
_7.withGlobal(_30.window,"selectElement",_f,[_31]);
_30.execCommand("inserthtml",_32||"");
}
}});
var _33=_2("dijit._editor.plugins.FontChoice",_d,{useDefaultCommand:false,_initButton:function(){
var _34={fontName:_16,fontSize:_1b,formatBlock:_21}[this.command],_35=this.params;
if(this.params.custom){
_35.values=this.params.custom;
}
var _36=this.editor;
this.button=new _34(_5.delegate({dir:_36.dir,lang:_36.lang},_35));
this.connect(this.button.select,"onChange",function(_37){
this.editor.focus();
if(this.command=="fontName"&&_37.indexOf(" ")!=-1){
_37="'"+_37+"'";
}
if(this.button._execCommand){
this.button._execCommand(this.editor,this.command,_37);
}else{
this.editor.execCommand(this.command,_37);
}
});
},updateState:function(){
var _38=this.editor;
var _39=this.command;
if(!_38||!_38.isLoaded||!_39.length){
return;
}
if(this.button){
var _3a=this.get("disabled");
this.button.set("disabled",_3a);
if(_3a){
return;
}
var _3b;
try{
_3b=_38.queryCommandValue(_39)||"";
}
catch(e){
_3b="";
}
var _3c=_5.isString(_3b)&&_3b.match(/'([^']*)'/);
if(_3c){
_3b=_3c[1];
}
if(_39==="formatBlock"){
if(!_3b||_3b=="p"){
_3b=null;
var _3d;
var sel=_e.getSelection(this.editor.window);
if(sel&&sel.rangeCount>0){
var _3e=sel.getRangeAt(0);
if(_3e){
_3d=_3e.endContainer;
}
}
while(_3d&&_3d!==_38.editNode&&_3d!==_38.document){
var tg=_3d.tagName?_3d.tagName.toLowerCase():"";
if(tg&&_1.indexOf(this.button.values,tg)>-1){
_3b=tg;
break;
}
_3d=_3d.parentNode;
}
if(!_3b){
_3b="noFormat";
}
}else{
if(_1.indexOf(this.button.values,_3b)<0){
_3b="noFormat";
}
}
}
if(_3b!==this.button.get("value")){
this.button.set("value",_3b,false);
}
}
}});
_1.forEach(["fontName","fontSize","formatBlock"],function(_3f){
_d.registry[_3f]=function(_40){
return new _33({command:_3f,plainText:_40.plainText});
};
});
});
