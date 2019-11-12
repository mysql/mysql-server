//>>built
define("dijit/Editor",["require","dojo/_base/array","dojo/_base/declare","dojo/_base/Deferred","dojo/i18n","dojo/dom-attr","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/event","dojo/keys","dojo/_base/lang","dojo/sniff","dojo/string","dojo/topic","dojo/_base/window","./_base/focus","./_Container","./Toolbar","./ToolbarSeparator","./layout/_LayoutWidget","./form/ToggleButton","./_editor/_Plugin","./_editor/plugins/EnterKeyHandling","./_editor/html","./_editor/range","./_editor/RichText","./main","dojo/i18n!./_editor/nls/commands"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,win,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1a,_1b){
var _1c=_3("dijit.Editor",_1a,{plugins:null,extraPlugins:null,constructor:function(){
if(!_c.isArray(this.plugins)){
this.plugins=["undo","redo","|","cut","copy","paste","|","bold","italic","underline","strikethrough","|","insertOrderedList","insertUnorderedList","indent","outdent","|","justifyLeft","justifyRight","justifyCenter","justifyFull",_17];
}
this._plugins=[];
this._editInterval=this.editActionInterval*1000;
if(_d("ie")||_d("trident")){
this.events.push("onBeforeDeactivate");
this.events.push("onBeforeActivate");
}
},postMixInProperties:function(){
this.setValueDeferred=new _4();
this.inherited(arguments);
},postCreate:function(){
this._steps=this._steps.slice(0);
this._undoedSteps=this._undoedSteps.slice(0);
if(_c.isArray(this.extraPlugins)){
this.plugins=this.plugins.concat(this.extraPlugins);
}
this.inherited(arguments);
this.commands=_5.getLocalization("dijit._editor","commands",this.lang);
if(!this.toolbar){
this.toolbar=new _12({ownerDocument:this.ownerDocument,dir:this.dir,lang:this.lang,"aria-label":this.id});
this.header.appendChild(this.toolbar.domNode);
}
_2.forEach(this.plugins,this.addPlugin,this);
this.setValueDeferred.resolve(true);
_7.add(this.iframe.parentNode,"dijitEditorIFrameContainer");
_7.add(this.iframe,"dijitEditorIFrame");
_6.set(this.iframe,"allowTransparency",true);
if(_d("webkit")){
_9.set(this.domNode,"KhtmlUserSelect","none");
}
this.toolbar.startup();
this.onNormalizedDisplayChanged();
},destroy:function(){
_2.forEach(this._plugins,function(p){
if(p&&p.destroy){
p.destroy();
}
});
this._plugins=[];
this.toolbar.destroyRecursive();
delete this.toolbar;
this.inherited(arguments);
},addPlugin:function(_1d,_1e){
var _1f=_c.isString(_1d)?{name:_1d}:_c.isFunction(_1d)?{ctor:_1d}:_1d;
if(!_1f.setEditor){
var o={"args":_1f,"plugin":null,"editor":this};
if(_1f.name){
if(_16.registry[_1f.name]){
o.plugin=_16.registry[_1f.name](_1f);
}else{
_f.publish(_1b._scopeName+".Editor.getPlugin",o);
}
}
if(!o.plugin){
try{
var pc=_1f.ctor||_c.getObject(_1f.name)||_1(_1f.name);
if(pc){
o.plugin=new pc(_1f);
}
}
catch(e){
throw new Error(this.id+": cannot find plugin ["+_1f.name+"]");
}
}
if(!o.plugin){
throw new Error(this.id+": cannot find plugin ["+_1f.name+"]");
}
_1d=o.plugin;
}
if(arguments.length>1){
this._plugins[_1e]=_1d;
}else{
this._plugins.push(_1d);
}
_1d.setEditor(this);
if(_c.isFunction(_1d.setToolbar)){
_1d.setToolbar(this.toolbar);
}
},resize:function(_20){
if(_20){
_14.prototype.resize.apply(this,arguments);
}
},layout:function(){
var _21=(this._contentBox.h-(this.getHeaderHeight()+this.getFooterHeight()+_8.getPadBorderExtents(this.iframe.parentNode).h+_8.getMarginExtents(this.iframe.parentNode).h));
this.editingArea.style.height=_21+"px";
if(this.iframe){
this.iframe.style.height="100%";
}
this._layoutMode=true;
},_onIEMouseDown:function(e){
var _22;
var b=this.document.body;
var _23=b.clientWidth;
var _24=b.clientHeight;
var _25=b.clientLeft;
var _26=b.offsetWidth;
var _27=b.offsetHeight;
var _28=b.offsetLeft;
if(/^rtl$/i.test(b.dir||"")){
if(_23<_26&&e.x>_23&&e.x<_26){
_22=true;
}
}else{
if(e.x<_25&&e.x>_28){
_22=true;
}
}
if(!_22){
if(_24<_27&&e.y>_24&&e.y<_27){
_22=true;
}
}
if(!_22){
delete this._cursorToStart;
delete this._savedSelection;
if(e.target.tagName=="BODY"){
this.defer("placeCursorAtEnd");
}
this.inherited(arguments);
}
},onBeforeActivate:function(){
this._restoreSelection();
},onBeforeDeactivate:function(e){
if(this.customUndo){
this.endEditing(true);
}
if(e.target.tagName!="BODY"){
this._saveSelection();
}
},customUndo:true,editActionInterval:3,beginEditing:function(cmd){
if(!this._inEditing){
this._inEditing=true;
this._beginEditing(cmd);
}
if(this.editActionInterval>0){
if(this._editTimer){
this._editTimer.remove();
}
this._editTimer=this.defer("endEditing",this._editInterval);
}
},_steps:[],_undoedSteps:[],execCommand:function(cmd){
if(this.customUndo&&(cmd=="undo"||cmd=="redo")){
return this[cmd]();
}else{
if(this.customUndo){
this.endEditing();
this._beginEditing();
}
var r=this.inherited(arguments);
if(this.customUndo){
this._endEditing();
}
return r;
}
},_pasteImpl:function(){
return this._clipboardCommand("paste");
},_cutImpl:function(){
return this._clipboardCommand("cut");
},_copyImpl:function(){
return this._clipboardCommand("copy");
},_clipboardCommand:function(cmd){
var r;
try{
r=this.document.execCommand(cmd,false,null);
if(_d("webkit")&&!r){
throw {};
}
}
catch(e){
var sub=_e.substitute,_29={cut:"X",copy:"C",paste:"V"};
alert(sub(this.commands.systemShortcut,[this.commands[cmd],sub(this.commands[_d("mac")?"appleKey":"ctrlKey"],[_29[cmd]])]));
r=false;
}
return r;
},queryCommandEnabled:function(cmd){
if(this.customUndo&&(cmd=="undo"||cmd=="redo")){
return cmd=="undo"?(this._steps.length>1):(this._undoedSteps.length>0);
}else{
return this.inherited(arguments);
}
},_moveToBookmark:function(b){
var _2a=b.mark;
var _2b=b.mark;
var col=b.isCollapsed;
var r,_2c,_2d,sel;
if(_2b){
if(_d("ie")<9){
if(_c.isArray(_2b)){
_2a=[];
_2.forEach(_2b,function(n){
_2a.push(_19.getNode(n,this.editNode));
},this);
win.withGlobal(this.window,"moveToBookmark",_10,[{mark:_2a,isCollapsed:col}]);
}else{
if(_2b.startContainer&&_2b.endContainer){
sel=_19.getSelection(this.window);
if(sel&&sel.removeAllRanges){
sel.removeAllRanges();
r=_19.create(this.window);
_2c=_19.getNode(_2b.startContainer,this.editNode);
_2d=_19.getNode(_2b.endContainer,this.editNode);
if(_2c&&_2d){
r.setStart(_2c,_2b.startOffset);
r.setEnd(_2d,_2b.endOffset);
sel.addRange(r);
}
}
}
}
}else{
sel=_19.getSelection(this.window);
if(sel&&sel.removeAllRanges){
sel.removeAllRanges();
r=_19.create(this.window);
_2c=_19.getNode(_2b.startContainer,this.editNode);
_2d=_19.getNode(_2b.endContainer,this.editNode);
if(_2c&&_2d){
r.setStart(_2c,_2b.startOffset);
r.setEnd(_2d,_2b.endOffset);
sel.addRange(r);
}
}
}
}
},_changeToStep:function(_2e,to){
this.setValue(to.text);
var b=to.bookmark;
if(!b){
return;
}
this._moveToBookmark(b);
},undo:function(){
var ret=false;
if(!this._undoRedoActive){
this._undoRedoActive=true;
this.endEditing(true);
var s=this._steps.pop();
if(s&&this._steps.length>0){
this.focus();
this._changeToStep(s,this._steps[this._steps.length-1]);
this._undoedSteps.push(s);
this.onDisplayChanged();
delete this._undoRedoActive;
ret=true;
}
delete this._undoRedoActive;
}
return ret;
},redo:function(){
var ret=false;
if(!this._undoRedoActive){
this._undoRedoActive=true;
this.endEditing(true);
var s=this._undoedSteps.pop();
if(s&&this._steps.length>0){
this.focus();
this._changeToStep(this._steps[this._steps.length-1],s);
this._steps.push(s);
this.onDisplayChanged();
ret=true;
}
delete this._undoRedoActive;
}
return ret;
},endEditing:function(_2f){
if(this._editTimer){
this._editTimer=this._editTimer.remove();
}
if(this._inEditing){
this._endEditing(_2f);
this._inEditing=false;
}
},_getBookmark:function(){
var b=win.withGlobal(this.window,_10.getBookmark);
var tmp=[];
if(b&&b.mark){
var _30=b.mark;
if(_d("ie")<9){
var sel=_19.getSelection(this.window);
if(!_c.isArray(_30)){
if(sel){
var _31;
if(sel.rangeCount){
_31=sel.getRangeAt(0);
}
if(_31){
b.mark=_31.cloneRange();
}else{
b.mark=win.withGlobal(this.window,_10.getBookmark);
}
}
}else{
_2.forEach(b.mark,function(n){
tmp.push(_19.getIndex(n,this.editNode).o);
},this);
b.mark=tmp;
}
}
try{
if(b.mark&&b.mark.startContainer){
tmp=_19.getIndex(b.mark.startContainer,this.editNode).o;
b.mark={startContainer:tmp,startOffset:b.mark.startOffset,endContainer:b.mark.endContainer===b.mark.startContainer?tmp:_19.getIndex(b.mark.endContainer,this.editNode).o,endOffset:b.mark.endOffset};
}
}
catch(e){
b.mark=null;
}
}
return b;
},_beginEditing:function(){
if(this._steps.length===0){
this._steps.push({"text":_18.getChildrenHtml(this.editNode),"bookmark":this._getBookmark()});
}
},_endEditing:function(){
var v=_18.getChildrenHtml(this.editNode);
this._undoedSteps=[];
this._steps.push({text:v,bookmark:this._getBookmark()});
},onKeyDown:function(e){
if(!_d("ie")&&!this.iframe&&e.keyCode==_b.TAB&&!this.tabIndent){
this._saveSelection();
}
if(!this.customUndo){
this.inherited(arguments);
return;
}
var k=e.keyCode;
if(e.ctrlKey&&!e.altKey){
if(k==90||k==122){
_a.stop(e);
this.undo();
return;
}else{
if(k==89||k==121){
_a.stop(e);
this.redo();
return;
}
}
}
this.inherited(arguments);
switch(k){
case _b.ENTER:
case _b.BACKSPACE:
case _b.DELETE:
this.beginEditing();
break;
case 88:
case 86:
if(e.ctrlKey&&!e.altKey&&!e.metaKey){
this.endEditing();
if(e.keyCode==88){
this.beginEditing("cut");
}else{
this.beginEditing("paste");
}
this.defer("endEditing",1);
break;
}
default:
if(!e.ctrlKey&&!e.altKey&&!e.metaKey&&(e.keyCode<_b.F1||e.keyCode>_b.F15)){
this.beginEditing();
break;
}
case _b.ALT:
this.endEditing();
break;
case _b.UP_ARROW:
case _b.DOWN_ARROW:
case _b.LEFT_ARROW:
case _b.RIGHT_ARROW:
case _b.HOME:
case _b.END:
case _b.PAGE_UP:
case _b.PAGE_DOWN:
this.endEditing(true);
break;
case _b.CTRL:
case _b.SHIFT:
case _b.TAB:
break;
}
},_onBlur:function(){
this.inherited(arguments);
this.endEditing(true);
},_saveSelection:function(){
try{
this._savedSelection=this._getBookmark();
}
catch(e){
}
},_restoreSelection:function(){
if(this._savedSelection){
delete this._cursorToStart;
if(win.withGlobal(this.window,"isCollapsed",_10)){
this._moveToBookmark(this._savedSelection);
}
delete this._savedSelection;
}
},onClick:function(){
this.endEditing(true);
this.inherited(arguments);
},replaceValue:function(_32){
if(!this.customUndo){
this.inherited(arguments);
}else{
if(this.isClosed){
this.setValue(_32);
}else{
this.beginEditing();
if(!_32){
_32="&#160;";
}
this.setValue(_32);
this.endEditing();
}
}
},_setDisabledAttr:function(_33){
this.setValueDeferred.then(_c.hitch(this,function(){
if((!this.disabled&&_33)||(!this._buttonEnabledPlugins&&_33)){
_2.forEach(this._plugins,function(p){
p.set("disabled",true);
});
}else{
if(this.disabled&&!_33){
_2.forEach(this._plugins,function(p){
p.set("disabled",false);
});
}
}
}));
this.inherited(arguments);
},_setStateClass:function(){
try{
this.inherited(arguments);
if(this.document&&this.document.body){
_9.set(this.document.body,"color",_9.get(this.iframe,"color"));
}
}
catch(e){
}
}});
function _34(_35){
return new _16({command:_35.name});
};
function _36(_37){
return new _16({buttonClass:_15,command:_37.name});
};
_c.mixin(_16.registry,{"undo":_34,"redo":_34,"cut":_34,"copy":_34,"paste":_34,"insertOrderedList":_34,"insertUnorderedList":_34,"indent":_34,"outdent":_34,"justifyCenter":_34,"justifyFull":_34,"justifyLeft":_34,"justifyRight":_34,"delete":_34,"selectAll":_34,"removeFormat":_34,"unlink":_34,"insertHorizontalRule":_34,"bold":_36,"italic":_36,"underline":_36,"strikethrough":_36,"subscript":_36,"superscript":_36,"|":function(){
return new _16({setEditor:function(_38){
this.editor=_38;
this.button=new _13({ownerDocument:_38.ownerDocument});
}});
}});
return _1c;
});
