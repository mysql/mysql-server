//>>built
define("dijit/_editor/plugins/EnterKeyHandling",["dojo/_base/declare","dojo/dom-construct","dojo/keys","dojo/_base/lang","dojo/on","dojo/sniff","dojo/_base/window","dojo/window","../_Plugin","../RichText","../range"],function(_1,_2,_3,_4,on,_5,_6,_7,_8,_9,_a){
return _1("dijit._editor.plugins.EnterKeyHandling",_8,{blockNodeForEnter:"BR",constructor:function(_b){
if(_b){
if("blockNodeForEnter" in _b){
_b.blockNodeForEnter=_b.blockNodeForEnter.toUpperCase();
}
_4.mixin(this,_b);
}
},setEditor:function(_c){
if(this.editor===_c){
return;
}
this.editor=_c;
if(this.blockNodeForEnter=="BR"){
this.editor.customUndo=true;
_c.onLoadDeferred.then(_4.hitch(this,function(d){
this.own(on(_c.document,"keydown",_4.hitch(this,function(e){
if(e.keyCode==_3.ENTER){
var ne=_4.mixin({},e);
ne.shiftKey=true;
if(!this.handleEnterKey(ne)){
e.stopPropagation();
e.preventDefault();
}
}
})));
if(_5("ie")>=9&&_5("ie")<=10){
this.own(on(_c.document,"paste",_4.hitch(this,function(e){
setTimeout(_4.hitch(this,function(){
var r=this.editor.document.selection.createRange();
r.move("character",-1);
r.select();
r.move("character",1);
r.select();
}),0);
})));
}
return d;
}));
}else{
if(this.blockNodeForEnter){
var h=_4.hitch(this,"handleEnterKey");
_c.addKeyHandler(13,0,0,h);
_c.addKeyHandler(13,0,1,h);
this.own(this.editor.on("KeyPressed",_4.hitch(this,"onKeyPressed")));
}
}
},onKeyPressed:function(){
if(this._checkListLater){
if(this.editor.selection.isCollapsed()){
var _d=this.editor.selection.getAncestorElement("LI");
if(!_d){
_9.prototype.execCommand.call(this.editor,"formatblock",this.blockNodeForEnter);
var _e=this.editor.selection.getAncestorElement(this.blockNodeForEnter);
if(_e){
_e.innerHTML=this.bogusHtmlContent;
if(_5("ie")<=9){
var r=this.editor.document.selection.createRange();
r.move("character",-1);
r.select();
}
}else{
console.error("onKeyPressed: Cannot find the new block node");
}
}else{
if(_5("mozilla")){
if(_d.parentNode.parentNode.nodeName=="LI"){
_d=_d.parentNode.parentNode;
}
}
var fc=_d.firstChild;
if(fc&&fc.nodeType==1&&(fc.nodeName=="UL"||fc.nodeName=="OL")){
_d.insertBefore(fc.ownerDocument.createTextNode(" "),fc);
var _f=_a.create(this.editor.window);
_f.setStart(_d.firstChild,0);
var _10=_a.getSelection(this.editor.window,true);
_10.removeAllRanges();
_10.addRange(_f);
}
}
}
this._checkListLater=false;
}
if(this._pressedEnterInBlock){
if(this._pressedEnterInBlock.previousSibling){
this.removeTrailingBr(this._pressedEnterInBlock.previousSibling);
}
delete this._pressedEnterInBlock;
}
},bogusHtmlContent:"&#160;",blockNodes:/^(?:P|H1|H2|H3|H4|H5|H6|LI)$/,handleEnterKey:function(e){
var _11,_12,_13,_14,_15,_16,doc=this.editor.document,br,rs,txt;
if(e.shiftKey){
var _17=this.editor.selection.getParentElement();
var _18=_a.getAncestor(_17,this.blockNodes);
if(_18){
if(_18.tagName=="LI"){
return true;
}
_11=_a.getSelection(this.editor.window);
_12=_11.getRangeAt(0);
if(!_12.collapsed){
_12.deleteContents();
_11=_a.getSelection(this.editor.window);
_12=_11.getRangeAt(0);
}
if(_a.atBeginningOfContainer(_18,_12.startContainer,_12.startOffset)){
br=doc.createElement("br");
_13=_a.create(this.editor.window);
_18.insertBefore(br,_18.firstChild);
_13.setStartAfter(br);
_11.removeAllRanges();
_11.addRange(_13);
}else{
if(_a.atEndOfContainer(_18,_12.startContainer,_12.startOffset)){
_13=_a.create(this.editor.window);
br=doc.createElement("br");
_18.appendChild(br);
_18.appendChild(doc.createTextNode(" "));
_13.setStart(_18.lastChild,0);
_11.removeAllRanges();
_11.addRange(_13);
}else{
rs=_12.startContainer;
if(rs&&rs.nodeType==3){
txt=rs.nodeValue;
_14=doc.createTextNode(txt.substring(0,_12.startOffset));
_15=doc.createTextNode(txt.substring(_12.startOffset));
_16=doc.createElement("br");
if(_15.nodeValue==""&&_5("webkit")){
_15=doc.createTextNode(" ");
}
_2.place(_14,rs,"after");
_2.place(_16,_14,"after");
_2.place(_15,_16,"after");
_2.destroy(rs);
_13=_a.create(this.editor.window);
_13.setStart(_15,0);
_11.removeAllRanges();
_11.addRange(_13);
return false;
}
return true;
}
}
}else{
_11=_a.getSelection(this.editor.window);
if(_11.rangeCount){
_12=_11.getRangeAt(0);
if(_12&&_12.startContainer){
if(!_12.collapsed){
_12.deleteContents();
_11=_a.getSelection(this.editor.window);
_12=_11.getRangeAt(0);
}
rs=_12.startContainer;
if(rs&&rs.nodeType==3){
var _19=_12.startOffset;
if(rs.length<_19){
ret=this._adjustNodeAndOffset(rs,_19);
rs=ret.node;
_19=ret.offset;
}
txt=rs.nodeValue;
_14=doc.createTextNode(txt.substring(0,_19));
_15=doc.createTextNode(txt.substring(_19));
_16=doc.createElement("br");
if(!_15.length){
_15=doc.createTextNode(" ");
}
if(_14.length){
_2.place(_14,rs,"after");
}else{
_14=rs;
}
_2.place(_16,_14,"after");
_2.place(_15,_16,"after");
_2.destroy(rs);
_13=_a.create(this.editor.window);
_13.setStart(_15,0);
_13.setEnd(_15,_15.length);
_11.removeAllRanges();
_11.addRange(_13);
this.editor.selection.collapse(true);
}else{
var _1a;
if(_12.startOffset>=0){
_1a=rs.childNodes[_12.startOffset];
}
var _16=doc.createElement("br");
var _15=doc.createTextNode(" ");
if(!_1a){
rs.appendChild(_16);
rs.appendChild(_15);
}else{
_2.place(_16,_1a,"before");
_2.place(_15,_16,"after");
}
_13=_a.create(this.editor.window);
_13.setStart(_15,0);
_13.setEnd(_15,_15.length);
_11.removeAllRanges();
_11.addRange(_13);
this.editor.selection.collapse(true);
}
}
}else{
_9.prototype.execCommand.call(this.editor,"inserthtml","<br>");
}
}
return false;
}
var _1b=true;
_11=_a.getSelection(this.editor.window);
_12=_11.getRangeAt(0);
if(!_12.collapsed){
_12.deleteContents();
_11=_a.getSelection(this.editor.window);
_12=_11.getRangeAt(0);
}
var _1c=_a.getBlockAncestor(_12.endContainer,null,this.editor.editNode);
var _1d=_1c.blockNode;
if((this._checkListLater=(_1d&&(_1d.nodeName=="LI"||_1d.parentNode.nodeName=="LI")))){
if(_5("mozilla")){
this._pressedEnterInBlock=_1d;
}
if(/^(\s|&nbsp;|&#160;|\xA0|<span\b[^>]*\bclass=['"]Apple-style-span['"][^>]*>(\s|&nbsp;|&#160;|\xA0)<\/span>)?(<br>)?$/.test(_1d.innerHTML)){
_1d.innerHTML="";
if(_5("webkit")){
_13=_a.create(this.editor.window);
_13.setStart(_1d,0);
_11.removeAllRanges();
_11.addRange(_13);
}
this._checkListLater=false;
}
return true;
}
if(!_1c.blockNode||_1c.blockNode===this.editor.editNode){
try{
_9.prototype.execCommand.call(this.editor,"formatblock",this.blockNodeForEnter);
}
catch(e2){
}
_1c={blockNode:this.editor.selection.getAncestorElement(this.blockNodeForEnter),blockContainer:this.editor.editNode};
if(_1c.blockNode){
if(_1c.blockNode!=this.editor.editNode&&(!(_1c.blockNode.textContent||_1c.blockNode.innerHTML).replace(/^\s+|\s+$/g,"").length)){
this.removeTrailingBr(_1c.blockNode);
return false;
}
}else{
_1c.blockNode=this.editor.editNode;
}
_11=_a.getSelection(this.editor.window);
_12=_11.getRangeAt(0);
}
var _1e=doc.createElement(this.blockNodeForEnter);
_1e.innerHTML=this.bogusHtmlContent;
this.removeTrailingBr(_1c.blockNode);
var _1f=_12.endOffset;
var _20=_12.endContainer;
if(_20.length<_1f){
var ret=this._adjustNodeAndOffset(_20,_1f);
_20=ret.node;
_1f=ret.offset;
}
if(_a.atEndOfContainer(_1c.blockNode,_20,_1f)){
if(_1c.blockNode===_1c.blockContainer){
_1c.blockNode.appendChild(_1e);
}else{
_2.place(_1e,_1c.blockNode,"after");
}
_1b=false;
_13=_a.create(this.editor.window);
_13.setStart(_1e,0);
_11.removeAllRanges();
_11.addRange(_13);
if(this.editor.height){
_7.scrollIntoView(_1e);
}
}else{
if(_a.atBeginningOfContainer(_1c.blockNode,_12.startContainer,_12.startOffset)){
_2.place(_1e,_1c.blockNode,_1c.blockNode===_1c.blockContainer?"first":"before");
if(_1e.nextSibling&&this.editor.height){
_13=_a.create(this.editor.window);
_13.setStart(_1e.nextSibling,0);
_11.removeAllRanges();
_11.addRange(_13);
_7.scrollIntoView(_1e.nextSibling);
}
_1b=false;
}else{
if(_1c.blockNode===_1c.blockContainer){
_1c.blockNode.appendChild(_1e);
}else{
_2.place(_1e,_1c.blockNode,"after");
}
_1b=false;
if(_1c.blockNode.style){
if(_1e.style){
if(_1c.blockNode.style.cssText){
_1e.style.cssText=_1c.blockNode.style.cssText;
}
}
}
rs=_12.startContainer;
var _21;
if(rs&&rs.nodeType==3){
var _22,_23;
_1f=_12.endOffset;
if(rs.length<_1f){
ret=this._adjustNodeAndOffset(rs,_1f);
rs=ret.node;
_1f=ret.offset;
}
txt=rs.nodeValue;
_14=doc.createTextNode(txt.substring(0,_1f));
_15=doc.createTextNode(txt.substring(_1f,txt.length));
_2.place(_14,rs,"before");
_2.place(_15,rs,"after");
_2.destroy(rs);
var _24=_14.parentNode;
while(_24!==_1c.blockNode){
var tg=_24.tagName;
var _25=doc.createElement(tg);
if(_24.style){
if(_25.style){
if(_24.style.cssText){
_25.style.cssText=_24.style.cssText;
}
}
}
if(_24.tagName==="FONT"){
if(_24.color){
_25.color=_24.color;
}
if(_24.face){
_25.face=_24.face;
}
if(_24.size){
_25.size=_24.size;
}
}
_22=_15;
while(_22){
_23=_22.nextSibling;
_25.appendChild(_22);
_22=_23;
}
_2.place(_25,_24,"after");
_14=_24;
_15=_25;
_24=_24.parentNode;
}
_22=_15;
if(_22.nodeType==1||(_22.nodeType==3&&_22.nodeValue)){
_1e.innerHTML="";
}
_21=_22;
while(_22){
_23=_22.nextSibling;
_1e.appendChild(_22);
_22=_23;
}
}
_13=_a.create(this.editor.window);
var _26;
var _27=_21;
if(this.blockNodeForEnter!=="BR"){
while(_27){
_26=_27;
_23=_27.firstChild;
_27=_23;
}
if(_26&&_26.parentNode){
_1e=_26.parentNode;
_13.setStart(_1e,0);
_11.removeAllRanges();
_11.addRange(_13);
if(this.editor.height){
_7.scrollIntoView(_1e);
}
if(_5("mozilla")){
this._pressedEnterInBlock=_1c.blockNode;
}
}else{
_1b=true;
}
}else{
_13.setStart(_1e,0);
_11.removeAllRanges();
_11.addRange(_13);
if(this.editor.height){
_7.scrollIntoView(_1e);
}
if(_5("mozilla")){
this._pressedEnterInBlock=_1c.blockNode;
}
}
}
}
return _1b;
},_adjustNodeAndOffset:function(_28,_29){
while(_28.length<_29&&_28.nextSibling&&_28.nextSibling.nodeType==3){
_29=_29-_28.length;
_28=_28.nextSibling;
}
return {"node":_28,"offset":_29};
},removeTrailingBr:function(_2a){
var _2b=/P|DIV|LI/i.test(_2a.tagName)?_2a:this.editor.selection.getParentOfType(_2a,["P","DIV","LI"]);
if(!_2b){
return;
}
if(_2b.lastChild){
if((_2b.childNodes.length>1&&_2b.lastChild.nodeType==3&&/^[\s\xAD]*$/.test(_2b.lastChild.nodeValue))||_2b.lastChild.tagName=="BR"){
_2.destroy(_2b.lastChild);
}
}
if(!_2b.childNodes.length){
_2b.innerHTML=this.bogusHtmlContent;
}
}});
});
