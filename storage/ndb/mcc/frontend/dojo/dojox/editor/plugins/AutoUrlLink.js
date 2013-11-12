//>>built
define("dojox/editor/plugins/AutoUrlLink",["dojo","dijit","dojox","dijit/_editor/range","dijit/_editor/selection","dijit/_editor/_Plugin","dijit/form/Button","dojo/_base/connect","dojo/_base/declare","dojo/string"],function(_1,_2,_3){
_1.declare("dojox.editor.plugins.AutoUrlLink",[_2._editor._Plugin],{_template:"<a _djrealurl='${url}' href='${url}'>${url}</a>",setEditor:function(_4){
this.editor=_4;
if(!_1.isIE){
_1.some(_4._plugins,function(_5){
if(_5.isInstanceOf(_2._editor.plugins.EnterKeyHandling)){
this.blockNodeForEnter=_5.blockNodeForEnter;
return true;
}
return false;
},this);
this.connect(_4,"onKeyPress","_keyPress");
this.connect(_4,"onClick","_recognize");
this.connect(_4,"onBlur","_recognize");
}
},_keyPress:function(_6){
var ks=_1.keys,v=118,V=86,kc=_6.keyCode,cc=_6.charCode;
if(cc==ks.SPACE||(_6.ctrlKey&&(cc==v||cc==V))){
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
},_recognize:function(_7){
var _8=this._template,_9=_7?_7.enter:false,ed=this.editor,_a=ed.window.getSelection();
if(_a){
var _b=_9?this._findLastEditingNode(_a.anchorNode):(this._saved||_a.anchorNode),bm=this._saved=_a.anchorNode,_c=_a.anchorOffset;
if(_b.nodeType==3&&!this._inLink(_b)){
var _d=false,_e=this._findUrls(_b,bm,_c),_f=ed.document.createRange(),_10,_11=0,_12=(bm==_b);
_10=_e.shift();
while(_10){
_f.setStart(_b,_10.start);
_f.setEnd(_b,_10.end);
_a.removeAllRanges();
_a.addRange(_f);
ed.execCommand("insertHTML",_1.string.substitute(_8,{url:_f.toString()}));
_11+=_10.end;
_10=_e.shift();
_d=true;
}
if(_12&&(_c=_c-_11)<=0){
return;
}
if(!_d){
return;
}
try{
_f.setStart(bm,0);
_f.setEnd(bm,_c);
_a.removeAllRanges();
_a.addRange(_f);
_1.withGlobal(ed.window,"collapse",_2._editor.selection,[]);
}
catch(e){
}
}
}
},_inLink:function(_13){
var _14=this.editor.editNode,_15=false,_16;
_13=_13.parentNode;
while(_13&&_13!==_14){
_16=_13.tagName?_13.tagName.toLowerCase():"";
if(_16=="a"){
_15=true;
break;
}
_13=_13.parentNode;
}
return _15;
},_findLastEditingNode:function(_17){
var _18=_2.range.BlockTagNames,_19=this.editor.editNode,_1a;
if(!_17){
return _17;
}
if(this.blockNodeForEnter=="BR"&&(!(_1a=_2.range.getBlockAncestor(_17,null,_19).blockNode)||_1a.tagName.toUpperCase()!="LI")){
while((_17=_17.previousSibling)&&_17.nodeType!=3){
}
}else{
if((_1a||(_1a=_2.range.getBlockAncestor(_17,null,_19).blockNode))&&_1a.tagName.toUpperCase()=="LI"){
_17=_1a;
}else{
_17=_2.range.getBlockAncestor(_17,null,_19).blockNode;
}
while((_17=_17.previousSibling)&&!(_17.tagName&&_17.tagName.match(_18))){
}
if(_17){
_17=_17.lastChild;
while(_17){
if(_17.nodeType==3&&_1.trim(_17.nodeValue)!=""){
break;
}else{
if(_17.nodeType==1){
_17=_17.lastChild;
}else{
_17=_17.previousSibling;
}
}
}
}
}
return _17;
},_findUrls:function(_1b,bm,_1c){
var _1d=/(http|https|ftp):\/\/[^\s]+/ig,_1e=[],_1f=0,_20=_1b.nodeValue,_21,ch;
if(_1b===bm&&_1c<_20.length){
_20=_20.substr(0,_1c);
}
while((_21=_1d.exec(_20))!=null){
if(_21.index==0||(ch=_20.charAt(_21.index-1))==" "||ch==" "){
_1e.push({start:_21.index-_1f,end:_21.index+_21[0].length-_1f});
_1f=_21.index+_21[0].length;
}
}
return _1e;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _22=o.args.name.toLowerCase();
if(_22==="autourllink"){
o.plugin=new _3.editor.plugins.AutoUrlLink();
}
});
return _3.editor.plugins.AutoUrlLink;
});
