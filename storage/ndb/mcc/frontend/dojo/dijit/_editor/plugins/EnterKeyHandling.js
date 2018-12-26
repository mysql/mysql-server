//>>built
define("dijit/_editor/plugins/EnterKeyHandling",["dojo/_base/declare","dojo/dom-construct","dojo/_base/event","dojo/keys","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/window","../_Plugin","../RichText","../range","../selection"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
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
_e.onLoadDeferred.addCallback(_5.hitch(this,function(d){
this.connect(_e.document,"onkeypress",function(e){
if(e.charOrCode==_4.ENTER){
var ne=_5.mixin({},e);
ne.shiftKey=true;
if(!this.handleEnterKey(ne)){
_3.stop(e);
}
}
});
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
if(_7.withGlobal(this.editor.window,"isCollapsed",dijit)){
var _f=_7.withGlobal(this.editor.window,"getAncestorElement",_10,["LI"]);
if(!_f){
_a.prototype.execCommand.call(this.editor,"formatblock",this.blockNodeForEnter);
var _11=_7.withGlobal(this.editor.window,"getAncestorElement",_10,[this.blockNodeForEnter]);
if(_11){
_11.innerHTML=this.bogusHtmlContent;
if(_6("ie")){
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
var _12=_b.create(this.editor.window);
_12.setStart(_f.firstChild,0);
var _10=_b.getSelection(this.editor.window,true);
_10.removeAllRanges();
_10.addRange(_12);
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
var _19=_7.withGlobal(this.editor.window,"getParentElement",_c);
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
_7.withGlobal(this.editor.window,function(){
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
_15=_b.create();
_15.setStart(_17,0);
_13.removeAllRanges();
_13.addRange(_15);
});
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
_7.withGlobal(this.editor.window,_5.hitch(this,function(){
var _1b=false;
var _1c=_14.startOffset;
if(rs.length<_1c){
ret=this._adjustNodeAndOffset(rs,_1c);
rs=ret.node;
_1c=ret.offset;
}
txt=rs.nodeValue;
_16=doc.createTextNode(txt.substring(0,_1c));
_17=doc.createTextNode(txt.substring(_1c));
_18=doc.createElement("br");
if(!_17.length){
_17=doc.createTextNode(" ");
_1b=true;
}
if(_16.length){
_2.place(_16,rs,"after");
}else{
_16=rs;
}
_2.place(_18,_16,"after");
_2.place(_17,_18,"after");
_2.destroy(rs);
_15=_b.create();
_15.setStart(_17,0);
_15.setEnd(_17,_17.length);
_13.removeAllRanges();
_13.addRange(_15);
if(_1b&&!_6("webkit")){
_c.remove();
}else{
_c.collapse(true);
}
}));
}else{
var _1d;
if(_14.startOffset>=0){
_1d=rs.childNodes[_14.startOffset];
}
_7.withGlobal(this.editor.window,_5.hitch(this,function(){
var _1e=doc.createElement("br");
var _1f=doc.createTextNode(" ");
if(!_1d){
rs.appendChild(_1e);
rs.appendChild(_1f);
}else{
_2.place(_1e,_1d,"before");
_2.place(_1f,_1e,"after");
}
_15=_b.create(_7.global);
_15.setStart(_1f,0);
_15.setEnd(_1f,_1f.length);
_13.removeAllRanges();
_13.addRange(_15);
_c.collapse(true);
}));
}
}
}else{
_a.prototype.execCommand.call(this.editor,"inserthtml","<br>");
}
}
return false;
}
var _20=true;
_13=_b.getSelection(this.editor.window);
_14=_13.getRangeAt(0);
if(!_14.collapsed){
_14.deleteContents();
_13=_b.getSelection(this.editor.window);
_14=_13.getRangeAt(0);
}
var _21=_b.getBlockAncestor(_14.endContainer,null,this.editor.editNode);
var _22=_21.blockNode;
if((this._checkListLater=(_22&&(_22.nodeName=="LI"||_22.parentNode.nodeName=="LI")))){
if(_6("mozilla")){
this._pressedEnterInBlock=_22;
}
if(/^(\s|&nbsp;|&#160;|\xA0|<span\b[^>]*\bclass=['"]Apple-style-span['"][^>]*>(\s|&nbsp;|&#160;|\xA0)<\/span>)?(<br>)?$/.test(_22.innerHTML)){
_22.innerHTML="";
if(_6("webkit")){
_15=_b.create(this.editor.window);
_15.setStart(_22,0);
_13.removeAllRanges();
_13.addRange(_15);
}
this._checkListLater=false;
}
return true;
}
if(!_21.blockNode||_21.blockNode===this.editor.editNode){
try{
_a.prototype.execCommand.call(this.editor,"formatblock",this.blockNodeForEnter);
}
catch(e2){
}
_21={blockNode:_7.withGlobal(this.editor.window,"getAncestorElement",_c,[this.blockNodeForEnter]),blockContainer:this.editor.editNode};
if(_21.blockNode){
if(_21.blockNode!=this.editor.editNode&&(!(_21.blockNode.textContent||_21.blockNode.innerHTML).replace(/^\s+|\s+$/g,"").length)){
this.removeTrailingBr(_21.blockNode);
return false;
}
}else{
_21.blockNode=this.editor.editNode;
}
_13=_b.getSelection(this.editor.window);
_14=_13.getRangeAt(0);
}
var _23=doc.createElement(this.blockNodeForEnter);
_23.innerHTML=this.bogusHtmlContent;
this.removeTrailingBr(_21.blockNode);
var _24=_14.endOffset;
var _25=_14.endContainer;
if(_25.length<_24){
var ret=this._adjustNodeAndOffset(_25,_24);
_25=ret.node;
_24=ret.offset;
}
if(_b.atEndOfContainer(_21.blockNode,_25,_24)){
if(_21.blockNode===_21.blockContainer){
_21.blockNode.appendChild(_23);
}else{
_2.place(_23,_21.blockNode,"after");
}
_20=false;
_15=_b.create(this.editor.window);
_15.setStart(_23,0);
_13.removeAllRanges();
_13.addRange(_15);
if(this.editor.height){
_8.scrollIntoView(_23);
}
}else{
if(_b.atBeginningOfContainer(_21.blockNode,_14.startContainer,_14.startOffset)){
_2.place(_23,_21.blockNode,_21.blockNode===_21.blockContainer?"first":"before");
if(_23.nextSibling&&this.editor.height){
_15=_b.create(this.editor.window);
_15.setStart(_23.nextSibling,0);
_13.removeAllRanges();
_13.addRange(_15);
_8.scrollIntoView(_23.nextSibling);
}
_20=false;
}else{
if(_21.blockNode===_21.blockContainer){
_21.blockNode.appendChild(_23);
}else{
_2.place(_23,_21.blockNode,"after");
}
_20=false;
if(_21.blockNode.style){
if(_23.style){
if(_21.blockNode.style.cssText){
_23.style.cssText=_21.blockNode.style.cssText;
}
}
}
rs=_14.startContainer;
var _26;
if(rs&&rs.nodeType==3){
var _27,_28;
_24=_14.endOffset;
if(rs.length<_24){
ret=this._adjustNodeAndOffset(rs,_24);
rs=ret.node;
_24=ret.offset;
}
txt=rs.nodeValue;
_16=doc.createTextNode(txt.substring(0,_24));
_17=doc.createTextNode(txt.substring(_24,txt.length));
_2.place(_16,rs,"before");
_2.place(_17,rs,"after");
_2.destroy(rs);
var _29=_16.parentNode;
while(_29!==_21.blockNode){
var tg=_29.tagName;
var _2a=doc.createElement(tg);
if(_29.style){
if(_2a.style){
if(_29.style.cssText){
_2a.style.cssText=_29.style.cssText;
}
}
}
if(_29.tagName==="FONT"){
if(_29.color){
_2a.color=_29.color;
}
if(_29.face){
_2a.face=_29.face;
}
if(_29.size){
_2a.size=_29.size;
}
}
_27=_17;
while(_27){
_28=_27.nextSibling;
_2a.appendChild(_27);
_27=_28;
}
_2.place(_2a,_29,"after");
_16=_29;
_17=_2a;
_29=_29.parentNode;
}
_27=_17;
if(_27.nodeType==1||(_27.nodeType==3&&_27.nodeValue)){
_23.innerHTML="";
}
_26=_27;
while(_27){
_28=_27.nextSibling;
_23.appendChild(_27);
_27=_28;
}
}
_15=_b.create(this.editor.window);
var _2b;
var _2c=_26;
if(this.blockNodeForEnter!=="BR"){
while(_2c){
_2b=_2c;
_28=_2c.firstChild;
_2c=_28;
}
if(_2b&&_2b.parentNode){
_23=_2b.parentNode;
_15.setStart(_23,0);
_13.removeAllRanges();
_13.addRange(_15);
if(this.editor.height){
_8.scrollIntoView(_23);
}
if(_6("mozilla")){
this._pressedEnterInBlock=_21.blockNode;
}
}else{
_20=true;
}
}else{
_15.setStart(_23,0);
_13.removeAllRanges();
_13.addRange(_15);
if(this.editor.height){
_8.scrollIntoView(_23);
}
if(_6("mozilla")){
this._pressedEnterInBlock=_21.blockNode;
}
}
}
}
return _20;
},_adjustNodeAndOffset:function(_2d,_2e){
while(_2d.length<_2e&&_2d.nextSibling&&_2d.nextSibling.nodeType==3){
_2e=_2e-_2d.length;
_2d=_2d.nextSibling;
}
return {"node":_2d,"offset":_2e};
},removeTrailingBr:function(_2f){
var _30=/P|DIV|LI/i.test(_2f.tagName)?_2f:_c.getParentOfType(_2f,["P","DIV","LI"]);
if(!_30){
return;
}
if(_30.lastChild){
if((_30.childNodes.length>1&&_30.lastChild.nodeType==3&&/^[\s\xAD]*$/.test(_30.lastChild.nodeValue))||_30.lastChild.tagName=="BR"){
_2.destroy(_30.lastChild);
}
}
if(!_30.childNodes.length){
_30.innerHTML=this.bogusHtmlContent;
}
}});
});
