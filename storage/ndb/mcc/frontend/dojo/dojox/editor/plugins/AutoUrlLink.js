//>>built
define("dojox/editor/plugins/AutoUrlLink",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/form/Button","dojo/_base/declare","dojo/string"],function(_1,_2,_3,_4){
var _5=_1.declare("dojox.editor.plugins.AutoUrlLink",[_4],{_template:"<a _djrealurl='${url}' href='${url}'>${url}</a>",setEditor:function(_6){
this.editor=_6;
if(!_1.isIE){
_1.some(_6._plugins,function(_7){
if(_7.isInstanceOf(_2._editor.plugins.EnterKeyHandling)){
this.blockNodeForEnter=_7.blockNodeForEnter;
return true;
}
return false;
},this);
this.connect(_6,"onKeyPress","_keyPress");
this.connect(_6,"onClick","_recognize");
this.connect(_6,"onBlur","_recognize");
}
},_keyPress:function(_8){
var ks=_1.keys,v=118,V=86,kc=_8.keyCode,cc=_8.charCode;
if(cc==ks.SPACE||(_8.ctrlKey&&(cc==v||cc==V))){
setTimeout(_1.hitch(this,"_recognize"),0);
}else{
if(kc==ks.ENTER){
setTimeout(_1.hitch(this,function(){
this._recognize({enter:true});
}),0);
}else{
this._saved=this.editor.window.getSelection().anchorNode;
}
}
},_recognize:function(_9){
var _a=this._template,_b=_9?_9.enter:false,ed=this.editor,_c=ed.window.getSelection();
if(_c){
var _d=_b?this._findLastEditingNode(_c.anchorNode):(this._saved||_c.anchorNode),bm=this._saved=_c.anchorNode,_e=_c.anchorOffset;
if(_d.nodeType==3&&!this._inLink(_d)){
var _f=false,_10=this._findUrls(_d,bm,_e),_11=ed.document.createRange(),_12,_13=0,_14=(bm==_d);
_12=_10.shift();
while(_12){
_11.setStart(_d,_12.start);
_11.setEnd(_d,_12.end);
_c.removeAllRanges();
_c.addRange(_11);
ed.execCommand("insertHTML",_1.string.substitute(_a,{url:_11.toString()}));
_13+=_12.end;
_12=_10.shift();
_f=true;
}
if(_14&&(_e=_e-_13)<=0){
return;
}
if(!_f){
return;
}
try{
_11.setStart(bm,0);
_11.setEnd(bm,_e);
_c.removeAllRanges();
_c.addRange(_11);
ed._sCall("collapse",[]);
}
catch(e){
}
}
}
},_inLink:function(_15){
var _16=this.editor.editNode,_17=false,_18;
_15=_15.parentNode;
while(_15&&_15!==_16){
_18=_15.tagName?_15.tagName.toLowerCase():"";
if(_18=="a"){
_17=true;
break;
}
_15=_15.parentNode;
}
return _17;
},_findLastEditingNode:function(_19){
var _1a=_2.range.BlockTagNames,_1b=this.editor.editNode,_1c;
if(!_19){
return _19;
}
if(this.blockNodeForEnter=="BR"&&(!(_1c=_2.range.getBlockAncestor(_19,null,_1b).blockNode)||_1c.tagName.toUpperCase()!="LI")){
while((_19=_19.previousSibling)&&_19.nodeType!=3){
}
}else{
if((_1c||(_1c=_2.range.getBlockAncestor(_19,null,_1b).blockNode))&&_1c.tagName.toUpperCase()=="LI"){
_19=_1c;
}else{
_19=_2.range.getBlockAncestor(_19,null,_1b).blockNode;
}
while((_19=_19.previousSibling)&&!(_19.tagName&&_19.tagName.match(_1a))){
}
if(_19){
_19=_19.lastChild;
while(_19){
if(_19.nodeType==3&&_1.trim(_19.nodeValue)!=""){
break;
}else{
if(_19.nodeType==1){
_19=_19.lastChild;
}else{
_19=_19.previousSibling;
}
}
}
}
}
return _19;
},_findUrls:function(_1d,bm,_1e){
var _1f=/(http|https|ftp):\/\/[^\s]+/ig,_20=[],_21=0,_22=_1d.nodeValue,_23,ch;
if(_1d===bm&&_1e<_22.length){
_22=_22.substr(0,_1e);
}
while((_23=_1f.exec(_22))!=null){
if(_23.index==0||(ch=_22.charAt(_23.index-1))==" "||ch==" "){
_20.push({start:_23.index-_21,end:_23.index+_23[0].length-_21});
_21=_23.index+_23[0].length;
}
}
return _20;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _24=o.args.name.toLowerCase();
if(_24==="autourllink"){
o.plugin=new _5();
}
});
return _5;
});
