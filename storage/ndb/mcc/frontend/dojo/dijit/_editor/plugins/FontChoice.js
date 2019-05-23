//>>built
define("dijit/_editor/plugins/FontChoice",["dojo/_base/array","dojo/_base/declare","dojo/dom-construct","dojo/i18n","dojo/_base/lang","dojo/store/Memory","../../registry","../../_Widget","../../_TemplatedMixin","../../_WidgetsInTemplateMixin","../../form/FilteringSelect","../_Plugin","../range","dojo/i18n!../nls/FontChoice"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
var _e=_2("dijit._editor.plugins._FontDropDown",[_8,_9,_a],{label:"",plainText:false,templateString:"<span style='white-space: nowrap' class='dijit dijitReset dijitInline'>"+"<label class='dijitLeft dijitInline' for='${selectId}'>${label}</label>"+"<input data-dojo-type='dijit.form.FilteringSelect' required='false' "+"data-dojo-props='labelType:\"html\", labelAttr:\"label\", searchAttr:\"name\"' "+"class='${comboClass}' "+"tabIndex='-1' id='${selectId}' data-dojo-attach-point='select' value=''/>"+"</span>",postMixInProperties:function(){
this.inherited(arguments);
this.strings=_4.getLocalization("dijit._editor","FontChoice");
this.label=this.strings[this.command];
this.id=_7.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.selectId=this.id+"_select";
this.inherited(arguments);
},postCreate:function(){
this.select.set("store",new _6({idProperty:"value",data:_1.map(this.values,function(_f){
var _10=this.strings[_f]||_f;
return {label:this.getLabel(_f,_10),name:_10,value:_f};
},this)}));
this.select.set("value","",false);
this.disabled=this.select.get("disabled");
},_setValueAttr:function(_11,_12){
_12=_12!==false;
this.select.set("value",_1.indexOf(this.values,_11)<0?"":_11,_12);
if(!_12){
this.select._lastValueReported=null;
}
},_getValueAttr:function(){
return this.select.get("value");
},focus:function(){
this.select.focus();
},_setDisabledAttr:function(_13){
this.disabled=_13;
this.select.set("disabled",_13);
}});
var _14=_2("dijit._editor.plugins._FontNameDropDown",_e,{generic:false,command:"fontName",comboClass:"dijitFontNameCombo",postMixInProperties:function(){
if(!this.values){
this.values=this.generic?["serif","sans-serif","monospace","cursive","fantasy"]:["Arial","Times New Roman","Comic Sans MS","Courier New"];
}
this.inherited(arguments);
},getLabel:function(_15,_16){
if(this.plainText){
return _16;
}else{
return "<div style='font-family: "+_15+"'>"+_16+"</div>";
}
},_setValueAttr:function(_17,_18){
_18=_18!==false;
if(this.generic){
var map={"Arial":"sans-serif","Helvetica":"sans-serif","Myriad":"sans-serif","Times":"serif","Times New Roman":"serif","Comic Sans MS":"cursive","Apple Chancery":"cursive","Courier":"monospace","Courier New":"monospace","Papyrus":"fantasy","Estrangelo Edessa":"cursive","Gabriola":"fantasy"};
_17=map[_17]||_17;
}
this.inherited(arguments,[_17,_18]);
}});
var _19=_2("dijit._editor.plugins._FontSizeDropDown",_e,{command:"fontSize",comboClass:"dijitFontSizeCombo",values:[1,2,3,4,5,6,7],getLabel:function(_1a,_1b){
if(this.plainText){
return _1b;
}else{
return "<font size="+_1a+"'>"+_1b+"</font>";
}
},_setValueAttr:function(_1c,_1d){
_1d=_1d!==false;
if(_1c.indexOf&&_1c.indexOf("px")!=-1){
var _1e=parseInt(_1c,10);
_1c={10:1,13:2,16:3,18:4,24:5,32:6,48:7}[_1e]||_1c;
}
this.inherited(arguments,[_1c,_1d]);
}});
var _1f=_2("dijit._editor.plugins._FormatBlockDropDown",_e,{command:"formatBlock",comboClass:"dijitFormatBlockCombo",values:["noFormat","p","h1","h2","h3","pre"],postCreate:function(){
this.inherited(arguments);
this.set("value","noFormat",false);
},getLabel:function(_20,_21){
if(this.plainText||_20=="noFormat"){
return _21;
}else{
return "<"+_20+">"+_21+"</"+_20+">";
}
},_execCommand:function(_22,_23,_24){
if(_24==="noFormat"){
var _25;
var end;
var sel=_d.getSelection(_22.window);
if(sel&&sel.rangeCount>0){
var _26=sel.getRangeAt(0);
var _27,tag;
if(_26){
_25=_26.startContainer;
end=_26.endContainer;
while(_25&&_25!==_22.editNode&&_25!==_22.document.body&&_25.nodeType!==1){
_25=_25.parentNode;
}
while(end&&end!==_22.editNode&&end!==_22.document.body&&end.nodeType!==1){
end=end.parentNode;
}
var _28=_5.hitch(this,function(_29,ary){
if(_29.childNodes&&_29.childNodes.length){
var i;
for(i=0;i<_29.childNodes.length;i++){
var c=_29.childNodes[i];
if(c.nodeType==1){
if(_22._sCall("inSelection",[c])){
var tag=c.tagName?c.tagName.toLowerCase():"";
if(_1.indexOf(this.values,tag)!==-1){
ary.push(c);
}
_28(c,ary);
}
}
}
}
});
var _2a=_5.hitch(this,function(_2b){
if(_2b&&_2b.length){
_22.beginEditing();
while(_2b.length){
this._removeFormat(_22,_2b.pop());
}
_22.endEditing();
}
});
var _2c=[];
if(_25==end){
var _2d;
_27=_25;
while(_27&&_27!==_22.editNode&&_27!==_22.document.body){
if(_27.nodeType==1){
tag=_27.tagName?_27.tagName.toLowerCase():"";
if(_1.indexOf(this.values,tag)!==-1){
_2d=_27;
break;
}
}
_27=_27.parentNode;
}
_28(_25,_2c);
if(_2d){
_2c=[_2d].concat(_2c);
}
_2a(_2c);
}else{
_27=_25;
while(_22._sCall("inSelection",[_27])){
if(_27.nodeType==1){
tag=_27.tagName?_27.tagName.toLowerCase():"";
if(_1.indexOf(this.values,tag)!==-1){
_2c.push(_27);
}
_28(_27,_2c);
}
_27=_27.nextSibling;
}
_2a(_2c);
}
_22.onDisplayChanged();
}
}
}else{
_22.execCommand(_23,_24);
}
},_removeFormat:function(_2e,_2f){
if(_2e.customUndo){
while(_2f.firstChild){
_3.place(_2f.firstChild,_2f,"before");
}
_2f.parentNode.removeChild(_2f);
}else{
_2e._sCall("selectElementChildren",[_2f]);
var _30=_2e._sCall("getSelectedHtml",[]);
_2e._sCall("selectElement",[_2f]);
_2e.execCommand("inserthtml",_30||"");
}
}});
var _31=_2("dijit._editor.plugins.FontChoice",_c,{useDefaultCommand:false,_initButton:function(){
var _32={fontName:_14,fontSize:_19,formatBlock:_1f}[this.command],_33=this.params;
if(this.params.custom){
_33.values=this.params.custom;
}
var _34=this.editor;
this.button=new _32(_5.delegate({dir:_34.dir,lang:_34.lang},_33));
this.connect(this.button.select,"onChange",function(_35){
if(this.editor.focused){
this.editor.focus();
}
if(this.command=="fontName"&&_35.indexOf(" ")!=-1){
_35="'"+_35+"'";
}
if(this.button._execCommand){
this.button._execCommand(this.editor,this.command,_35);
}else{
this.editor.execCommand(this.command,_35);
}
});
},updateState:function(){
var _36=this.editor;
var _37=this.command;
if(!_36||!_36.isLoaded||!_37.length){
return;
}
if(this.button){
var _38=this.get("disabled");
this.button.set("disabled",_38);
if(_38){
return;
}
var _39;
try{
_39=_36.queryCommandValue(_37)||"";
}
catch(e){
_39="";
}
var _3a=_5.isString(_39)&&(_39.match(/'([^']*)'/)||_39.match(/"([^"]*)"/));
if(_3a){
_39=_3a[1];
}
if(_37==="formatBlock"){
if(!_39||_39=="p"){
_39=null;
var _3b;
var sel=_d.getSelection(this.editor.window);
if(sel&&sel.rangeCount>0){
var _3c=sel.getRangeAt(0);
if(_3c){
_3b=_3c.endContainer;
}
}
while(_3b&&_3b!==_36.editNode&&_3b!==_36.document){
var tg=_3b.tagName?_3b.tagName.toLowerCase():"";
if(tg&&_1.indexOf(this.button.values,tg)>-1){
_39=tg;
break;
}
_3b=_3b.parentNode;
}
if(!_39){
_39="noFormat";
}
}else{
if(_1.indexOf(this.button.values,_39)<0){
_39="noFormat";
}
}
}
if(_39!==this.button.get("value")){
this.button.set("value",_39,false);
}
}
}});
_1.forEach(["fontName","fontSize","formatBlock"],function(_3d){
_c.registry[_3d]=function(_3e){
return new _31({command:_3d,plainText:_3e.plainText});
};
});
_31._FontDropDown=_e;
_31._FontNameDropDown=_14;
_31._FontSizeDropDown=_19;
_31._FormatBlockDropDown=_1f;
return _31;
});
