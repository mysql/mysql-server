//>>built
define("dojox/editor/plugins/BidiSupport",["dojo/_base/declare","dojo/_base/array","dojo/aspect","dojo/_base/lang","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/i18n","dojo/NodeList-dom","dojo/NodeList-traverse","dojo/dom-style","dojo/sniff","dojo/query","dijit","dojox","dijit/_editor/_Plugin","dijit/_editor/range","dijit/_editor/plugins/EnterKeyHandling","dijit/_editor/plugins/FontChoice","./NormalizeIndentOutdent","dijit/form/ToggleButton","dojo/i18n!./nls/BidiSupport"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15){
var _16=_1("dojox.editor.plugins.BidiSupport",_10,{useDefaultCommand:false,buttonClass:null,iconClassPrefix:"dijitAdditionalEditorIcon",command:"bidiSupport",blockMode:"DIV",shortcutonly:false,bogusHtmlContent:"&nbsp;",buttonLtr:null,buttonRtl:null,_indentBy:40,_lineTextArray:["DIV","P","LI","H1","H2","H3","H4","H5","H6","ADDRESS","PRE","DT","DE","TD"],_lineStyledTextArray:["H1","H2","H3","H4","H5","H6","ADDRESS","PRE","P"],_tableContainers:["TABLE","THEAD","TBODY","TR"],_blockContainers:["TABLE","OL","UL","BLOCKQUOTE"],_initButton:function(){
if(this.shortcutonly){
return;
}
if(!this.buttonLtr){
this.buttonLtr=this._createButton("ltr");
}
if(!this.buttonRtl){
this.buttonRtl=this._createButton("rtl");
}
},_createButton:function(_17){
return _15(_4.mixin({label:_8.getLocalization("dojox.editor.plugins","BidiSupport")[_17],dir:this.editor.dir,lang:this.editor.lang,showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+(_17=="ltr"?"ParaLeftToRight":"ParaRightToLeft"),onClick:_4.hitch(this,"_changeState",[_17])},this.params||{}));
},setToolbar:function(_18){
if(this.shortcutonly){
return;
}
if(this.editor.isLeftToRight()){
_18.addChild(this.buttonLtr);
_18.addChild(this.buttonRtl);
}else{
_18.addChild(this.buttonRtl);
_18.addChild(this.buttonLtr);
}
},updateState:function(){
if(!this.editor||!this.editor.isLoaded||this.shortcutonly){
return;
}
this.buttonLtr.set("disabled",!!this.disabled);
this.buttonRtl.set("disabled",!!this.disabled);
if(this.disabled){
return;
}
var sel=_11.getSelection(this.editor.window);
if(!sel||sel.rangeCount==0){
return;
}
var _19=sel.getRangeAt(0),_1a;
if(_19.startContainer===this.editor.editNode&&!_19.startContainer.hasChildNodes()){
_1a=_19.startContainer;
}else{
var _1b=_19.startContainer,_1c=_19.startOffset;
if(this._isBlockElement(_1b)){
while(_1b.hasChildNodes()){
if(_1c==_1b.childNodes.length){
_1c--;
}
_1b=_1b.childNodes[_1c];
_1c=0;
}
}
_1a=this._getBlockAncestor(_1b);
}
var _1d=_b.get(_1a,"direction");
this.buttonLtr.set("checked","ltr"==_1d);
this.buttonRtl.set("checked","rtl"==_1d);
},setEditor:function(_1e){
this.editor=_1e;
if(this.blockMode!="P"&&this.blockMode!="DIV"){
this.blockMode="DIV";
}
this._initButton();
var _1f=this.editor.dir=="ltr";
this.editor.contentPreFilters.push(this._preFilterNewLines);
var _20=_4.hitch(this,function(_21){
if(this.disabled||!_21.hasChildNodes()){
return _21;
}
this._changeStateOfBlocks(this.editor.editNode,this.editor.editNode,this.editor.editNode,"explicitdir",null);
return this.editor.editNode;
});
this.editor.contentDomPostFilters.push(_20);
this.editor._justifyleftImpl=_4.hitch(this,function(){
this._changeState("left");
return true;
});
this.editor._justifyrightImpl=_4.hitch(this,function(){
this._changeState("right");
return true;
});
this.editor._justifycenterImpl=_4.hitch(this,function(){
this._changeState("center");
return true;
});
this.editor._insertorderedlistImpl=_4.hitch(this,"_insertLists","insertorderedlist");
this.editor._insertunorderedlistImpl=_4.hitch(this,"_insertLists","insertunorderedlist");
this.editor._indentImpl=_4.hitch(this,"_indentAndOutdent","indent");
this.editor._outdentImpl=_4.hitch(this,"_indentAndOutdent","outdent");
this.editor._formatblockImpl=_4.hitch(this,"_formatBlocks");
this.editor.onLoadDeferred.addCallback(_4.hitch(this,function(){
var _22=this.editor._plugins,i,p,ind=_22.length,f=false,h=_4.hitch(this,"_changeState","mirror"),hl=_4.hitch(this,"_changeState","ltr"),hr=_4.hitch(this,"_changeState","rtl");
this.editor.addKeyHandler("9",1,0,h);
this.editor.addKeyHandler("8",1,0,hl);
this.editor.addKeyHandler("0",1,0,hr);
for(i=0;i<_22.length;i++){
p=_22[i];
if(!p){
continue;
}
if(p.constructor===_12){
p.destroy();
p=null;
ind=i;
}else{
if(p.constructor===_14){
this.editor._normalizeIndentOutdent=true;
this.editor._indentImpl=_4.hitch(this,"_indentAndOutdent","indent");
this.editor._outdentImpl=_4.hitch(this,"_indentAndOutdent","outdent");
}else{
if(p.constructor===_13&&p.command==="formatBlock"){
this.own(_3.before(p.button,"_execCommand",_4.hitch(this,"_handleNoFormat")));
}
}
}
}
this.editor.addPlugin({ctor:_12,blockNodeForEnter:this.blockMode,blockNodes:/^(?:P|H1|H2|H3|H4|H5|H6|LI|DIV)$/},ind);
p=this.editor._plugins[ind];
this.own(_3.after(p,"handleEnterKey",_4.hitch(this,"_checkNewLine"),true));
}));
this.own(_3.after(this.editor,"onNormalizedDisplayChanged",_4.hitch(this,"updateState"),true));
},_checkNewLine:function(){
var _23=_11.getSelection(this.editor.window).getRangeAt(0);
var _24=_11.getBlockAncestor(_23.startContainer,null,this.editor.editNode).blockNode;
if(_24.innerHTML===this.bogusHtmlContent&&_24.previousSibling){
_24.style.cssText=_24.previousSibling.style.cssText;
}else{
if(_24.innerHTML!==this.bogusHtmlContent&&_24.previousSibling&&_24.previousSibling.innerHTML===this.bogusHtmlContent){
_24.previousSibling.style.cssText=_24.style.cssText;
}
}
},_handleNoFormat:function(_25,_26,_27){
if(_27==="noFormat"){
return [_25,_26,"DIV"];
}
return arguments;
},_execNativeCmd:function(cmd,arg,_28){
if(this._isSimpleInfo(_28)){
var _29=this.editor.document.execCommand(cmd,false,arg);
if(_c("webkit")){
_d("table",this.editor.editNode).prev().forEach(function(x,ind,arr){
if(this._hasTag(x,"BR")){
x.parentNode.removeChild(x);
}
},this);
}
return _29;
}
var sel=_11.getSelection(this.editor.window);
if(!sel||sel.rangeCount==0){
return false;
}
var _2a=sel.getRangeAt(0),_2b=_2a.cloneRange();
var _2c=_2a.startContainer,_2d=_2a.startOffset,_2e=_2a.endContainer,_2f=_2a.endOffset;
for(var i=0;i<_28.groups.length;i++){
var _30=_28.groups[i];
var _31=_30[_30.length-1].childNodes.length;
_2b.setStart(_30[0],0);
_2b.setEnd(_30[_30.length-1],_31);
sel.removeAllRanges();
sel.addRange(_2b);
var _32=this.editor.selection.getParentOfType(_30[0],["TABLE"]);
var _33=this.editor.document.execCommand(cmd,false,arg);
if(_c("webkit")){
if(_32&&this._hasTag(_32.previousSibling,"BR")){
_32.parentNode.removeChild(_32.previousSibling);
}
this.editor.focus();
sel=_11.getSelection(this.editor.window);
var _34=sel.getRangeAt(0);
if(i==0){
_2c=_34.endContainer;
_2d=_34.endOffset;
}else{
if(i==_28.groups.length-1){
_2e=_34.endContainer;
_2f=_34.endOffset;
}
}
}
if(!_33){
break;
}
if(_c("webkit")){
this._changeState(cmd);
}
}
sel.removeAllRanges();
try{
_2b.setStart(_2c,_2d);
_2b.setEnd(_2e,_2f);
sel.addRange(_2b);
}
catch(e){
}
return true;
},_insertLists:function(cmd){
var _35=this._changeState("preparelists",cmd);
var _36=this._execNativeCmd(cmd,null,_35);
if(!_36){
return false;
}
if(!_c("webkit")||this._isSimpleInfo(_35)){
this._changeState(cmd);
}
this._cleanLists();
this._mergeLists();
return true;
},_indentAndOutdent:function(cmd){
if(this.editor._normalizeIndentOutdent){
this._changeState("normalize"+cmd);
return true;
}
var _37=this._changeState("prepare"+cmd);
if(_c("mozilla")){
var _38;
try{
_38=this.editor.document.queryCommandValue("styleWithCSS");
}
catch(e){
_38=false;
}
this.editor.document.execCommand("styleWithCSS",false,true);
}
var _39=this._execNativeCmd(cmd,null,_37);
if(_c("mozilla")){
this.editor.document.execCommand("styleWithCSS",false,_38);
}
if(!_39){
return false;
}
this._changeState(cmd);
this._mergeLists();
return true;
},_formatBlocks:function(arg){
var _3a;
if(_c("mozilla")||_c("webkit")){
_3a=this._changeState("prepareformat",arg);
}
if(_c("ie")&&arg&&arg.charAt(0)!="<"){
arg="<"+arg+">";
}
var _3b=this._execNativeCmd("formatblock",arg,_3a);
if(!_3b){
return false;
}
if(!_c("webkit")||this._isSimpleInfo(_3a)){
this._changeState("formatblock",arg);
}
this._mergeLists();
return true;
},_changeState:function(cmd,arg){
if(!this.editor.window){
return;
}
this.editor.focus();
var sel=_11.getSelection(this.editor.window);
if(!sel||sel.rangeCount==0){
return;
}
var _3c=sel.getRangeAt(0),_3d=_3c.cloneRange(),_3e,_3f,_40,_41;
_3e=_3c.startContainer;
_40=_3c.startOffset;
_3f=_3c.endContainer;
_41=_3c.endOffset;
var _42=_3e===_3f&&_40==_41;
if(this._isBlockElement(_3e)||this._hasTagFrom(_3e,this._tableContainers)){
while(_3e.hasChildNodes()){
if(_40==_3e.childNodes.length){
_40--;
}
_3e=_3e.childNodes[_40];
_40=0;
}
}
_3d.setStart(_3e,_40);
_3e=this._getClosestBlock(_3e,"start",_3d);
var _43=_11.getBlockAncestor(_3e,/li/i,this.editor.editNode).blockNode;
if(_43&&_43!==_3e){
_3e=_43;
}
_3f=_3d.endContainer;
_41=_3d.endOffset;
if(this._isBlockElement(_3f)||this._hasTagFrom(_3f,this._tableContainers)){
while(_3f.hasChildNodes()){
if(_41==_3f.childNodes.length){
_41--;
}
_3f=_3f.childNodes[_41];
if(_3f.hasChildNodes()){
_41=_3f.childNodes.length;
}else{
if(_3f.nodeType==3&&_3f.nodeValue){
_41=_3f.nodeValue.length;
}else{
_41=0;
}
}
}
}
_3d.setEnd(_3f,_41);
_3f=this._getClosestBlock(_3f,"end",_3d);
_43=_11.getBlockAncestor(_3f,/li/i,this.editor.editNode).blockNode;
if(_43&&_43!==_3f){
_3f=_43;
}
sel=_11.getSelection(this.editor.window,true);
sel.removeAllRanges();
sel.addRange(_3d);
var _44=_11.getCommonAncestor(_3e,_3f);
var _45=this._changeStateOfBlocks(_3e,_3f,_44,cmd,arg,_3d);
if(_42){
_3f=_3d.startContainer;
_41=_3d.startOffset;
_3d.setEnd(_3f,_41);
sel=_11.getSelection(this.editor.window,true);
sel.removeAllRanges();
sel.addRange(_3d);
}
return _45;
},_isBlockElement:function(_46){
if(!_46||_46.nodeType!=1){
return false;
}
var _47=_b.get(_46,"display");
return (_47=="block"||_47=="list-item"||_47=="table-cell");
},_isInlineOrTextElement:function(_48){
return !this._isBlockElement(_48)&&(_48.nodeType==1||_48.nodeType==3||_48.nodeType==8);
},_isElement:function(_49){
return _49&&(_49.nodeType==1||_49.nodeType==3);
},_isBlockWithText:function(_4a){
return _4a!==this.editor.editNode&&this._hasTagFrom(_4a,this._lineTextArray);
},_getBlockAncestor:function(_4b){
while(_4b.parentNode&&!this._isBlockElement(_4b)){
_4b=_4b.parentNode;
}
return _4b;
},_getClosestBlock:function(_4c,_4d,_4e){
if(this._isBlockElement(_4c)){
return _4c;
}
var _4f=_4c.parentNode,_50,_51,_52=false,_53=false;
removeOffset=false;
while(true){
var _54=_4c;
_52=false;
while(true){
if(this._isInlineOrTextElement(_54)){
_50=_54;
if(!_51){
_51=_54;
}
}
_54=_54.previousSibling;
if(!_54){
break;
}else{
if(this._isBlockElement(_54)||this._hasTagFrom(_54,this._blockContainers)||this._hasTag(_54,"BR")){
_52=true;
break;
}else{
if(_54.nodeType==3&&_54.nextSibling.nodeType==3){
_54.nextSibling.nodeValue=_54.nodeValue+_54.nextSibling.nodeValue;
_53=true;
if(_4d=="start"&&_54===_4e.startContainer){
_4e.setStart(_54.nextSibling,0);
}else{
if(_4d=="end"&&(_54===_4e.endContainer||_54.nextSibling===_4e.endContainer)){
_4e.setEnd(_54.nextSibling,_54.nextSibling.nodeValue.length);
}
}
_54=_54.nextSibling;
_54.parentNode.removeChild(_54.previousSibling);
if(!_54.previousSibling){
break;
}
}
}
}
}
_54=_4c;
while(true){
if(this._isInlineOrTextElement(_54)){
if(!_50){
_50=_54;
}
_51=_54;
}
_54=_54.nextSibling;
if(!_54){
break;
}else{
if(this._isBlockElement(_54)||this._hasTagFrom(_54,this._blockContainers)){
_52=true;
break;
}else{
if(this._hasTag(_54,"BR")&&_54.nextSibling&&!(this._isBlockElement(_54.nextSibling)||this._hasTagFrom(_54.nextSibling,this._blockContainers))){
_51=_54;
_52=true;
break;
}else{
if(_54.nodeType==3&&_54.previousSibling.nodeType==3){
_54.previousSibling.nodeValue+=_54.nodeValue;
_53=true;
if(_4d=="start"&&_54===_4e.startContainer){
_4e.setStart(_54.previousSibling,0);
}else{
if(_4d=="end"&&(_54===_4e.endContainer||_54.previousSibling===_4e.endContainer)){
_4e.setEnd(_54.previousSibling,_54.previousSibling.nodeValue.length);
}
}
_54=_54.previousSibling;
_54.parentNode.removeChild(_54.nextSibling);
if(!_54.nextSibling){
break;
}
}
}
}
}
}
if(_52||(this._isBlockElement(_4f)&&!this._isBlockWithText(_4f)&&_50)){
var _55=_4e?_4e.startOffset:0,_56=_4e?_4e.endOffset:0,_57=_4e?_4e.startContainer:null,_58=_4e?_4e.endContainer:null,_59=this._repackInlineElements(_50,_51,_4f),div=_59[_4d=="start"?0:_59.length-1];
if(_4e&&div&&_50===_57&&this._hasTag(_50,"BR")){
_57=div;
_55=0;
if(_51===_50){
_58=_57;
_56=0;
}
}
if(_4e){
_4e.setStart(_57,_55);
_4e.setEnd(_58,_56);
}
return div;
}
if(this._isBlockElement(_4f)){
return _4f;
}
_4c=_4f;
removeOffset=true;
_4f=_4f.parentNode;
_50=_51=null;
}
},_changeStateOfBlocks:function(_5a,_5b,_5c,cmd,arg,_5d){
var _5e=[];
if(_5a===this.editor.editNode){
if(!_5a.hasChildNodes()){
return;
}
if(this._isInlineOrTextElement(_5a.firstChild)){
this._rebuildBlock(_5a);
}
_5a=this._getClosestBlock(_5a.firstChild,"start",null);
}
if(_5b===this.editor.editNode){
if(!_5b.hasChildNodes()){
return;
}
if(this._isInlineOrTextElement(_5b.lastChild)){
this._rebuildBlock(_5b);
}
_5b=this._getClosestBlock(_5b.lastChild,"end",null);
}
var _5f=_5d?_5d.startOffset:0,_60=_5d?_5d.endOffset:0,_61=_5d?_5d.startContainer:null,_62=_5d?_5d.endContainer:null;
var _63=this._collectNodes(_5a,_5b,_5c,_5d,_5e,_61,_5f,_62,_60,cmd);
var _64={nodes:_5e,groups:_63.groups,cells:_63.cells};
cmd=cmd.toString();
switch(cmd){
case "mirror":
case "ltr":
case "rtl":
case "left":
case "right":
case "center":
case "explicitdir":
this._execDirAndAlignment(_64,cmd,arg);
break;
case "preparelists":
this._prepareLists(_64,arg);
break;
case "insertorderedlist":
case "insertunorderedlist":
this._execInsertLists(_64);
break;
case "prepareoutdent":
this._prepareOutdent(_64);
break;
case "prepareindent":
this._prepareIndent(_64);
break;
case "indent":
this._execIndent(_64);
break;
case "outdent":
this._execOutdent(_64);
break;
case "normalizeindent":
this._execNormalizedIndent(_64);
break;
case "normalizeoutdent":
this._execNormalizedOutdent(_64);
break;
case "prepareformat":
this._prepareFormat(_64,arg);
break;
case "formatblock":
this._execFormatBlocks(_64,arg);
break;
default:
console.error("Command "+cmd+" isn't handled");
}
if(_5d){
_5d.setStart(_61,_5f);
_5d.setEnd(_62,_60);
sel=_11.getSelection(this.editor.window,true);
sel.removeAllRanges();
sel.addRange(_5d);
this.editor.onDisplayChanged();
}
return _64;
},_collectNodes:function(_65,_66,_67,_68,_69,_6a,_6b,_6c,_6d,cmd){
var _6e=_65,_6f,_70,_71=_6e.parentNode,_72=[],_73,_74,_75=[],_76=[],_77=[],_78=this.editor.editNode;
var _79=_4.hitch(this,function(x){
_69.push(x);
var _7a=this.editor.selection.getParentOfType(x,["TD"]);
if(_78!==_7a||_c("webkit")&&(cmd==="prepareformat"||cmd==="preparelists")){
if(_76.length){
_75.push(_76);
}
_76=[];
if(_78!=_7a){
_78=_7a;
if(_78){
_77.push(_78);
}
}
}
_76.push(x);
});
this._rebuildBlock(_71);
while(true){
if(this._hasTagFrom(_6e,this._tableContainers)){
if(_6e.firstChild){
_71=_6e;
_6e=_6e.firstChild;
continue;
}
}else{
if(this._isBlockElement(_6e)){
var _7b=_11.getBlockAncestor(_6e,/li/i,this.editor.editNode).blockNode;
if(_7b&&_7b!==_6e){
_6e=_7b;
_71=_6e.parentNode;
continue;
}
if(!this._hasTag(_6e,"LI")){
if(_6e.firstChild){
this._rebuildBlock(_6e);
if(this._isBlockElement(_6e.firstChild)||this._hasTagFrom(_6e.firstChild,this._tableContainers)){
_71=_6e;
_6e=_6e.firstChild;
continue;
}
}
}
if(this._hasTagFrom(_6e,this._lineTextArray)){
_79(_6e);
}
}else{
if(this._isInlineOrTextElement(_6e)&&!this._hasTagFrom(_6e.parentNode,this._tableContainers)){
_73=_6e;
while(_6e){
var _7c=_6e.nextSibling;
if(this._isInlineOrTextElement(_6e)){
_74=_6e;
if(this._hasTag(_6e,"BR")){
if(!(this._isBlockElement(_71)&&_6e===_71.lastChild)){
_72=this._repackInlineElements(_73,_74,_71);
_6e=_72[_72.length-1];
for(var nd=0;nd<_72.length;nd++){
_79(_72[nd]);
}
_73=_74=null;
if(_7c&&this._isInlineOrTextElement(_7c)){
_73=_7c;
}
}
}
}else{
if(this._isBlockElement(_6e)){
break;
}
}
_6e=_7c;
}
if(!_73){
continue;
}
_72=this._repackInlineElements(_73,_74,_71);
_6e=_72[_72.length-1];
for(var nd=0;nd<_72.length;nd++){
_79(_72[nd]);
}
}
}
}
if(_6e===_66){
break;
}
if(_6e.nextSibling){
_6e=_6e.nextSibling;
}else{
if(_71!==_67){
while(!_71.nextSibling){
_6e=_71;
_71=_6e.parentNode;
if(_71===_67){
break;
}
}
if(_71!==_67&&_71.nextSibling){
_6e=_71.nextSibling;
_71=_71.parentNode;
}else{
break;
}
}else{
break;
}
}
}
if(_76.length){
if(_c("webkit")||_78){
_75.push(_76);
}else{
_75.unshift(_76);
}
}
return {groups:_75,cells:_77};
},_execDirAndAlignment:function(_7d,cmd,arg){
switch(cmd){
case "mirror":
case "ltr":
case "rtl":
_2.forEach(_7d.nodes,function(x){
var _7e=_b.getComputedStyle(x),_7f=_7e.direction,_80=_7f=="ltr"?"rtl":"ltr",_81=(cmd!="mirror"?cmd:_80),_82=_7e.textAlign,_83=isNaN(parseInt(_7e.marginLeft))?0:parseInt(_7e.marginLeft),_84=isNaN(parseInt(_7e.marginRight))?0:parseInt(_7e.marginRight);
_5.remove(x,"dir");
_5.remove(x,"align");
_b.set(x,{direction:_81,textAlign:""});
if(this._hasTag(x,"CENTER")){
return;
}
if(_82.indexOf("center")>=0){
_b.set(x,"textAlign","center");
}
if(this._hasTag(x,"LI")){
this._refineLIMargins(x);
var _85=_7f==="rtl"?_84:_83;
var _86=0,_87=x.parentNode,_88;
if(_7f!=_b.get(_87,"direction")){
while(_87!==this.editor.editNode){
if(this._hasTagFrom(_87,["OL","UL"])){
_86++;
}
_87=_87.parentNode;
}
_85-=this._getMargins(_86);
}
var _89=_81=="rtl"?"marginRight":"marginLeft";
var _8a=_b.get(x,_89);
var _8b=isNaN(_8a)?0:parseInt(_8a);
_b.set(x,_89,""+(_8b+_85)+"px");
if(_c("webkit")){
if(_82.indexOf("center")<0){
_b.set(x,"textAlign",(_81=="rtl"?"right":"left"));
}
}else{
if(x.firstChild&&x.firstChild.tagName){
if(this._hasTagFrom(x.firstChild,this._lineStyledTextArray)){
var _7e=_b.getComputedStyle(x),_8c=this._refineAlignment(_7e.direction,_7e.textAlign);
if(_c("mozilla")){
_b.set(x.firstChild,{textAlign:_8c});
}else{
_b.set(x.firstChild,{direction:_81,textAlign:_8c});
}
}
}
}
}else{
if(_81=="rtl"&&_83!=0){
_b.set(x,{marginLeft:"",marginRight:""+_83+"px"});
}else{
if(_81=="ltr"&&_84!=0){
_b.set(x,{marginRight:"",marginLeft:""+_84+"px"});
}
}
}
},this);
_d("table",this.editor.editNode).forEach(function(_8d,idx,_8e){
var dir=cmd;
if(cmd==="mirror"){
dir=_b.get(_8d,"direction")==="ltr"?"rtl":"ltr";
}
var _8f=_d("td",_8d),_90=false,_91=false;
for(var i=0;i<_7d.cells.length;i++){
if(!_90&&_8f[0]===_7d.cells[i]){
_90=true;
}else{
if(_8f[_8f.length-1]===_7d.cells[i]){
_91=true;
break;
}
}
}
if(_90&&_91){
_b.set(_8d,"direction",dir);
for(i=0;i<_8f.length;i++){
_b.set(_8f[i],"direction",dir);
}
}
},this);
break;
case "left":
case "right":
case "center":
_2.forEach(_7d.nodes,function(x){
if(this._hasTag(x,"CENTER")){
return;
}
_5.remove(x,"align");
_b.set(x,"textAlign",cmd);
if(this._hasTag(x,"LI")){
if(x.firstChild&&x.firstChild.tagName){
if(this._hasTagFrom(x.firstChild,this._lineStyledTextArray)){
var _92=_b.getComputedStyle(x),_93=this._refineAlignment(_92.direction,_92.textAlign);
_b.set(x.firstChild,"textAlign",_93);
}
}
}
},this);
break;
case "explicitdir":
_2.forEach(_7d.nodes,function(x){
var _94=_b.getComputedStyle(x),_95=_94.direction;
_5.remove(x,"dir");
_b.set(x,{direction:_95});
},this);
break;
}
},_prepareLists:function(_96,arg){
_2.forEach(_96.nodes,function(x,_97,arr){
if(_c("mozilla")||_c("webkit")){
if(_c("mozilla")){
var _98=this._getParentFrom(x,["TD"]);
if(_98&&_d("div[tempRole]",_98).length==0){
_7.create("div",{innerHTML:"<span tempRole='true'>"+this.bogusHtmlContent+"</span",tempRole:"true"},_98);
}
}
var _99=this._tag(x);
var _9a;
if(_c("webkit")&&this._hasTagFrom(x,this._lineStyledTextArray)||(this._hasTag(x,"LI")&&this._hasStyledTextLineTag(x.firstChild))){
var _9b=this._hasTag(x,"LI")?this._tag(x.firstChild):_99;
if(this._hasTag(x,"LI")){
while(x.firstChild.lastChild){
_7.place(x.firstChild.lastChild,x.firstChild,"after");
}
x.removeChild(x.firstChild);
}
_9a=_7.create("span",{innerHTML:this.bogusHtmlContent,bogusFormat:_9b},x,"first");
}
if(!_c("webkit")&&_99!="DIV"&&_99!="P"&&_99!="LI"){
return;
}
if(_c("webkit")&&this._isListTypeChanged(x,arg)&&x===x.parentNode.lastChild){
_7.create("li",{tempRole:"true"},x,"after");
}
if(_99=="LI"&&x.firstChild&&x.firstChild.tagName){
if(this._hasTagFrom(x.firstChild,this._lineStyledTextArray)){
return;
}
}
var _9c=_b.getComputedStyle(x),_9d=_9c.direction,_9e=_9c.textAlign;
_9e=this._refineAlignment(_9d,_9e);
var val=this._getLIIndent(x);
var _9f=val==0?"":""+val+"px";
if(_c("webkit")&&_99=="LI"){
_b.set(x,"textAlign","");
}
var _a0=_9a?x.firstChild:_7.create("span",{innerHTML:this.bogusHtmlContent},x,"first");
_5.set(_a0,"bogusDir",_9d);
if(_9e!=""){
_5.set(_a0,"bogusAlign",_9e);
}
if(_9f){
_5.set(_a0,"bogusMargin",_9f);
}
}else{
if(_c("ie")){
if(this._hasTag(x,"LI")){
var dir=_b.getComputedStyle(x).direction;
_b.set(x,"marginRight","");
_b.set(x,"marginLeft","");
if(this._getLILevel(x)==1&&!this._isListTypeChanged(x,cmd)){
if(x.firstChild&&this._hasTagFrom(x.firstChild,["P","PRE"])){
_7.create("span",{bogusIEFormat:this._tag(x.firstChild)},x.firstChild,"first");
}
if(this._hasTag(x.firstChild,"PRE")){
var p=_7.create("p",null,x.firstChild,"after");
while(x.firstChild.firstChild){
_7.place(x.firstChild.firstChild,p,"last");
}
p.style.cssText=x.style.cssText;
x.removeChild(x.firstChild);
}
}
}
}
}
},this);
if(_c("webkit")){
_d("table",this.editor.editNode).forEach(function(x,ind,arr){
var _a1=x.nextSibling;
if(_a1&&this._hasTagFrom(_a1,["UL","OL"])){
_7.create("UL",{tempRole:"true"},x,"after");
}
},this);
}
},_execInsertLists:function(_a2){
_2.forEach(_a2.nodes,function(x,_a3){
if(this._hasTag(x,"LI")){
if(x.firstChild&&x.firstChild.tagName){
if(this._hasTagFrom(x.firstChild,this._lineStyledTextArray)){
var _a4=_b.getComputedStyle(x.firstChild),_a5=this._refineAlignment(_a4.direction,_a4.textAlign);
_b.set(x,{direction:_a4.direction,textAlign:_a5});
var _a6=this._getIntStyleValue(x,"marginLeft")+this._getIntStyleValue(x.firstChild,"marginLeft");
var _a7=this._getIntStyleValue(x,"marginRight")+this._getIntStyleValue(x.firstChild,"marginRight");
var _a8=_a6?""+_a6+"px":"";
var _a9=_a7?""+_a7+"px":"";
_b.set(x,{marginLeft:_a8,marginRight:_a9});
_b.set(x.firstChild,{direction:"",textAlign:""});
if(!_c("mozilla")){
_b.set(x.firstChild,{marginLeft:"",marginRight:""});
}
}
}
while(x.childNodes.length>1){
if(!(x.lastChild.nodeType==3&&x.lastChild.previousSibling&&x.lastChild.previousSibling.nodeType==3&&_4.trim(x.lastChild.nodeValue)=="")){
break;
}
x.removeChild(x.lastChild);
}
if(_c("safari")){
if(this._hasTag(x.firstChild,"SPAN")&&_6.contains(x.firstChild,"Apple-style-span")){
var _aa=x.firstChild;
if(this._hasTag(_aa.firstChild,"SPAN")&&_5.has(_aa.firstChild,"bogusFormat")){
while(_aa.lastChild){
_7.place(_aa.lastChild,_aa,"after");
}
x.removeChild(_aa);
}
}
}
}else{
if(this._hasTag(x,"DIV")&&x.childNodes.length==0){
x.parentNode.removeChild(x);
return;
}
}
if(_c("ie")){
if(this._hasTag(x,"P")&&this.blockMode.toUpperCase()=="DIV"){
if(this._hasTag(x.firstChild,"SPAN")&&_5.has(x.firstChild,"bogusIEFormat")){
if(_5.get(x.firstChild,"bogusIEFormat").toUpperCase()==="PRE"){
var pre=_7.create("pre",{innerHTML:x.innerHTML},x,"before");
pre.style.cssText=x.style.cssText;
pre.removeChild(pre.firstChild);
x.parentNode.removeChild(x);
}else{
x.removeChild(x.firstChild);
}
return;
}
var _ab=_7.create("div");
_ab.style.cssText=x.style.cssText;
x.parentNode.insertBefore(_ab,x);
while(x.firstChild){
_ab.appendChild(x.firstChild);
}
x.parentNode.removeChild(x);
}
if(!this._hasTag(x,"LI")){
return;
}
this._refineLIMargins(x);
var div=x.firstChild;
if(!this._hasTag(div,"DIV")){
return;
}
if(!(div===x.lastChild)){
return;
}
var _a4=_b.getComputedStyle(div),dir=_a4.direction,_a5=_a4.textAlign,_ac=_b.getComputedStyle(x).textAlign;
_b.set(x,"direction",dir);
_a5=this._refineAlignment(dir,_a5);
_b.set(x,"textAlign",_a5);
while(div.firstChild){
x.insertBefore(div.firstChild,div);
}
x.removeChild(div);
}else{
if(!this._hasTag(x.firstChild,"SPAN")){
if(this._hasTag(x,"LI")){
this._refineLIMargins(x);
if(_c("mozilla")&&this._hasStyledTextLineTag(x.firstChild)){
this._recountLIMargins(x);
}
}
return;
}
}
var _ad=false;
var _ae=false;
var _af=false;
var _b0=0;
if(_5.has(x.firstChild,"bogusDir")){
_ad=true;
var dir=_5.get(x.firstChild,"bogusDir");
_b.set(x,"direction",dir);
}
if(_5.has(x.firstChild,"bogusAlign")){
_ad=true;
_af=true;
var _a5=_5.get(x.firstChild,"bogusAlign");
_b.set(x,"textAlign",_a5);
var _b1=x.firstChild.nextSibling;
if(this._hasTag(_b1,"SPAN")&&_b.get(_b1,"textAlign")===_a5){
_b.set(_b1,"textAlign","");
if(_b1.style.cssText==""){
while(_b1.lastChild){
_7.place(_b1.lastChild,_b1,"after");
}
x.removeChild(_b1);
}
}
}
if(_5.has(x.firstChild,"bogusMargin")){
_ad=true;
_ae=true;
_b0=parseInt(_5.get(x.firstChild,"bogusMargin"));
if(!this._hasTag(x,"LI")){
var _b2=_b.get(x,"direction")==="rtl"?"marginRight":"marginLeft";
var _b3=this._getIntStyleValue(x,_b2)+_b0;
_b.set(x,_b2,(_b3==0?"":""+_b3+"px"));
}
}
if(_5.has(x.firstChild,"bogusFormat")){
_ad=false;
_5.remove(x.firstChild,"bogusDir");
if(x.firstChild.nextSibling&&this._hasTag(x.firstChild.nextSibling,"SPAN")){
var _b4=x.firstChild.style.cssText.trim().split(";");
var _b5=x.firstChild.nextSibling.style.cssText.trim().split(";");
for(var i=0;i<_b4.length;i++){
if(_b4[i]){
for(var j=0;j<_b5.length;j++){
if(_b4[i].trim()==_b5[j].trim()){
var _a4=_b4[i].trim().split(":")[0];
_b.set(x.firstChild.nextSibling,_a4,"");
break;
}
}
}
}
if(x.firstChild.nextSibling.style.cssText===""){
while(x.firstChild.nextSibling.firstChild){
_7.place(x.firstChild.nextSibling.firstChild,x.firstChild.nextSibling,"after");
}
x.removeChild(x.firstChild.nextSibling);
}
}
var tag=_5.get(x.firstChild,"bogusFormat");
var _b6=_7.create(tag,null,x.firstChild,"after");
while(_b6.nextSibling){
_7.place(_b6.nextSibling,_b6,"last");
}
x.removeChild(x.firstChild);
if(_c("webkit")){
if(this._hasTag(x,"LI")){
var _b7=x.parentNode.parentNode;
if(this._hasTag(_b7,tag)){
_5.set(_b7,"tempRole","true");
}
}
}
if(x.childNodes.length==1&&!this._hasTag(x,"TD")){
if(!_c("mozilla")&&!this._hasTag(x,"LI")){
_b6.style.cssText=x.style.cssText;
_5.set(x,"tempRole","true");
}else{
if(!this._hasTag(x,"LI")){
_b6.style.cssText=x.style.cssText;
_7.place(_b6,x,"after");
_5.set(x,"tempRole","true");
}
}
}
}
if(_ad){
x.removeChild(x.firstChild);
}
if(this._hasTag(x,"LI")){
if(_c("webkit")&&!_af&&_b.get(x,"textAlign")!="center"){
_b.set(x,"textAlign",(_b.get(x,"direction")=="rtl"?"right":"left"));
}
if(_c("safari")&&this._hasTag(x,"DIV")){
x.innerHTML=x.nextSibling.innerHTML;
x.parentNode.removeChild(x.nextSibling);
}
var _b8=x.parentNode.parentNode;
if(_b8!==this.editor.editNode&&this._hasTag(_b8,"DIV")){
if(_b8.childNodes.length==1){
_b8.parentNode.insertBefore(x.parentNode,_b8);
_b8.parentNode.removeChild(_b8);
}
}
this._refineLIMargins(x);
if(_ae){
this._recountLIMargins(x,_b0);
}
}
},this);
if(_c("mozilla")){
_d("*[tempRole]",this.editor.editNode).forEach(function(x,_b9,arr){
if(this._hasTag(x,"SPAN")){
if(_5.get(x.parentNode,"tempRole")){
return;
}else{
if(this._hasTag(x.parentNode,"LI")){
x.parentNode.parentNode.removeChild(x.parentNode);
return;
}
}
}
x.parentNode.removeChild(x);
},this);
}else{
if(_c("webkit")){
_d("*[tempRole]",this.editor.editNode).forEach(function(x,_ba,arr){
if(this._hasTag(x,"LI")||this._hasTag(x,"UL")){
return;
}
while(x.lastChild){
_7.place(x.lastChild,x,"after");
}
x.parentNode.removeChild(x);
},this);
}
}
},_execNormalizedIndent:function(_bb){
_2.forEach(_bb.nodes,function(x){
var _bc=_b.get(x,"direction")==="rtl"?"marginRight":"marginLeft";
var _bd=_b.get(x,_bc);
var _be=isNaN(_bd)?0:parseInt(_bd);
_b.set(x,_bc,""+(_be+this._indentBy)+"px");
},this);
},_execNormalizedOutdent:function(_bf){
_2.forEach(_bf.nodes,function(x){
var _c0=_b.get(x,"direction")==="rtl"?"marginRight":"marginLeft";
var _c1=_b.get(x,_c0);
var _c2=isNaN(_c1)?0:parseInt(_c1);
var _c3=0;
if(x.tagName.toUpperCase()==="LI"){
var _c4=0,_c5=x.parentNode,_c6;
if(_b.get(x,"direction")!=_b.get(_c5,"direction")){
while(_c5!==this.editor.editNode){
if(this._hasTagFrom(_c5,["OL","UL"])){
_c4++;
}
_c5=_c5.parentNode;
}
_c3=this._getMargins(_c4);
}
}
if(_c2>=this._indentBy+_c3){
_b.set(x,_c0,(_c2==this._indentBy?"":""+(_c2-this._indentBy)+"px"));
}
},this);
},_prepareIndent:function(_c7){
_2.forEach(_c7.nodes,function(x){
if(_c("mozilla")){
var _c8=this._getParentFrom(x,["TD"]);
if(!!_c8&&(_d("div[tempRole]",_c8).length==0)){
_7.create("div",{innerHTML:this.bogusHtmlContent,tempRole:"true"},_c8);
}
if(this._hasTag(x,"LI")){
var _c9=this._getLIIndent(x);
_5.set(x,"tempIndent",_c9);
}
}
if(_c("webkit")&&this._hasTag(x,"LI")&&this._hasStyledTextLineTag(x.firstChild)){
var _ca=this._tag(x.firstChild);
while(x.firstChild.lastChild){
_7.place(x.firstChild.lastChild,x.firstChild,"after");
}
x.removeChild(x.firstChild);
_7.create("span",{innerHTML:this.bogusHtmlContent,bogusFormat:_ca},x,"first");
}
},this);
},_prepareOutdent:function(_cb){
_2.forEach(_cb.nodes,function(x){
if(_c("mozilla")||_c("webkit")){
if(_c("mozilla")){
var _cc=this._getParentFrom(x,["TD"]);
if(!!_cc&&(_d("div[tempRole]",_cc).length==0)){
_7.create("div",{innerHTML:this.bogusHtmlContent,tempRole:"true"},_cc);
}
}
var _cd=this._tag(x);
if(_c("mozilla")&&_cd!=="LI"){
return;
}
var _ce=null;
if(_c("webkit")){
if(this._hasTag(x,"LI")&&this._hasStyledTextLineTag(x.firstChild)){
_cd=this._tag(x.firstChild);
var _cf=x.firstChild;
while(_cf.lastChild){
_7.place(_cf.lastChild,_cf,"after");
}
x.removeChild(x.firstChild);
_ce=_7.create("span",{innerHTML:this.bogusHtmlContent,bogusFormat:_cd},x,"first");
}
}
if(x.firstChild&&x.firstChild.tagName){
if(this._hasTagFrom(x.firstChild,this._lineStyledTextArray)){
if(_c("mozilla")){
x.firstChild.style.cssText=x.style.cssText;
var _d0=_b.get(x,"direction")==="rtl"?"marginRight":"marginLeft";
var _d1=this._getLIIndent(x);
if(_d1>0){
_b.set(x.firstChild,_d0,""+_d1+"px");
}
}
return;
}
}
var _d2=_b.getComputedStyle(x),_d3=_d2.direction,_d4=_d2.textAlign;
_d4=this._refineAlignment(_d3,_d4);
if(_c("webkit")&&_cd=="LI"){
_b.set(x,"textAlign","");
}
var _d5=_ce?x.firstChild:_7.create("span",{innerHTML:this.bogusHtmlContent},x,"first");
_5.set(_d5,"bogusDir",_d3);
if(_d4!=""){
_5.set(_d5,"bogusAlign",_d4);
}
if(_c("mozilla")){
var _d1=this._getLIIndent(x);
_5.set(_d5,"bogusIndent",_d1);
}
}
if(_c("ie")){
if(x.tagName.toUpperCase()=="LI"){
_b.set(x,"marginLeft","");
_b.set(x,"marginRight","");
if(this._getLILevel(x)==1){
if(x.firstChild&&this._hasTagFrom(x.firstChild,["P","PRE"])){
_7.create("span",{bogusIEFormat:this._tag(x.firstChild)},x.firstChild,"first");
}
if(this._hasTag(x.firstChild,"PRE")){
var p=_7.create("p",null,x.firstChild,"after");
while(x.firstChild.firstChild){
_7.place(x.firstChild.firstChild,p,"last");
}
p.style.cssText=x.style.cssText;
x.removeChild(x.firstChild);
}
}
}
}
},this);
},_execIndent:function(_d6){
_2.forEach(_d6.nodes,function(x){
if(!_c("mozilla")){
_b.set(x,"margin","");
}
if(this._hasTag(x,"LI")){
var _d7=0;
if(_c("mozilla")&&_5.has(x,"tempIndent")){
_d7=parseInt(_5.get(x,"tempIndent"));
_5.remove(x,"tempIndent");
}
this._refineLIMargins(x);
if(_c("mozilla")){
this._recountLIMargins(x,_d7);
}
}
if(_5.has(x.firstChild,"bogusFormat")){
var tag=_5.get(x.firstChild,"bogusFormat");
var _d8=_7.create(tag,null,x.firstChild,"after");
while(_d8.nextSibling){
_7.place(_d8.nextSibling,_d8,"last");
}
x.removeChild(x.firstChild);
}
if(_c("ie")||_c("webkit")){
var _d9=x.parentNode;
while(_d9!==this.editor.editNode){
_d9=_11.getBlockAncestor(_d9,/blockquote/i,this.editor.editNode).blockNode;
if(!_d9){
break;
}
if(_5.has(_d9,"dir")){
_5.remove(_d9,"dir");
}
_b.set(_d9,"marginLeft","");
_b.set(_d9,"marginRight","");
_b.set(_d9,"margin","");
_d9=_d9.parentNode;
}
}
},this);
if(_c("mozilla")){
_d("div[tempRole]",this.editor.editNode).forEach(function(x,_da,arr){
x.parentNode.removeChild(x);
});
_d("ul,ol",this.editor.editNode).forEach(function(x,_db,arr){
_b.set(x,"marginLeft","");
_b.set(x,"marginRight","");
});
}
},_execOutdent:function(_dc){
_2.forEach(_dc.nodes,function(x){
if(_c("mozilla")||_c("webkit")){
if(!this._hasTag(x.firstChild,"SPAN")){
if(this._hasTag(x,"LI")){
this._refineLIMargins(x);
if(_c("mozilla")&&this._hasStyledTextLineTag(x.firstChild)){
this._recountLIMargins(x);
x.firstChild.style.cssText="";
}
}
return;
}
var _dd=false;
var _de=false;
var _df=0;
if(_5.has(x.firstChild,"bogusDir")){
_dd=true;
var dir=_5.get(x.firstChild,"bogusDir");
_b.set(x,"direction",dir);
}
if(_5.has(x.firstChild,"bogusAlign")){
_dd=true;
var _e0=_5.get(x.firstChild,"bogusAlign");
_b.set(x,"textAlign",_e0);
}
if(_5.has(x.firstChild,"bogusIndent")){
_dd=true;
_df=parseInt(_5.get(x.firstChild,"bogusIndent"));
if(!this._hasTag(x,"LI")){
var _e1=_b.get(x,"direction")==="rtl"?"marginRight":"marginLeft";
var _e2=""+(this._getIntStyleValue(x,_e1)+_df)+"px";
_b.set(x,_e1,_e2);
}
}
if(_5.has(x.firstChild,"bogusFormat")){
_dd=true;
var tag=_5.get(x.firstChild,"bogusFormat");
var _e3=_7.create(tag,null,x.firstChild,"after");
while(_e3.nextSibling){
_7.place(_e3.nextSibling,_e3,"last");
}
if(!this._hasTag(x,"LI")){
_e3.style.cssText=x.style.cssText;
_de=true;
}
}
if(_dd){
x.removeChild(x.firstChild);
if(_de){
while(x.lastChild){
_7.place(x.lastChild,x,"after");
}
_5.set(x,"tempRole","true");
}
}
if(_c("webkit")&&this._hasTag(x,"LI")&&_b.get(x,"textAlign")!="center"){
_b.set(x,"textAlign",(_b.get(x,"direction")=="rtl"?"right":"left"));
}
if(_c("mozilla")&&this._hasTag(x,"LI")){
var _e4=x.parentNode.parentNode;
if(_e4!==this.editor.editNode&&this._hasTag(_e4,"DIV")){
if(_e4.childNodes.length==1){
_e4.parentNode.insertBefore(x.parentNode,_e4);
_e4.parentNode.removeChild(_e4);
}
}
}
}
if(_c("ie")){
if(this._hasTag(x,"P")&&this.blockMode.toUpperCase()=="DIV"){
if(this._hasTag(x.firstChild,"SPAN")&&_5.has(x.firstChild,"bogusIEFormat")){
if(_5.get(x.firstChild,"bogusIEFormat").toUpperCase()==="PRE"){
var pre=_7.create("pre",{innerHTML:x.innerHTML},x,"before");
pre.style.cssText=x.style.cssText;
pre.removeChild(pre.firstChild);
x.parentNode.removeChild(x);
}else{
x.removeChild(x.firstChild);
}
return;
}
var _e5=_7.create("div");
_e5.style.cssText=x.style.cssText;
x.parentNode.insertBefore(_e5,x);
while(x.firstChild){
_e5.appendChild(x.firstChild);
}
x.parentNode.removeChild(x);
}
}
if(this._hasTag(x,"LI")){
this._refineLIMargins(x);
if(_c("mozilla")){
this._recountLIMargins(x,_df);
}
}
},this);
if(_c("mozilla")||_c("webkit")){
_d("div[tempRole]",this.editor.editNode).forEach(function(x,_e6,arr){
x.parentNode.removeChild(x);
});
}
},_prepareFormat:function(_e7,arg){
_2.forEach(_e7.nodes,function(x){
if(_c("mozilla")){
if(this._hasTag(x,"LI")){
if(x.firstChild&&!this._isBlockElement(x.firstChild)){
var div=x.ownerDocument.createElement(arg),_e8=x.firstChild;
x.insertBefore(div,x.firstChild);
while(_e8){
div.appendChild(_e8);
_e8=_e8.nextSibling;
}
}
var _e9=this._getLIIndent(x);
_5.set(x,"tempIndent",_e9);
}
}
if(_c("webkit")){
var _ea;
if(this._hasTag(x,"LI")){
var _eb=arg;
if(this._hasStyledTextLineTag(x.firstChild)){
while(x.firstChild.lastChild){
_7.place(x.firstChild.lastChild,x.firstChild,"after");
}
x.removeChild(x.firstChild);
}
_ea=_7.create("span",{innerHTML:this.bogusHtmlContent,bogusFormat:_eb},x,"first");
}
var _ec=_b.getComputedStyle(x),_ed=_ec.direction,_ee=_ec.textAlign;
_ee=this._refineAlignment(_ed,_ee);
var _ef=_ea?x.firstChild:_7.create("span",{innerHTML:this.bogusHtmlContent},x,"first");
_5.set(_ef,"bogusDir",_ed);
if(_ee!=""){
_5.set(_ef,"bogusAlign",_ee);
}
}
},this);
},_execFormatBlocks:function(_f0,arg){
_2.forEach(_f0.nodes,function(x){
if(this._hasTagFrom(x,this._lineTextArray)){
if(this._hasTag(x.parentNode,"DIV")&&x.parentNode!==this.editor.editNode){
while(x.parentNode.lastChild){
if(!(x.parentNode.lastChild.nodeType==3&&_4.trim(x.parentNode.lastChild.nodeValue)==""||this._hasTag(x.parentNode.lastChild,"BR"))){
break;
}
x.parentNode.removeChild(x.parentNode.lastChild);
}
}
if(this._hasTag(x.parentNode,"DIV")&&x.parentNode!==this.editor.editNode&&x.parentNode.childNodes.length==1){
var div=x.parentNode,_f1=_b.getComputedStyle(div),_f2=this._refineAlignment(_f1.direction,_f1.textAlign);
_b.set(x,{direction:_f1.direction,textAlign:_f2});
var _f3=_f1.direction==="rtl"?"marginRight":"marginLeft";
var _f4=parseInt(_b.get(div,_f3));
if(_f4!=0&&!isNan(_f4)){
_b.set(x,_f3,_f4);
}
div.parentNode.insertBefore(x,div);
div.parentNode.removeChild(div);
}
}
if(this._hasTag(x,"LI")){
var _f5=0;
if(_5.has(x,"tempIndent")){
_f5=parseInt(_5.get(x,"tempIndent"));
_5.remove(x,"tempIndent");
}
this._refineLIMargins(x);
if(_f5){
this._recountLIMargins(x,_f5);
}
while(x.childNodes.length>1){
if(!(x.lastChild.nodeType==3&&_4.trim(x.lastChild.nodeValue)=="")){
break;
}
x.removeChild(x.lastChild);
}
if(this._hasTagFrom(x.firstChild,this._lineStyledTextArray)){
var _f1=_b.getComputedStyle(x),_f2=this._refineAlignment(_f1.direction,_f1.textAlign);
if(!_c("mozilla")&&!(_c("ie")&&this._hasTag(x,"LI"))){
_b.set(x.firstChild,{direction:_f1.direction,textAlign:_f2});
}
}else{
if(this._hasTag(x.firstChild,"DIV")){
var div=x.firstChild;
while(div.firstChild){
x.insertBefore(div.firstChild,div);
}
x.removeChild(div);
}
}
if(_c("ie")&&!this._hasTag(x.firstChild,"P")&&arg==="<p>"){
var p=_7.create("p");
var _f6=this._hasTagFrom(p.nextSibling,this._lineStyledTextArray)?p.nextSibling:x;
while(_f6.firstChild){
_7.place(_f6.firstChild,p,"last");
}
_7.place(p,x,"first");
if(_f6!==x){
x.removeChild(_f6);
}
}
}
if(_c("webkit")){
if(this._hasTag(x,"DIV")){
if(_5.has(x,"tempRole")){
return;
}else{
if(this._hasTag(x.previousSibling,"LI")){
while(x.firstChild){
_7.place(x.firstChild,x.previousSibling,"last");
}
_5.set(x,"tempRole",true);
x=x.previousSibling;
}
}
}
var _f7=false;
if(_5.has(x.firstChild,"bogusDir")){
_f7=true;
var dir=_5.get(x.firstChild,"bogusDir");
_b.set(x,"direction",dir);
}
if(_5.has(x.firstChild,"bogusAlign")){
_f7=true;
var _f2=_5.get(x.firstChild,"bogusAlign");
_b.set(x,"textAlign",_f2);
}
if(_5.has(x.firstChild,"bogusFormat")){
_f7=true;
var tag=_5.get(x.firstChild,"bogusFormat");
var _f6;
if(tag.toUpperCase()!=="DIV"){
_f6=_7.create(tag,null,x.firstChild,"after");
while(_f6.nextSibling){
_7.place(_f6.nextSibling,_f6,"last");
}
}else{
_f6=x;
}
if(_c("safari")&&this._hasTag(x.nextSibling,"DIV")){
while(x.nextSibling.firstChild){
_7.place(x.nextSibling.firstChild,_f6,"last");
}
_5.set(x.nextSibling,"tempRole","true");
}
}
if(_f7){
x.removeChild(x.firstChild);
}
if(tag&&this._hasTag(x,"LI")){
var _f8=x.parentNode.parentNode;
if(this._hasTag(_f8,tag)){
_5.set(_f8,"tempRole","true");
}
}
}
},this);
if(_c("webkit")){
_d("*[tempRole]",this.editor.editNode).forEach(function(x,_f9,arr){
while(x.lastChild){
_7.place(x.lastChild,x,"after");
}
x.parentNode.removeChild(x);
},this);
}
},_rebuildBlock:function(_fa){
var _fb=_fa.firstChild,_fc,_fd;
var _fe=false;
while(_fb){
if(this._isInlineOrTextElement(_fb)&&!this._hasTagFrom(_fb,this._tableContainers)){
_fe=!this._hasTagFrom(_fa,this._lineTextArray);
if(!_fc){
_fc=_fb;
}
_fd=_fb;
}else{
if(this._isBlockElement(_fb)||this._hasTagFrom(_fb,this._tableContainers)){
if(_fc){
this._repackInlineElements(_fc,_fd,_fa);
_fc=null;
}
_fe=true;
}
}
_fb=_fb.nextSibling;
}
if(_fe&&_fc){
this._repackInlineElements(_fc,_fd,_fa);
}
},_repackInlineElements:function(_ff,_100,_101){
var divs=[],div=_101.ownerDocument.createElement(this.blockMode),_102;
var _103=_ff.previousSibling&&_ff.previousSibling.nodeType==1?_ff.previousSibling.style.cssText:_101.style.cssText;
var _104=_101===this.editor.editNode;
divs.push(div);
_ff=_101.replaceChild(div,_ff);
_7.place(_ff,div,"after");
if(_104){
_b.set(div,"direction",_b.get(this.editor.editNode,"direction"));
}else{
div.style.cssText=_103;
}
for(var _105=_ff;_105;){
var _106=_105.nextSibling;
if(this._isInlineOrTextElement(_105)){
if(this._hasTag(_105,"BR")&&_105!==_100){
_102=_101.ownerDocument.createElement(this.blockMode);
divs.push(_102);
_105=_101.replaceChild(_102,_105);
_7.place(_105,_102,"after");
if(_104){
_b.set(_102,"direction",_b.get(this.editor.editNode,"direction"));
}else{
_102.style.cssText=_103;
}
}
if((this._hasTag(_105,"BR")||_105.nodeType==8)&&!div.hasChildNodes()){
div.innerHTML=this.bogusHtmlContent;
}
if(this._hasTag(_105,"BR")&&_c("ie")){
_105.parentNode.removeChild(_105);
}else{
if(_105.nodeType!=8){
div.appendChild(_105);
}else{
_105.parentNode.removeChild(_105);
}
}
if(_105.nodeType==3&&_105.previousSibling&&_105.previousSibling.nodeType==3){
_105.previousSibling.nodeValue+=_105.nodeValue;
_105.parentNode.removeChild(_105);
}
if(_102){
div=_102;
_102=null;
}
}
if(_105===_100){
break;
}
_105=_106;
}
return divs;
},_preFilterNewLines:function(html){
var _107=html.split(/(<\/?pre.*>)/i),_108=false;
for(var i=0;i<_107.length;i++){
if(_107[i].search(/<\/?pre/i)<0&&!_108){
_107[i]=_107[i].replace(/\n/g,"").replace(/\t+/g," ").replace(/^\s+/," ").replace(/\xA0\xA0+$/,"");
}else{
if(_107[i].search(/<\/?pre/i)>=0){
_108=!_108;
}
}
}
return _107.join("");
},_refineAlignment:function(dir,_109){
if(_109.indexOf("left")>=0&&dir=="rtl"){
_109="left";
}else{
if(_109.indexOf("right")>=0&&dir=="ltr"){
_109="right";
}else{
if(_109.indexOf("center")>=0){
_109="center";
}else{
_109="";
}
}
}
return _109;
},_refineLIMargins:function(node){
var _10a=_b.get(node,"direction"),pDir=_b.get(node.parentNode,"direction"),_10b=0,_10c=node.parentNode,name,_10d,offs,val;
if(_c("webkit")){
pDir=_b.get(this.editor.editNode,"direction");
}
while(_10c!==this.editor.editNode){
if(this._hasTagFrom(_10c,["OL","UL"])){
_10b++;
}
_10c=_10c.parentNode;
}
_b.set(node,"marginRight","");
_b.set(node,"marginLeft","");
_10d=_10a=="rtl"?"marginRight":"marginLeft";
offs=this._getMargins(_10b);
val=""+offs+"px";
if(_10a!=pDir){
_b.set(node,_10d,val);
}
},_getMargins:function(_10e){
if(_10e==0){
return 0;
}
var _10f=35;
if(_c("mozilla")){
_10f=45;
}else{
if(_c("ie")){
_10f=25;
}
}
return _10f+(_10e-1)*40;
},_recountLIMargins:function(node,_110){
var _111=_b.get(node,"direction"),pDir=_b.get(node.parentNode,"direction");
var _112=_111=="rtl"?"marginRight":"marginLeft";
var _113=_b.get(node,_112);
var val=(isNaN(parseInt(_113))?0:parseInt(_113))+(_110?_110:0);
if(node.firstChild&&node.firstChild.nodeType==1){
_113=_b.get(node.firstChild,_112);
val+=isNaN(parseInt(_113))?0:parseInt(_113);
_b.set(node.firstChild,{marginLeft:"",marginRight:""});
}
if(_111!=pDir){
val-=this._getMargins(this._getLILevel(node));
}
var _114=this._getListMargins(node);
if(_114){
for(var i=0;i<_114/40;i++){
var _115=_7.create(this._tag(node.parentNode),null,node,"before");
_7.place(node,_115,"last");
}
}
if(_111!=pDir){
val+=this._getMargins(this._getLILevel(node));
}
if(val){
_b.set(node,_112,""+(val)+"px");
}
},_getLILevel:function(node){
var _116=node.parentNode;
var _117=0;
while(this._hasTagFrom(_116,["UL","OL"])){
_117++;
_116=_116.parentNode;
}
return _117;
},_getLIIndent:function(node){
var _118=node.parentNode,_119=_b.get(node,"direction"),pDir=_b.get(_118,"direction"),_11a=_119==="rtl"?"marginRight":"marginLeft";
var _11b=this._getIntStyleValue(node,_11a);
var _11c=_119===pDir?0:this._getMargins(this._getLILevel(node));
return _11b-_11c;
},_getListMargins:function(node){
var _11d=node.parentNode;
var _11e,val=0,_11f;
while(this._hasTagFrom(_11d,["UL","OL"])){
var pDir=_b.get(_11d,"direction");
_11e=pDir=="rtl"?"marginRight":"marginLeft";
_11f=_b.get(_11d,_11e);
val+=isNaN(parseInt(_11f))?0:parseInt(_11f);
_11d=_11d.parentNode;
}
return val;
},_tag:function(node){
return node&&node.tagName&&node.tagName.toUpperCase();
},_hasTag:function(node,tag){
return (node&&tag&&node.tagName&&node.tagName.toUpperCase()===tag.toUpperCase());
},_hasStyledTextLineTag:function(node){
return this._hasTagFrom(node,this._lineStyledTextArray);
},_hasTagFrom:function(node,arr){
return node&&arr&&node.tagName&&_2.indexOf(arr,node.tagName.toUpperCase())>=0;
},_getParentFrom:function(node,arr){
if(!node||!arr||!arr.length){
return null;
}
var x=node;
while(x!==this.editor.editNode){
if(this._hasTagFrom(x,arr)){
return x;
}
x=x.parentNode;
}
return null;
},_isSimpleInfo:function(info){
return !info||info.groups.length<2;
},_isListTypeChanged:function(node,cmd){
if(!this._hasTag(node,"LI")){
return false;
}
var _120=node.parentNode;
return (this._hasTag(_120,"UL")&&cmd==="insertorderedlist"||this._hasTag(_120,"OL")&&cmd==="insertunorderedlist");
},_getIntStyleValue:function(node,_121){
var val=parseInt(_b.get(node,_121));
return isNaN(val)?0:val;
},_mergeLists:function(){
var sel=_11.getSelection(this.editor.window);
var _122=sel&&sel.rangeCount>0;
if(_122){
var _123=sel.getRangeAt(0).cloneRange();
var _124=_123.startContainer,_125=_123.startOffset,_126=_123.endContainer,_127=_123.endOffset;
}
var _128=false;
_d("ul,ol",this.editor.editNode).forEach(function(x,ind,arr){
if(_5.has(x,"tempRole")){
x.parentNode.removeChild(x);
return;
}
var _129=x.nextSibling;
while(this._hasTag(_129,this._tag(x))){
while(_129.firstChild){
_7.place(_129.firstChild,x,"last");
_128=true;
}
_5.set(_129,"tempRole","true");
_129=_129.nextSibling;
}
},this);
if(_122&&_128){
sel.removeAllRanges();
try{
_123.setStart(_124,_125);
_123.setEnd(_126,_127);
sel.addRange(_123);
}
catch(e){
}
}
},_cleanLists:function(){
if(_c("webkit")){
_d("table",this.editor.editNode).forEach(function(x,ind,arr){
var _12a=x.nextSibling;
if(this._hasTag(_12a,"UL")&&_5.get(_12a,"tempRole")==="true"){
_12a.parentNode.removeChild(_12a);
}
},this);
_d("li[tempRole]",this.editor.editNode).forEach(function(x,ind,arr){
if(x.parentNode.childNodes.length==1){
x.parentNode.parentNode.removeChild(x.parentNode);
}else{
x.parentNode.removeChild(x);
}
});
}
var sel=_11.getSelection(this.editor.window);
var _12b=sel&&sel.rangeCount>0;
if(_12b){
var _12c=sel.getRangeAt(0).cloneRange();
var _12d=_12c.startContainer,_12e=_12c.startOffset,_12f=_12c.endContainer,_130=_12c.endOffset;
}
var _131=false;
_d("span[bogusDir]",this.editor.editNode).forEach(function(x,ind,arr){
var node=x.firstChild,_132=node;
if(node.nodeType==1){
while(node){
_132=node.nextSibling;
_7.place(node,x,"after");
_131=true;
node=_132;
}
}
x.parentNode.removeChild(x);
},this);
if(_12b&&_131){
sel.removeAllRanges();
try{
_12c.setStart(_12d,_12e);
_12c.setEnd(_12f,_130);
sel.addRange(_12c);
}
catch(e){
}
}
}});
_10.registry["bidiSupport"]=_10.registry["bidisupport"]=function(args){
return new _16({});
};
return _16;
});
