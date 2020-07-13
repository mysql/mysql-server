//>>built
define("dojox/editor/plugins/Blockquote",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/form/ToggleButton","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/Blockquote"],function(_1,_2,_3,_4){
var _5=_1.declare("dojox.editor.plugins.Blockquote",_4,{iconClassPrefix:"dijitAdditionalEditorIcon",_initButton:function(){
this._nlsResources=_1.i18n.getLocalization("dojox.editor.plugins","Blockquote");
this.button=new _2.form.ToggleButton({label:this._nlsResources["blockquote"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"Blockquote",tabIndex:"-1",onClick:_1.hitch(this,"_toggleQuote")});
},setEditor:function(_6){
this.editor=_6;
this._initButton();
this.connect(this.editor,"onNormalizedDisplayChanged","updateState");
_6.customUndo=true;
},_toggleQuote:function(_7){
try{
var ed=this.editor;
ed.focus();
var _8=this.button.get("checked");
var _9=_2.range.getSelection(ed.window);
var _a,_b,_c,_d;
if(_9&&_9.rangeCount>0){
_a=_9.getRangeAt(0);
}
if(_a){
ed.beginEditing();
if(_8){
var bq,_e;
if(_a.startContainer===_a.endContainer){
if(this._isRootInline(_a.startContainer)){
_c=_a.startContainer;
while(_c&&_c.parentNode!==ed.editNode){
_c=_c.parentNode;
}
while(_c&&_c.previousSibling&&(this._isTextElement(_c)||(_c.nodeType===1&&this._isInlineFormat(this._getTagName(_c))))){
_c=_c.previousSibling;
}
if(_c&&_c.nodeType===1&&!this._isInlineFormat(this._getTagName(_c))){
_c=_c.nextSibling;
}
if(_c){
bq=ed.document.createElement("blockquote");
_1.place(bq,_c,"after");
bq.appendChild(_c);
_d=bq.nextSibling;
while(_d&&(this._isTextElement(_d)||(_d.nodeType===1&&this._isInlineFormat(this._getTagName(_d))))){
bq.appendChild(_d);
_d=bq.nextSibling;
}
}
}else{
var _f=_a.startContainer;
while((this._isTextElement(_f)||this._isInlineFormat(this._getTagName(_f))||this._getTagName(_f)==="li")&&_f!==ed.editNode&&_f!==ed.document.body){
_f=_f.parentNode;
}
if(_f!==ed.editNode&&_f!==_f.ownerDocument.documentElement){
bq=ed.document.createElement("blockquote");
_1.place(bq,_f,"after");
bq.appendChild(_f);
}
}
if(bq){
ed._sCall("selectElementChildren",[bq]);
ed._sCall("collapse",[true]);
}
}else{
var _10;
_c=_a.startContainer;
_d=_a.endContainer;
while(_c&&this._isTextElement(_c)&&_c.parentNode!==ed.editNode){
_c=_c.parentNode;
}
_10=_c;
while(_10.nextSibling&&ed._sCall("inSelection",[_10])){
_10=_10.nextSibling;
}
_d=_10;
if(_d===ed.editNode||_d===ed.document.body){
bq=ed.document.createElement("blockquote");
_1.place(bq,_c,"after");
_e=this._getTagName(_c);
if(this._isTextElement(_c)||this._isInlineFormat(_e)){
var _11=_c;
while(_11&&(this._isTextElement(_11)||(_11.nodeType===1&&this._isInlineFormat(this._getTagName(_11))))){
bq.appendChild(_11);
_11=bq.nextSibling;
}
}else{
bq.appendChild(_c);
}
return;
}
_d=_d.nextSibling;
_10=_c;
while(_10&&_10!==_d){
if(_10.nodeType===1){
_e=this._getTagName(_10);
if(_e!=="br"){
if(!window.getSelection){
if(_e==="p"&&this._isEmpty(_10)){
_10=_10.nextSibling;
continue;
}
}
if(this._isInlineFormat(_e)){
if(!bq){
bq=ed.document.createElement("blockquote");
_1.place(bq,_10,"after");
bq.appendChild(_10);
}else{
bq.appendChild(_10);
}
_10=bq;
}else{
if(bq){
if(this._isEmpty(bq)){
bq.parentNode.removeChild(bq);
}
}
bq=ed.document.createElement("blockquote");
_1.place(bq,_10,"after");
bq.appendChild(_10);
_10=bq;
}
}
}else{
if(this._isTextElement(_10)){
if(!bq){
bq=ed.document.createElement("blockquote");
_1.place(bq,_10,"after");
bq.appendChild(_10);
}else{
bq.appendChild(_10);
}
_10=bq;
}
}
_10=_10.nextSibling;
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
var _12=false;
if(_a.startContainer===_a.endContainer){
_b=_a.endContainer;
while(_b&&_b!==ed.editNode&&_b!==ed.document.body){
var tg=_b.tagName?_b.tagName.toLowerCase():"";
if(tg==="blockquote"){
_12=true;
break;
}
_b=_b.parentNode;
}
if(_12){
var _13;
while(_b.firstChild){
_13=_b.firstChild;
_1.place(_13,_b,"before");
}
_b.parentNode.removeChild(_b);
if(_13){
ed._sCall("selectElementChildren",[_13]);
ed._sCall("collapse",[true]);
}
}
}else{
_c=_a.startContainer;
_d=_a.endContainer;
while(_c&&this._isTextElement(_c)&&_c.parentNode!==ed.editNode){
_c=_c.parentNode;
}
var _14=[];
var _15=_c;
while(_15&&_15.nextSibling&&ed._sCall("inSelection",[_15])){
if(_15.parentNode&&this._getTagName(_15.parentNode)==="blockquote"){
_15=_15.parentNode;
}
_14.push(_15);
_15=_15.nextSibling;
}
var _16=this._findBlockQuotes(_14);
while(_16.length){
var bn=_16.pop();
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
var _17=this.get("disabled");
if(!ed||!ed.isLoaded){
return;
}
if(this.button){
this.button.set("disabled",_17);
if(_17){
return;
}
var _18;
var _19=false;
var sel=_2.range.getSelection(ed.window);
if(sel&&sel.rangeCount>0){
var _1a=sel.getRangeAt(0);
if(_1a){
_18=_1a.endContainer;
}
}
while(_18&&_18!==ed.editNode&&_18!==ed.document){
var tg=_18.tagName?_18.tagName.toLowerCase():"";
if(tg==="blockquote"){
_19=true;
break;
}
_18=_18.parentNode;
}
this.button.set("checked",_19);
}
},_findBlockQuotes:function(_1b){
var _1c=[];
if(_1b){
var i;
for(i=0;i<_1b.length;i++){
var _1d=_1b[i];
if(_1d.nodeType===1){
if(this._getTagName(_1d)==="blockquote"){
_1c.push(_1d);
}
if(_1d.childNodes&&_1d.childNodes.length>0){
_1c=_1c.concat(this._findBlockQuotes(_1d.childNodes));
}
}
}
}
return _1c;
},_getTagName:function(_1e){
var tag="";
if(_1e&&_1e.nodeType===1){
tag=_1e.tagName?_1e.tagName.toLowerCase():"";
}
return tag;
},_isRootInline:function(_1f){
var ed=this.editor;
if(this._isTextElement(_1f)&&_1f.parentNode===ed.editNode){
return true;
}else{
if(_1f.nodeType===1&&this._isInlineFormat(_1f)&&_1f.parentNode===ed.editNode){
return true;
}else{
if(this._isTextElement(_1f)&&this._isInlineFormat(this._getTagName(_1f.parentNode))){
_1f=_1f.parentNode;
while(_1f&&_1f!==ed.editNode&&this._isInlineFormat(this._getTagName(_1f))){
_1f=_1f.parentNode;
}
if(_1f===ed.editNode){
return true;
}
}
}
}
return false;
},_isTextElement:function(_20){
if(_20&&_20.nodeType===3||_20.nodeType===4){
return true;
}
return false;
},_isEmpty:function(_21){
if(_21.childNodes){
var _22=true;
var i;
for(i=0;i<_21.childNodes.length;i++){
var n=_21.childNodes[i];
if(n.nodeType===1){
if(this._getTagName(n)==="p"){
if(!_1.trim(n.innerHTML)){
continue;
}
}
_22=false;
break;
}else{
if(this._isTextElement(n)){
var nv=_1.trim(n.nodeValue);
if(nv&&nv!=="&nbsp;"&&nv!==" "){
_22=false;
break;
}
}else{
_22=false;
break;
}
}
}
return _22;
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
var _23=o.args.name.toLowerCase();
if(_23==="blockquote"){
o.plugin=new _5({});
}
});
return _5;
});
