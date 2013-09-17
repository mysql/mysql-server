//>>built
define("dojox/editor/plugins/Blockquote",["dojo","dijit","dojox","dijit/_editor/range","dijit/_editor/selection","dijit/_editor/_Plugin","dijit/form/ToggleButton","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/Blockquote"],function(_1,_2,_3){
_1.declare("dojox.editor.plugins.Blockquote",_2._editor._Plugin,{iconClassPrefix:"dijitAdditionalEditorIcon",_initButton:function(){
this._nlsResources=_1.i18n.getLocalization("dojox.editor.plugins","Blockquote");
this.button=new _2.form.ToggleButton({label:this._nlsResources["blockquote"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"Blockquote",tabIndex:"-1",onClick:_1.hitch(this,"_toggleQuote")});
},setEditor:function(_4){
this.editor=_4;
this._initButton();
this.connect(this.editor,"onNormalizedDisplayChanged","updateState");
_4.customUndo=true;
},_toggleQuote:function(_5){
try{
var ed=this.editor;
ed.focus();
var _6=this.button.get("checked");
var _7=_2.range.getSelection(ed.window);
var _8,_9,_a,_b;
if(_7&&_7.rangeCount>0){
_8=_7.getRangeAt(0);
}
if(_8){
ed.beginEditing();
if(_6){
var bq,_c;
if(_8.startContainer===_8.endContainer){
if(this._isRootInline(_8.startContainer)){
_a=_8.startContainer;
while(_a&&_a.parentNode!==ed.editNode){
_a=_a.parentNode;
}
while(_a&&_a.previousSibling&&(this._isTextElement(_a)||(_a.nodeType===1&&this._isInlineFormat(this._getTagName(_a))))){
_a=_a.previousSibling;
}
if(_a&&_a.nodeType===1&&!this._isInlineFormat(this._getTagName(_a))){
_a=_a.nextSibling;
}
if(_a){
bq=ed.document.createElement("blockquote");
_1.place(bq,_a,"after");
bq.appendChild(_a);
_b=bq.nextSibling;
while(_b&&(this._isTextElement(_b)||(_b.nodeType===1&&this._isInlineFormat(this._getTagName(_b))))){
bq.appendChild(_b);
_b=bq.nextSibling;
}
}
}else{
var _d=_8.startContainer;
while((this._isTextElement(_d)||this._isInlineFormat(this._getTagName(_d))||this._getTagName(_d)==="li")&&_d!==ed.editNode&&_d!==ed.document.body){
_d=_d.parentNode;
}
if(_d!==ed.editNode&&_d!==_d.ownerDocument.documentElement){
bq=ed.document.createElement("blockquote");
_1.place(bq,_d,"after");
bq.appendChild(_d);
}
}
if(bq){
_1.withGlobal(ed.window,"selectElementChildren",_2._editor.selection,[bq]);
_1.withGlobal(ed.window,"collapse",_2._editor.selection,[true]);
}
}else{
var _e;
_a=_8.startContainer;
_b=_8.endContainer;
while(_a&&this._isTextElement(_a)&&_a.parentNode!==ed.editNode){
_a=_a.parentNode;
}
_e=_a;
while(_e.nextSibling&&_1.withGlobal(ed.window,"inSelection",_2._editor.selection,[_e])){
_e=_e.nextSibling;
}
_b=_e;
if(_b===ed.editNode||_b===ed.document.body){
bq=ed.document.createElement("blockquote");
_1.place(bq,_a,"after");
_c=this._getTagName(_a);
if(this._isTextElement(_a)||this._isInlineFormat(_c)){
var _f=_a;
while(_f&&(this._isTextElement(_f)||(_f.nodeType===1&&this._isInlineFormat(this._getTagName(_f))))){
bq.appendChild(_f);
_f=bq.nextSibling;
}
}else{
bq.appendChild(_a);
}
return;
}
_b=_b.nextSibling;
_e=_a;
while(_e&&_e!==_b){
if(_e.nodeType===1){
_c=this._getTagName(_e);
if(_c!=="br"){
if(!window.getSelection){
if(_c==="p"&&this._isEmpty(_e)){
_e=_e.nextSibling;
continue;
}
}
if(this._isInlineFormat(_c)){
if(!bq){
bq=ed.document.createElement("blockquote");
_1.place(bq,_e,"after");
bq.appendChild(_e);
}else{
bq.appendChild(_e);
}
_e=bq;
}else{
if(bq){
if(this._isEmpty(bq)){
bq.parentNode.removeChild(bq);
}
}
bq=ed.document.createElement("blockquote");
_1.place(bq,_e,"after");
bq.appendChild(_e);
_e=bq;
}
}
}else{
if(this._isTextElement(_e)){
if(!bq){
bq=ed.document.createElement("blockquote");
_1.place(bq,_e,"after");
bq.appendChild(_e);
}else{
bq.appendChild(_e);
}
_e=bq;
}
}
_e=_e.nextSibling;
}
if(bq){
if(this._isEmpty(bq)){
bq.parentNode.removeChild(bq);
}else{
_1.withGlobal(ed.window,"selectElementChildren",_2._editor.selection,[bq]);
_1.withGlobal(ed.window,"collapse",_2._editor.selection,[true]);
}
bq=null;
}
}
}else{
var _10=false;
if(_8.startContainer===_8.endContainer){
_9=_8.endContainer;
while(_9&&_9!==ed.editNode&&_9!==ed.document.body){
var tg=_9.tagName?_9.tagName.toLowerCase():"";
if(tg==="blockquote"){
_10=true;
break;
}
_9=_9.parentNode;
}
if(_10){
var _11;
while(_9.firstChild){
_11=_9.firstChild;
_1.place(_11,_9,"before");
}
_9.parentNode.removeChild(_9);
if(_11){
_1.withGlobal(ed.window,"selectElementChildren",_2._editor.selection,[_11]);
_1.withGlobal(ed.window,"collapse",_2._editor.selection,[true]);
}
}
}else{
_a=_8.startContainer;
_b=_8.endContainer;
while(_a&&this._isTextElement(_a)&&_a.parentNode!==ed.editNode){
_a=_a.parentNode;
}
var _12=[];
var _13=_a;
while(_13&&_13.nextSibling&&_1.withGlobal(ed.window,"inSelection",_2._editor.selection,[_13])){
if(_13.parentNode&&this._getTagName(_13.parentNode)==="blockquote"){
_13=_13.parentNode;
}
_12.push(_13);
_13=_13.nextSibling;
}
var _14=this._findBlockQuotes(_12);
while(_14.length){
var bn=_14.pop();
if(bn.parentNode){
while(bn.firstChild){
_1.place(bn.firstChild,bn,"before");
}
bn.parentNode.removeChild(bn);
}
}
}
}
ed.endEditing();
}
ed.onNormalizedDisplayChanged();
}
catch(e){
}
},updateState:function(){
var ed=this.editor;
var _15=this.get("disabled");
if(!ed||!ed.isLoaded){
return;
}
if(this.button){
this.button.set("disabled",_15);
if(_15){
return;
}
var _16;
var _17=false;
var sel=_2.range.getSelection(ed.window);
if(sel&&sel.rangeCount>0){
var _18=sel.getRangeAt(0);
if(_18){
_16=_18.endContainer;
}
}
while(_16&&_16!==ed.editNode&&_16!==ed.document){
var tg=_16.tagName?_16.tagName.toLowerCase():"";
if(tg==="blockquote"){
_17=true;
break;
}
_16=_16.parentNode;
}
this.button.set("checked",_17);
}
},_findBlockQuotes:function(_19){
var _1a=[];
if(_19){
var i;
for(i=0;i<_19.length;i++){
var _1b=_19[i];
if(_1b.nodeType===1){
if(this._getTagName(_1b)==="blockquote"){
_1a.push(_1b);
}
if(_1b.childNodes&&_1b.childNodes.length>0){
_1a=_1a.concat(this._findBlockQuotes(_1b.childNodes));
}
}
}
}
return _1a;
},_getTagName:function(_1c){
var tag="";
if(_1c&&_1c.nodeType===1){
tag=_1c.tagName?_1c.tagName.toLowerCase():"";
}
return tag;
},_isRootInline:function(_1d){
var ed=this.editor;
if(this._isTextElement(_1d)&&_1d.parentNode===ed.editNode){
return true;
}else{
if(_1d.nodeType===1&&this._isInlineFormat(_1d)&&_1d.parentNode===ed.editNode){
return true;
}else{
if(this._isTextElement(_1d)&&this._isInlineFormat(this._getTagName(_1d.parentNode))){
_1d=_1d.parentNode;
while(_1d&&_1d!==ed.editNode&&this._isInlineFormat(this._getTagName(_1d))){
_1d=_1d.parentNode;
}
if(_1d===ed.editNode){
return true;
}
}
}
}
return false;
},_isTextElement:function(_1e){
if(_1e&&_1e.nodeType===3||_1e.nodeType===4){
return true;
}
return false;
},_isEmpty:function(_1f){
if(_1f.childNodes){
var _20=true;
var i;
for(i=0;i<_1f.childNodes.length;i++){
var n=_1f.childNodes[i];
if(n.nodeType===1){
if(this._getTagName(n)==="p"){
if(!_1.trim(n.innerHTML)){
continue;
}
}
_20=false;
break;
}else{
if(this._isTextElement(n)){
var nv=_1.trim(n.nodeValue);
if(nv&&nv!=="&nbsp;"&&nv!==" "){
_20=false;
break;
}
}else{
_20=false;
break;
}
}
}
return _20;
}else{
return true;
}
},_isInlineFormat:function(tag){
switch(tag){
case "a":
case "b":
case "strong":
case "s":
case "strike":
case "i":
case "u":
case "em":
case "sup":
case "sub":
case "span":
case "font":
case "big":
case "cite":
case "q":
case "img":
case "small":
return true;
default:
return false;
}
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _21=o.args.name.toLowerCase();
if(_21==="blockquote"){
o.plugin=new _3.editor.plugins.Blockquote({});
}
});
return _3.editor.plugins.Blockquote;
});
