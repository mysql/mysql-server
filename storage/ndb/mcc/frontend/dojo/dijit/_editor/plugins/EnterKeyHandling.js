//>>built
define("dijit/_editor/plugins/EnterKeyHandling",["dojo/_base/declare","dojo/dom-construct","dojo/_base/event","dojo/keys","dojo/_base/lang","dojo/sniff","dojo/_base/window","dojo/window","../_Plugin","../RichText","../range","../../_base/focus"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
return _1("dijit._editor.plugins.EnterKeyHandling",_9,{blockNodeForEnter:"BR",constructor:function(_d){
if(_d){
if("blockNodeForEnter" in _d){
_d.blockNodeForEnter=_d.blockNodeForEnter.toUpperCase();
}
_5.mixin(this,_d);
}
},setEditor:function(_e){
if(this.editor===_e){
return;
}
this.editor=_e;
if(this.blockNodeForEnter=="BR"){
this.editor.customUndo=true;
_e.onLoadDeferred.then(_5.hitch(this,function(d){
this.connect(_e.document,"onkeypress",function(e){
if(e.charOrCode==_4.ENTER){
var ne=_5.mixin({},e);
ne.shiftKey=true;
if(!this.handleEnterKey(ne)){
_3.stop(e);
}
}
});
if(_6("ie")>=9&&_6("ie")<=10){
this.connect(_e.document,"onpaste",function(e){
setTimeout(dojo.hitch(this,function(){
var r=this.editor.document.selection.createRange();
r.move("character",-1);
r.select();
r.move("character",1);
r.select();
}),0);
});
}
return d;
}));
}else{
if(this.blockNodeForEnter){
var h=_5.hitch(this,this.handleEnterKey);
_e.addKeyHandler(13,0,0,h);
_e.addKeyHandler(13,0,1,h);
this.connect(this.editor,"onKeyPressed","onKeyPressed");
}
}
},onKeyPressed:function(){
if(this._checkListLater){
if(_7.withGlobal(this.editor.window,"isCollapsed",_c)){
var _f=this.editor._sCall("getAncestorElement",["LI"]);
if(!_f){
_a.prototype.execCommand.call(this.editor,"formatblock",this.blockNodeForEnter);
var _10=this.editor._sCall("getAncestorElement",[this.blockNodeForEnter]);
if(_10){
_10.innerHTML=this.bogusHtmlContent;
if(_6("ie")<=9){
var r=this.editor.document.selection.createRange();
r.move("character",-1);
r.select();
}
}else{
console.error("onKeyPressed: Cannot find the new block node");
}
}else{
if(_6("mozilla")){
if(_f.parentNode.parentNode.nodeName=="LI"){
_f=_f.parentNode.parentNode;
}
}
var fc=_f.firstChild;
if(fc&&fc.nodeType==1&&(fc.nodeName=="UL"||fc.nodeName=="OL")){
_f.insertBefore(fc.ownerDocument.createTextNode(" "),fc);
var _11=_b.create(this.editor.window);
_11.setStart(_f.firstChild,0);
var _12=_b.getSelection(this.editor.window,true);
_12.removeAllRanges();
_12.addRange(_11);
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
var _13,_14,_15,_16,_17,_18,doc=this.editor.document,br,rs,txt;
if(e.shiftKey){
var _19=this.editor._sCall("getParentElement",[]);
var _1a=_b.getAncestor(_19,this.blockNodes);
if(_1a){
if(_1a.tagName=="LI"){
return true;
}
_13=_b.getSelection(this.editor.window);
_14=_13.getRangeAt(0);
if(!_14.collapsed){
_14.deleteContents();
_13=_b.getSelection(this.editor.window);
_14=_13.getRangeAt(0);
}
if(_b.atBeginningOfContainer(_1a,_14.startContainer,_14.startOffset)){
br=doc.createElement("br");
_15=_b.create(this.editor.window);
_1a.insertBefore(br,_1a.firstChild);
_15.setStartAfter(br);
_13.removeAllRanges();
_13.addRange(_15);
}else{
if(_b.atEndOfContainer(_1a,_14.startContainer,_14.startOffset)){
_15=_b.create(this.editor.window);
br=doc.createElement("br");
_1a.appendChild(br);
_1a.appendChild(doc.createTextNode(" "));
_15.setStart(_1a.lastChild,0);
_13.removeAllRanges();
_13.addRange(_15);
}else{
rs=_14.startContainer;
if(rs&&rs.nodeType==3){
txt=rs.nodeValue;
_16=doc.createTextNode(txt.substring(0,_14.startOffset));
_17=doc.createTextNode(txt.substring(_14.startOffset));
_18=doc.createElement("br");
if(_17.nodeValue==""&&_6("webkit")){
_17=doc.createTextNode(" ");
}
_2.place(_16,rs,"after");
_2.place(_18,_16,"after");
_2.place(_17,_18,"after");
_2.destroy(rs);
_15=_b.create(this.editor.window);
_15.setStart(_17,0);
_13.removeAllRanges();
_13.addRange(_15);
return false;
}
return true;
}
}
}else{
_13=_b.getSelection(this.editor.window);
if(_13.rangeCount){
_14=_13.getRangeAt(0);
if(_14&&_14.startContainer){
if(!_14.collapsed){
_14.deleteContents();
_13=_b.getSelection(this.editor.window);
_14=_13.getRangeAt(0);
}
rs=_14.startContainer;
if(rs&&rs.nodeType==3){
var _1b=_14.startOffset;
if(rs.length<_1b){
ret=this._adjustNodeAndOffset(rs,_1b);
rs=ret.node;
_1b=ret.offset;
}
txt=rs.nodeValue;
_16=doc.createTextNode(txt.substring(0,_1b));
_17=doc.createTextNode(txt.substring(_1b));
_18=doc.createElement("br");
if(!_17.length){
_17=doc.createTextNode(" ");
}
if(_16.length){
_2.place(_16,rs,"after");
}else{
_16=rs;
}
_2.place(_18,_16,"after");
_2.place(_17,_18,"after");
_2.destroy(rs);
_15=_b.create(this.editor.window);
_15.setStart(_17,0);
_15.setEnd(_17,_17.length);
_13.removeAllRanges();
_13.addRange(_15);
this.editor._sCall("collapse",[true]);
}else{
var _1c;
if(_14.startOffset>=0){
_1c=rs.childNodes[_14.startOffset];
}
var _18=doc.createElement("br");
var _17=doc.createTextNode(" ");
if(!_1c){
rs.appendChild(_18);
rs.appendChild(_17);
}else{
_2.place(_18,_1c,"before");
_2.place(_17,_18,"after");
}
_15=_b.create(this.editor.window);
_15.setStart(_17,0);
_15.setEnd(_17,_17.length);
_13.removeAllRanges();
_13.addRange(_15);
this.editor._sCall("collapse",[true]);
}
}
}else{
_a.prototype.execCommand.call(this.editor,"inserthtml","<br>");
}
}
return false;
}
var _1d=true;
_13=_b.getSelection(this.editor.window);
_14=_13.getRangeAt(0);
if(!_14.collapsed){
_14.deleteContents();
_13=_b.getSelection(this.editor.window);
_14=_13.getRangeAt(0);
}
var _1e=_b.getBlockAncestor(_14.endContainer,null,this.editor.editNode);
var _1f=_1e.blockNode;
if((this._checkListLater=(_1f&&(_1f.nodeName=="LI"||_1f.parentNode.nodeName=="LI")))){
if(_6("mozilla")){
this._pressedEnterInBlock=_1f;
}
if(/^(\s|&nbsp;|&#160;|\xA0|<span\b[^>]*\bclass=['"]Apple-style-span['"][^>]*>(\s|&nbsp;|&#160;|\xA0)<\/span>)?(<br>)?$/.test(_1f.innerHTML)){
_1f.innerHTML="";
if(_6("webkit")){
_15=_b.create(this.editor.window);
_15.setStart(_1f,0);
_13.removeAllRanges();
_13.addRange(_15);
}
this._checkListLater=false;
}
return true;
}
if(!_1e.blockNode||_1e.blockNode===this.editor.editNode){
try{
_a.prototype.execCommand.call(this.editor,"formatblock",this.blockNodeForEnter);
}
catch(e2){
}
_1e={blockNode:this.editor._sCall("getAncestorElement",[this.blockNodeForEnter]),blockContainer:this.editor.editNode};
if(_1e.blockNode){
if(_1e.blockNode!=this.editor.editNode&&(!(_1e.blockNode.textContent||_1e.blockNode.innerHTML).replace(/^\s+|\s+$/g,"").length)){
this.removeTrailingBr(_1e.blockNode);
return false;
}
}else{
_1e.blockNode=this.editor.editNode;
}
_13=_b.getSelection(this.editor.window);
_14=_13.getRangeAt(0);
}
var _20=doc.createElement(this.blockNodeForEnter);
_20.innerHTML=this.bogusHtmlContent;
this.removeTrailingBr(_1e.blockNode);
var _21=_14.endOffset;
var _22=_14.endContainer;
if(_22.length<_21){
var ret=this._adjustNodeAndOffset(_22,_21);
_22=ret.node;
_21=ret.offset;
}
if(_b.atEndOfContainer(_1e.blockNode,_22,_21)){
if(_1e.blockNode===_1e.blockContainer){
_1e.blockNode.appendChild(_20);
}else{
_2.place(_20,_1e.blockNode,"after");
}
_1d=false;
_15=_b.create(this.editor.window);
_15.setStart(_20,0);
_13.removeAllRanges();
_13.addRange(_15);
if(this.editor.height){
_8.scrollIntoView(_20);
}
}else{
if(_b.atBeginningOfContainer(_1e.blockNode,_14.startContainer,_14.startOffset)){
_2.place(_20,_1e.blockNode,_1e.blockNode===_1e.blockContainer?"first":"before");
if(_20.nextSibling&&this.editor.height){
_15=_b.create(this.editor.window);
_15.setStart(_20.nextSibling,0);
_13.removeAllRanges();
_13.addRange(_15);
_8.scrollIntoView(_20.nextSibling);
}
_1d=false;
}else{
if(_1e.blockNode===_1e.blockContainer){
_1e.blockNode.appendChild(_20);
}else{
_2.place(_20,_1e.blockNode,"after");
}
_1d=false;
if(_1e.blockNode.style){
if(_20.style){
if(_1e.blockNode.style.cssText){
_20.style.cssText=_1e.blockNode.style.cssText;
}
}
}
rs=_14.startContainer;
var _23;
if(rs&&rs.nodeType==3){
var _24,_25;
_21=_14.endOffset;
if(rs.length<_21){
ret=this._adjustNodeAndOffset(rs,_21);
rs=ret.node;
_21=ret.offset;
}
txt=rs.nodeValue;
_16=doc.createTextNode(txt.substring(0,_21));
_17=doc.createTextNode(txt.substring(_21,txt.length));
_2.place(_16,rs,"before");
_2.place(_17,rs,"after");
_2.destroy(rs);
var _26=_16.parentNode;
while(_26!==_1e.blockNode){
var tg=_26.tagName;
var _27=doc.createElement(tg);
if(_26.style){
if(_27.style){
if(_26.style.cssText){
_27.style.cssText=_26.style.cssText;
}
}
}
if(_26.tagName==="FONT"){
if(_26.color){
_27.color=_26.color;
}
if(_26.face){
_27.face=_26.face;
}
if(_26.size){
_27.size=_26.size;
}
}
_24=_17;
while(_24){
_25=_24.nextSibling;
_27.appendChild(_24);
_24=_25;
}
_2.place(_27,_26,"after");
_16=_26;
_17=_27;
_26=_26.parentNode;
}
_24=_17;
if(_24.nodeType==1||(_24.nodeType==3&&_24.nodeValue)){
_20.innerHTML="";
}
_23=_24;
while(_24){
_25=_24.nextSibling;
_20.appendChild(_24);
_24=_25;
}
}
_15=_b.create(this.editor.window);
var _28;
var _29=_23;
if(this.blockNodeForEnter!=="BR"){
while(_29){
_28=_29;
_25=_29.firstChild;
_29=_25;
}
if(_28&&_28.parentNode){
_20=_28.parentNode;
_15.setStart(_20,0);
_13.removeAllRanges();
_13.addRange(_15);
if(this.editor.height){
_8.scrollIntoView(_20);
}
if(_6("mozilla")){
this._pressedEnterInBlock=_1e.blockNode;
}
}else{
_1d=true;
}
}else{
_15.setStart(_20,0);
_13.removeAllRanges();
_13.addRange(_15);
if(this.editor.height){
_8.scrollIntoView(_20);
}
if(_6("mozilla")){
this._pressedEnterInBlock=_1e.blockNode;
}
}
}
}
return _1d;
},_adjustNodeAndOffset:function(_2a,_2b){
while(_2a.length<_2b&&_2a.nextSibling&&_2a.nextSibling.nodeType==3){
_2b=_2b-_2a.length;
_2a=_2a.nextSibling;
}
return {"node":_2a,"offset":_2b};
},removeTrailingBr:function(_2c){
var _2d=/P|DIV|LI/i.test(_2c.tagName)?_2c:this.editor._sCall("getParentOfType",[_2c,["P","DIV","LI"]]);
if(!_2d){
return;
}
if(_2d.lastChild){
if((_2d.childNodes.length>1&&_2d.lastChild.nodeType==3&&/^[\s\xAD]*$/.test(_2d.lastChild.nodeValue))||_2d.lastChild.tagName=="BR"){
_2.destroy(_2d.lastChild);
}
}
if(!_2d.childNodes.length){
_2d.innerHTML=this.bogusHtmlContent;
}
}});
});
