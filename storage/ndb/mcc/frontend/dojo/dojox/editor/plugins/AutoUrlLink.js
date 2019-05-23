//>>built
define("dojox/editor/plugins/AutoUrlLink",["dojo","dijit","dojox","dijit/_editor/range","dijit/_editor/selection","dijit/_editor/_Plugin","dijit/form/Button","dojo/_base/connect","dojo/_base/declare","dojo/string"],function(_1,_2,_3,_4,_5,_6){
_1.declare("dojox.editor.plugins.AutoUrlLink",[_6],{_template:"<a _djrealurl='${url}' href='${url}'>${url}</a>",setEditor:function(_7){
this.editor=_7;
if(!_1.isIE){
_1.some(_7._plugins,function(_8){
if(_8.isInstanceOf(_2._editor.plugins.EnterKeyHandling)){
this.blockNodeForEnter=_8.blockNodeForEnter;
return true;
}
return false;
},this);
this.connect(_7,"onKeyPress","_keyPress");
this.connect(_7,"onClick","_recognize");
this.connect(_7,"onBlur","_recognize");
}
},_keyPress:function(_9){
var ks=_1.keys,v=118,V=86,kc=_9.keyCode,cc=_9.charCode;
if(cc==ks.SPACE||(_9.ctrlKey&&(cc==v||cc==V))){
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
},_recognize:function(_a){
var _b=this._template,_c=_a?_a.enter:false,ed=this.editor,_5=ed.window.getSelection();
if(_5){
var _d=_c?this._findLastEditingNode(_5.anchorNode):(this._saved||_5.anchorNode),bm=this._saved=_5.anchorNode,_e=_5.anchorOffset;
if(_d.nodeType==3&&!this._inLink(_d)){
var _f=false,_10=this._findUrls(_d,bm,_e),_4=ed.document.createRange(),_11,_12=0,_13=(bm==_d);
_11=_10.shift();
while(_11){
_4.setStart(_d,_11.start);
_4.setEnd(_d,_11.end);
_5.removeAllRanges();
_5.addRange(_4);
ed.execCommand("insertHTML",_1.string.substitute(_b,{url:_4.toString()}));
_12+=_11.end;
_11=_10.shift();
_f=true;
}
if(_13&&(_e=_e-_12)<=0){
return;
}
if(!_f){
return;
}
try{
_4.setStart(bm,0);
_4.setEnd(bm,_e);
_5.removeAllRanges();
_5.addRange(_4);
ed._sCall("collapse",[]);
}
catch(e){
}
}
}
},_inLink:function(_14){
var _15=this.editor.editNode,_16=false,_17;
_14=_14.parentNode;
while(_14&&_14!==_15){
_17=_14.tagName?_14.tagName.toLowerCase():"";
if(_17=="a"){
_16=true;
break;
}
_14=_14.parentNode;
}
return _16;
},_findLastEditingNode:function(_18){
var _19=_2.range.BlockTagNames,_1a=this.editor.editNode,_1b;
if(!_18){
return _18;
}
if(this.blockNodeForEnter=="BR"&&(!(_1b=_2.range.getBlockAncestor(_18,null,_1a).blockNode)||_1b.tagName.toUpperCase()!="LI")){
while((_18=_18.previousSibling)&&_18.nodeType!=3){
}
}else{
if((_1b||(_1b=_2.range.getBlockAncestor(_18,null,_1a).blockNode))&&_1b.tagName.toUpperCase()=="LI"){
_18=_1b;
}else{
_18=_2.range.getBlockAncestor(_18,null,_1a).blockNode;
}
while((_18=_18.previousSibling)&&!(_18.tagName&&_18.tagName.match(_19))){
}
if(_18){
_18=_18.lastChild;
while(_18){
if(_18.nodeType==3&&_1.trim(_18.nodeValue)!=""){
break;
}else{
if(_18.nodeType==1){
_18=_18.lastChild;
}else{
_18=_18.previousSibling;
}
}
}
}
}
return _18;
},_findUrls:function(_1c,bm,_1d){
var _1e=/(http|https|ftp):\/\/[^\s]+/ig,_1f=[],_20=0,_21=_1c.nodeValue,_22,ch;
if(_1c===bm&&_1d<_21.length){
_21=_21.substr(0,_1d);
}
while((_22=_1e.exec(_21))!=null){
if(_22.index==0||(ch=_21.charAt(_22.index-1))==" "||ch==" "){
_1f.push({start:_22.index-_20,end:_22.index+_22[0].length-_20});
_20=_22.index+_22[0].length;
}
}
return _1f;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _23=o.args.name.toLowerCase();
if(_23==="autourllink"){
o.plugin=new _3.editor.plugins.AutoUrlLink();
}
});
return _3.editor.plugins.AutoUrlLink;
});
