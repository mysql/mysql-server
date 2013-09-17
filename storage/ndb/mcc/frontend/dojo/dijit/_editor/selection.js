//>>built
define("dijit/_editor/selection",["dojo/dom","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window",".."],function(_1,_2,_3,_4,_5){
_2.getObject("_editor.selection",true,_5);
_2.mixin(_5._editor.selection,{getType:function(){
if(_3("ie")<9){
return _4.doc.selection.type.toLowerCase();
}else{
var _6="text";
var _7;
try{
_7=_4.global.getSelection();
}
catch(e){
}
if(_7&&_7.rangeCount==1){
var _8=_7.getRangeAt(0);
if((_8.startContainer==_8.endContainer)&&((_8.endOffset-_8.startOffset)==1)&&(_8.startContainer.nodeType!=3)){
_6="control";
}
}
return _6;
}
},getSelectedText:function(){
if(_3("ie")<9){
if(_5._editor.selection.getType()=="control"){
return null;
}
return _4.doc.selection.createRange().text;
}else{
var _9=_4.global.getSelection();
if(_9){
return _9.toString();
}
}
return "";
},getSelectedHtml:function(){
if(_3("ie")<9){
if(_5._editor.selection.getType()=="control"){
return null;
}
return _4.doc.selection.createRange().htmlText;
}else{
var _a=_4.global.getSelection();
if(_a&&_a.rangeCount){
var i;
var _b="";
for(i=0;i<_a.rangeCount;i++){
var _c=_a.getRangeAt(i).cloneContents();
var _d=_4.doc.createElement("div");
_d.appendChild(_c);
_b+=_d.innerHTML;
}
return _b;
}
return null;
}
},getSelectedElement:function(){
if(_5._editor.selection.getType()=="control"){
if(_3("ie")<9){
var _e=_4.doc.selection.createRange();
if(_e&&_e.item){
return _4.doc.selection.createRange().item(0);
}
}else{
var _f=_4.global.getSelection();
return _f.anchorNode.childNodes[_f.anchorOffset];
}
}
return null;
},getParentElement:function(){
if(_5._editor.selection.getType()=="control"){
var p=this.getSelectedElement();
if(p){
return p.parentNode;
}
}else{
if(_3("ie")<9){
var r=_4.doc.selection.createRange();
r.collapse(true);
return r.parentElement();
}else{
var _10=_4.global.getSelection();
if(_10){
var _11=_10.anchorNode;
while(_11&&(_11.nodeType!=1)){
_11=_11.parentNode;
}
return _11;
}
}
}
return null;
},hasAncestorElement:function(_12){
return this.getAncestorElement.apply(this,arguments)!=null;
},getAncestorElement:function(_13){
var _14=this.getSelectedElement()||this.getParentElement();
return this.getParentOfType(_14,arguments);
},isTag:function(_15,_16){
if(_15&&_15.tagName){
var _17=_15.tagName.toLowerCase();
for(var i=0;i<_16.length;i++){
var _18=String(_16[i]).toLowerCase();
if(_17==_18){
return _18;
}
}
}
return "";
},getParentOfType:function(_19,_1a){
while(_19){
if(this.isTag(_19,_1a).length){
return _19;
}
_19=_19.parentNode;
}
return null;
},collapse:function(_1b){
if(window.getSelection){
var _1c=_4.global.getSelection();
if(_1c.removeAllRanges){
if(_1b){
_1c.collapseToStart();
}else{
_1c.collapseToEnd();
}
}else{
_1c.collapse(_1b);
}
}else{
if(_3("ie")){
var _1d=_4.doc.selection.createRange();
_1d.collapse(_1b);
_1d.select();
}
}
},remove:function(){
var sel=_4.doc.selection;
if(_3("ie")<9){
if(sel.type.toLowerCase()!="none"){
sel.clear();
}
return sel;
}else{
sel=_4.global.getSelection();
sel.deleteFromDocument();
return sel;
}
},selectElementChildren:function(_1e,_1f){
var _20=_4.global;
var doc=_4.doc;
var _21;
_1e=_1.byId(_1e);
if(doc.selection&&_3("ie")<9&&_4.body().createTextRange){
_21=_1e.ownerDocument.body.createTextRange();
_21.moveToElementText(_1e);
if(!_1f){
try{
_21.select();
}
catch(e){
}
}
}else{
if(_20.getSelection){
var _22=_4.global.getSelection();
if(_3("opera")){
if(_22.rangeCount){
_21=_22.getRangeAt(0);
}else{
_21=doc.createRange();
}
_21.setStart(_1e,0);
_21.setEnd(_1e,(_1e.nodeType==3)?_1e.length:_1e.childNodes.length);
_22.addRange(_21);
}else{
_22.selectAllChildren(_1e);
}
}
}
},selectElement:function(_23,_24){
var _25;
var doc=_4.doc;
var _26=_4.global;
_23=_1.byId(_23);
if(_3("ie")<9&&_4.body().createTextRange){
try{
var tg=_23.tagName?_23.tagName.toLowerCase():"";
if(tg==="img"||tg==="table"){
_25=_4.body().createControlRange();
}else{
_25=_4.body().createRange();
}
_25.addElement(_23);
if(!_24){
_25.select();
}
}
catch(e){
this.selectElementChildren(_23,_24);
}
}else{
if(_26.getSelection){
var _27=_26.getSelection();
_25=doc.createRange();
if(_27.removeAllRanges){
if(_3("opera")){
if(_27.getRangeAt(0)){
_25=_27.getRangeAt(0);
}
}
_25.selectNode(_23);
_27.removeAllRanges();
_27.addRange(_25);
}
}
}
},inSelection:function(_28){
if(_28){
var _29;
var doc=_4.doc;
var _2a;
if(_4.global.getSelection){
var sel=_4.global.getSelection();
if(sel&&sel.rangeCount>0){
_2a=sel.getRangeAt(0);
}
if(_2a&&_2a.compareBoundaryPoints&&doc.createRange){
try{
_29=doc.createRange();
_29.setStart(_28,0);
if(_2a.compareBoundaryPoints(_2a.START_TO_END,_29)===1){
return true;
}
}
catch(e){
}
}
}else{
if(doc.selection){
_2a=doc.selection.createRange();
try{
_29=_28.ownerDocument.body.createControlRange();
if(_29){
_29.addElement(_28);
}
}
catch(e1){
try{
_29=_28.ownerDocument.body.createTextRange();
_29.moveToElementText(_28);
}
catch(e2){
}
}
if(_2a&&_29){
if(_2a.compareEndPoints("EndToStart",_29)===1){
return true;
}
}
}
}
}
return false;
}});
return _5._editor.selection;
});
