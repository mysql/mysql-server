//>>built
define("dojox/editor/plugins/Blockquote",["dojo","dijit","dojox","dijit/_editor/range","dijit/_editor/selection","dijit/_editor/_Plugin","dijit/form/ToggleButton","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/Blockquote"],function(_1,_2,_3,_4,_5,_6){
_1.declare("dojox.editor.plugins.Blockquote",_6,{iconClassPrefix:"dijitAdditionalEditorIcon",_initButton:function(){
this._nlsResources=_1.i18n.getLocalization("dojox.editor.plugins","Blockquote");
this.button=new _2.form.ToggleButton({label:this._nlsResources["blockquote"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"Blockquote",tabIndex:"-1",onClick:_1.hitch(this,"_toggleQuote")});
},setEditor:function(_7){
this.editor=_7;
this._initButton();
this.connect(this.editor,"onNormalizedDisplayChanged","updateState");
_7.customUndo=true;
},_toggleQuote:function(_8){
try{
var ed=this.editor;
ed.focus();
var _9=this.button.get("checked");
var _a=_2.range.getSelection(ed.window);
var _b,_c,_d,_e;
if(_a&&_a.rangeCount>0){
_b=_a.getRangeAt(0);
}
if(_b){
ed.beginEditing();
if(_9){
var bq,_f;
if(_b.startContainer===_b.endContainer){
if(this._isRootInline(_b.startContainer)){
_d=_b.startContainer;
while(_d&&_d.parentNode!==ed.editNode){
_d=_d.parentNode;
}
while(_d&&_d.previousSibling&&(this._isTextElement(_d)||(_d.nodeType===1&&this._isInlineFormat(this._getTagName(_d))))){
_d=_d.previousSibling;
}
if(_d&&_d.nodeType===1&&!this._isInlineFormat(this._getTagName(_d))){
_d=_d.nextSibling;
}
if(_d){
bq=ed.document.createElement("blockquote");
_1.place(bq,_d,"after");
bq.appendChild(_d);
_e=bq.nextSibling;
while(_e&&(this._isTextElement(_e)||(_e.nodeType===1&&this._isInlineFormat(this._getTagName(_e))))){
bq.appendChild(_e);
_e=bq.nextSibling;
}
}
}else{
var _10=_b.startContainer;
while((this._isTextElement(_10)||this._isInlineFormat(this._getTagName(_10))||this._getTagName(_10)==="li")&&_10!==ed.editNode&&_10!==ed.document.body){
_10=_10.parentNode;
}
if(_10!==ed.editNode&&_10!==_10.ownerDocument.documentElement){
bq=ed.document.createElement("blockquote");
_1.place(bq,_10,"after");
bq.appendChild(_10);
}
}
if(bq){
ed._sCall("selectElementChildren",[bq]);
ed._sCall("collapse",[true]);
}
}else{
var _11;
_d=_b.startContainer;
_e=_b.endContainer;
while(_d&&this._isTextElement(_d)&&_d.parentNode!==ed.editNode){
_d=_d.parentNode;
}
_11=_d;
while(_11.nextSibling&&ed._sCall("inSelection",[_11])){
_11=_11.nextSibling;
}
_e=_11;
if(_e===ed.editNode||_e===ed.document.body){
bq=ed.document.createElement("blockquote");
_1.place(bq,_d,"after");
_f=this._getTagName(_d);
if(this._isTextElement(_d)||this._isInlineFormat(_f)){
var _12=_d;
while(_12&&(this._isTextElement(_12)||(_12.nodeType===1&&this._isInlineFormat(this._getTagName(_12))))){
bq.appendChild(_12);
_12=bq.nextSibling;
}
}else{
bq.appendChild(_d);
}
return;
}
_e=_e.nextSibling;
_11=_d;
while(_11&&_11!==_e){
if(_11.nodeType===1){
_f=this._getTagName(_11);
if(_f!=="br"){
if(!window.getSelection){
if(_f==="p"&&this._isEmpty(_11)){
_11=_11.nextSibling;
continue;
}
}
if(this._isInlineFormat(_f)){
if(!bq){
bq=ed.document.createElement("blockquote");
_1.place(bq,_11,"after");
bq.appendChild(_11);
}else{
bq.appendChild(_11);
}
_11=bq;
}else{
if(bq){
if(this._isEmpty(bq)){
bq.parentNode.removeChild(bq);
}
}
bq=ed.document.createElement("blockquote");
_1.place(bq,_11,"after");
bq.appendChild(_11);
_11=bq;
}
}
}else{
if(this._isTextElement(_11)){
if(!bq){
bq=ed.document.createElement("blockquote");
_1.place(bq,_11,"after");
bq.appendChild(_11);
}else{
bq.appendChild(_11);
}
_11=bq;
}
}
_11=_11.nextSibling;
}
if(bq){
if(this._isEmpty(bq)){
bq.parentNode.removeChild(bq);
}else{
ed._sCall("selectElementChildren",[bq]);
ed._sCall("collapse",[true]);
}
bq=null;
}
}
}else{
var _13=false;
if(_b.startContainer===_b.endContainer){
_c=_b.endContainer;
while(_c&&_c!==ed.editNode&&_c!==ed.document.body){
var tg=_c.tagName?_c.tagName.toLowerCase():"";
if(tg==="blockquote"){
_13=true;
break;
}
_c=_c.parentNode;
}
if(_13){
var _14;
while(_c.firstChild){
_14=_c.firstChild;
_1.place(_14,_c,"before");
}
_c.parentNode.removeChild(_c);
if(_14){
ed._sCall("selectElementChildren",[_14]);
ed._sCall("collapse",[true]);
}
}
}else{
_d=_b.startContainer;
_e=_b.endContainer;
while(_d&&this._isTextElement(_d)&&_d.parentNode!==ed.editNode){
_d=_d.parentNode;
}
var _15=[];
var _16=_d;
while(_16&&_16.nextSibling&&ed._sCall("inSelection",[_16])){
if(_16.parentNode&&this._getTagName(_16.parentNode)==="blockquote"){
_16=_16.parentNode;
}
_15.push(_16);
_16=_16.nextSibling;
}
var _17=this._findBlockQuotes(_15);
while(_17.length){
var bn=_17.pop();
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
var _18=this.get("disabled");
if(!ed||!ed.isLoaded){
return;
}
if(this.button){
this.button.set("disabled",_18);
if(_18){
return;
}
var _19;
var _1a=false;
var sel=_2.range.getSelection(ed.window);
if(sel&&sel.rangeCount>0){
var _1b=sel.getRangeAt(0);
if(_1b){
_19=_1b.endContainer;
}
}
while(_19&&_19!==ed.editNode&&_19!==ed.document){
var tg=_19.tagName?_19.tagName.toLowerCase():"";
if(tg==="blockquote"){
_1a=true;
break;
}
_19=_19.parentNode;
}
this.button.set("checked",_1a);
}
},_findBlockQuotes:function(_1c){
var _1d=[];
if(_1c){
var i;
for(i=0;i<_1c.length;i++){
var _1e=_1c[i];
if(_1e.nodeType===1){
if(this._getTagName(_1e)==="blockquote"){
_1d.push(_1e);
}
if(_1e.childNodes&&_1e.childNodes.length>0){
_1d=_1d.concat(this._findBlockQuotes(_1e.childNodes));
}
}
}
}
return _1d;
},_getTagName:function(_1f){
var tag="";
if(_1f&&_1f.nodeType===1){
tag=_1f.tagName?_1f.tagName.toLowerCase():"";
}
return tag;
},_isRootInline:function(_20){
var ed=this.editor;
if(this._isTextElement(_20)&&_20.parentNode===ed.editNode){
return true;
}else{
if(_20.nodeType===1&&this._isInlineFormat(_20)&&_20.parentNode===ed.editNode){
return true;
}else{
if(this._isTextElement(_20)&&this._isInlineFormat(this._getTagName(_20.parentNode))){
_20=_20.parentNode;
while(_20&&_20!==ed.editNode&&this._isInlineFormat(this._getTagName(_20))){
_20=_20.parentNode;
}
if(_20===ed.editNode){
return true;
}
}
}
}
return false;
},_isTextElement:function(_21){
if(_21&&_21.nodeType===3||_21.nodeType===4){
return true;
}
return false;
},_isEmpty:function(_22){
if(_22.childNodes){
var _23=true;
var i;
for(i=0;i<_22.childNodes.length;i++){
var n=_22.childNodes[i];
if(n.nodeType===1){
if(this._getTagName(n)==="p"){
if(!_1.trim(n.innerHTML)){
continue;
}
}
_23=false;
break;
}else{
if(this._isTextElement(n)){
var nv=_1.trim(n.nodeValue);
if(nv&&nv!=="&nbsp;"&&nv!==" "){
_23=false;
break;
}
}else{
_23=false;
break;
}
}
}
return _23;
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
var _24=o.args.name.toLowerCase();
if(_24==="blockquote"){
o.plugin=new _3.editor.plugins.Blockquote({});
}
});
return _3.editor.plugins.Blockquote;
});
