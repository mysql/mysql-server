//>>built
define(["dijit","dojo","dojox","dojo/i18n!dojox/editor/plugins/nls/SpellCheck","dojo/require!dijit/_base/popup,dijit/_Widget,dijit/_Templated,dijit/form/TextBox,dijit/form/DropDownButton,dijit/TooltipDialog,dijit/form/MultiSelect,dojo/io/script,dijit/Menu"],function(_1,_2,_3){
_2.provide("dojox.editor.plugins.SpellCheck");
_2.require("dijit._base.popup");
_2.require("dijit._Widget");
_2.require("dijit._Templated");
_2.require("dijit.form.TextBox");
_2.require("dijit.form.DropDownButton");
_2.require("dijit.TooltipDialog");
_2.require("dijit.form.MultiSelect");
_2.require("dojo.io.script");
_2.require("dijit.Menu");
_2.requireLocalization("dojox.editor.plugins","SpellCheck");
_2.experimental("dojox.editor.plugins.SpellCheck");
_2.declare("dojox.editor.plugins._spellCheckControl",[_1._Widget,_1._Templated],{widgetsInTemplate:true,templateString:"<table class='dijitEditorSpellCheckTable'>"+"<tr><td colspan='3' class='alignBottom'><label for='${textId}' id='${textId}_label'>${unfound}</label>"+"<div class='dijitEditorSpellCheckBusyIcon' id='${id}_progressIcon'></div></td></tr>"+"<tr>"+"<td class='dijitEditorSpellCheckBox'><input dojoType='dijit.form.TextBox' required='false' intermediateChanges='true' "+"class='dijitEditorSpellCheckBox' dojoAttachPoint='unfoundTextBox' id='${textId}'/></td>"+"<td><button dojoType='dijit.form.Button' class='blockButton' dojoAttachPoint='skipButton'>${skip}</button></td>"+"<td><button dojoType='dijit.form.Button' class='blockButton' dojoAttachPoint='skipAllButton'>${skipAll}</button></td>"+"</tr>"+"<tr>"+"<td class='alignBottom'><label for='${selectId}'>${suggestions}</td></label>"+"<td colspan='2'><button dojoType='dijit.form.Button' class='blockButton' dojoAttachPoint='toDicButton'>${toDic}</button></td>"+"</tr>"+"<tr>"+"<td>"+"<select dojoType='dijit.form.MultiSelect' id='${selectId}' "+"class='dijitEditorSpellCheckBox listHeight' dojoAttachPoint='suggestionSelect'></select>"+"</td>"+"<td colspan='2'>"+"<button dojoType='dijit.form.Button' class='blockButton' dojoAttachPoint='replaceButton'>${replace}</button>"+"<div class='topMargin'><button dojoType='dijit.form.Button' class='blockButton' "+"dojoAttachPoint='replaceAllButton'>${replaceAll}</button><div>"+"</td>"+"</tr>"+"<tr>"+"<td><div class='topMargin'><button dojoType='dijit.form.Button' dojoAttachPoint='cancelButton'>${cancel}</button></div></td>"+"<td></td>"+"<td></td>"+"</tr>"+"</table>",constructor:function(){
this.ignoreChange=false;
this.isChanged=false;
this.isOpen=false;
this.closable=true;
},postMixInProperties:function(){
this.id=_1.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.textId=this.id+"_textBox";
this.selectId=this.id+"_select";
},postCreate:function(){
var _4=this.suggestionSelect;
_2.removeAttr(_4.domNode,"multiple");
_4.addItems=function(_5){
var _6=this;
var o=null;
if(_5&&_5.length>0){
_2.forEach(_5,function(_7,i){
o=_2.create("option",{innerHTML:_7,value:_7},_6.domNode);
if(i==0){
o.selected=true;
}
});
}
};
_4.removeItems=function(){
_2.empty(this.domNode);
};
_4.deselectAll=function(){
this.containerNode.selectedIndex=-1;
};
this.connect(this,"onKeyPress","_cancel");
this.connect(this.unfoundTextBox,"onKeyPress","_enter");
this.connect(this.unfoundTextBox,"onChange","_unfoundTextBoxChange");
this.connect(this.suggestionSelect,"onKeyPress","_enter");
this.connect(this.skipButton,"onClick","onSkip");
this.connect(this.skipAllButton,"onClick","onSkipAll");
this.connect(this.toDicButton,"onClick","onAddToDic");
this.connect(this.replaceButton,"onClick","onReplace");
this.connect(this.replaceAllButton,"onClick","onReplaceAll");
this.connect(this.cancelButton,"onClick","onCancel");
},onSkip:function(){
},onSkipAll:function(){
},onAddToDic:function(){
},onReplace:function(){
},onReplaceAll:function(){
},onCancel:function(){
},onEnter:function(){
},focus:function(){
this.unfoundTextBox.focus();
},_cancel:function(_8){
if(_8.keyCode==_2.keys.ESCAPE){
this.onCancel();
_2.stopEvent(_8);
}
},_enter:function(_9){
if(_9.keyCode==_2.keys.ENTER){
this.onEnter();
_2.stopEvent(_9);
}
},_unfoundTextBoxChange:function(){
var id=this.textId+"_label";
if(!this.ignoreChange){
_2.byId(id).innerHTML=this["replaceWith"];
this.isChanged=true;
this.suggestionSelect.deselectAll();
}else{
_2.byId(id).innerHTML=this["unfound"];
}
},_setUnfoundWordAttr:function(_a){
_a=_a||"";
this.unfoundTextBox.set("value",_a);
},_getUnfoundWordAttr:function(){
return this.unfoundTextBox.get("value");
},_setSuggestionListAttr:function(_b){
var _c=this.suggestionSelect;
_b=_b||[];
_c.removeItems();
_c.addItems(_b);
},_getSelectedWordAttr:function(){
var _d=this.suggestionSelect.getSelected();
if(_d&&_d.length>0){
return _d[0].value;
}else{
return this.unfoundTextBox.get("value");
}
},_setDisabledAttr:function(_e){
this.skipButton.set("disabled",_e);
this.skipAllButton.set("disabled",_e);
this.toDicButton.set("disabled",_e);
this.replaceButton.set("disabled",_e);
this.replaceAllButton.set("disabled",_e);
},_setInProgressAttr:function(_f){
var id=this.id+"_progressIcon",cmd=_f?"removeClass":"addClass";
_2[cmd](id,"hidden");
}});
_2.declare("dojox.editor.plugins._SpellCheckScriptMultiPart",null,{ACTION_QUERY:"query",ACTION_UPDATE:"update",callbackHandle:"callback",maxBufferLength:100,delimiter:" ",label:"response",_timeout:30000,SEC:1000,constructor:function(){
this.serviceEndPoint="";
this._queue=[];
this.isWorking=false;
this.exArgs=null;
this._counter=0;
},send:function(_10,_11){
var _12=this,dt=this.delimiter,mbl=this.maxBufferLength,_13=this.label,_14=this.serviceEndPoint,_15=this.callbackHandle,_16=this.exArgs,_17=this._timeout,l=0,r=0;
if(!this._result){
this._result=[];
}
_11=_11||this.ACTION_QUERY;
var _18=function(){
var _19=[];
var _1a=0;
if(_10&&_10.length>0){
_12.isWorking=true;
var len=_10.length;
do{
l=r+1;
if((r+=mbl)>len){
r=len;
}else{
while(dt&&_10.charAt(r)!=dt&&r<=len){
r++;
}
}
_19.push({l:l,r:r});
_1a++;
}while(r<len);
_2.forEach(_19,function(_1b,_1c){
var _1d={url:_14,action:_11,timeout:_17,callbackParamName:_15,handle:function(_1e,_1f){
if(++_12._counter<=this.size&&!(_1e instanceof Error)&&_1e[_13]&&_2.isArray(_1e[_13])){
var _20=this.offset;
_2.forEach(_1e[_13],function(_21){
_21.offset+=_20;
});
_12._result[this.number]=_1e[_13];
}
if(_12._counter==this.size){
_12._finalizeCollection(this.action);
_12.isWorking=false;
if(_12._queue.length>0){
(_12._queue.shift())();
}
}
}};
_1d.content=_16?_2.mixin(_16,{action:_11,content:_10.substring(_1b.l-1,_1b.r)}):{action:_11,content:_10.substring(_1b.l-1,_1b.r)};
_1d.size=_1a;
_1d.number=_1c;
_1d.offset=_1b.l-1;
_2.io.script.get(_1d);
});
}
};
if(!_12.isWorking){
_18();
}else{
_12._queue.push(_18);
}
},_finalizeCollection:function(_22){
var _23=this._result,len=_23.length;
for(var i=0;i<len;i++){
var _24=_23.shift();
_23=_23.concat(_24);
}
if(_22==this.ACTION_QUERY){
this.onLoad(_23);
}
this._counter=0;
this._result=[];
},onLoad:function(_25){
},setWaitingTime:function(_26){
this._timeout=_26*this.SEC;
}});
_2.declare("dojox.editor.plugins.SpellCheck",[_1._editor._Plugin],{url:"",bufferLength:100,interactive:false,timeout:30,button:null,_editor:null,exArgs:null,_cursorSpan:"<span class=\"cursorPlaceHolder\"></span>",_cursorSelector:"cursorPlaceHolder",_incorrectWordsSpan:"<span class='incorrectWordPlaceHolder'>${text}</span>",_ignoredIncorrectStyle:{"cursor":"inherit","borderBottom":"none","backgroundColor":"transparent"},_normalIncorrectStyle:{"cursor":"pointer","borderBottom":"1px dotted red","backgroundColor":"yellow"},_highlightedIncorrectStyle:{"borderBottom":"1px dotted red","backgroundColor":"#b3b3ff"},_selector:"incorrectWordPlaceHolder",_maxItemNumber:3,constructor:function(){
this._spanList=[];
this._cache={};
this._enabled=true;
this._iterator=0;
},setEditor:function(_27){
this._editor=_27;
this._initButton();
this._setNetwork();
this._connectUp();
},_initButton:function(){
var _28=this,_29=(this._strings=_2.i18n.getLocalization("dojox.editor.plugins","SpellCheck")),_2a=(this._dialog=new _1.TooltipDialog());
_2a.set("content",(this._dialogContent=new _3.editor.plugins._spellCheckControl({unfound:_29["unfound"],skip:_29["skip"],skipAll:_29["skipAll"],toDic:_29["toDic"],suggestions:_29["suggestions"],replaceWith:_29["replaceWith"],replace:_29["replace"],replaceAll:_29["replaceAll"],cancel:_29["cancel"]})));
this.button=new _1.form.DropDownButton({label:_29["widgetLabel"],showLabel:false,iconClass:"dijitEditorSpellCheckIcon",dropDown:_2a,id:_1.getUniqueId(this.declaredClass.replace(/\./g,"_"))+"_dialogPane",closeDropDown:function(_2b){
if(_28._dialogContent.closable){
_28._dialogContent.isOpen=false;
if(_2.isIE){
var pos=_28._iterator,_2c=_28._spanList;
if(pos<_2c.length&&pos>=0){
_2.style(_2c[pos],_28._normalIncorrectStyle);
}
}
if(this._opened){
_1.popup.close(this.dropDown);
if(_2b){
this.focus();
}
this._opened=false;
this.state="";
}
}
}});
_28._dialogContent.isOpen=false;
_2a.domNode.setAttribute("aria-label",this._strings["widgetLabel"]);
},_setNetwork:function(){
var _2d=this.exArgs;
if(!this._service){
var _2e=(this._service=new _3.editor.plugins._SpellCheckScriptMultiPart());
_2e.serviceEndPoint=this.url;
_2e.maxBufferLength=this.bufferLength;
_2e.setWaitingTime(this.timeout);
if(_2d){
delete _2d.name;
delete _2d.url;
delete _2d.interactive;
delete _2d.timeout;
_2e.exArgs=_2d;
}
}
},_connectUp:function(){
var _2f=this._editor,_30=this._dialogContent;
this.connect(this.button,"set","_disabled");
this.connect(this._service,"onLoad","_loadData");
this.connect(this._dialog,"onOpen","_openDialog");
this.connect(_2f,"onKeyPress","_keyPress");
this.connect(_2f,"onLoad","_submitContent");
this.connect(_30,"onSkip","_skip");
this.connect(_30,"onSkipAll","_skipAll");
this.connect(_30,"onAddToDic","_add");
this.connect(_30,"onReplace","_replace");
this.connect(_30,"onReplaceAll","_replaceAll");
this.connect(_30,"onCancel","_cancel");
this.connect(_30,"onEnter","_enter");
_2f.contentPostFilters.push(this._spellCheckFilter);
_2.publish(_1._scopeName+".Editor.plugin.SpellCheck.getParser",[this]);
if(!this.parser){
console.error("Can not get the word parser!");
}
},_disabled:function(_31,_32){
if(_31=="disabled"){
if(_32){
this._iterator=0;
this._spanList=[];
}else{
if(this.interactive&&!_32&&this._service){
this._submitContent(true);
}
}
this._enabled=!_32;
}
},_keyPress:function(evt){
if(this.interactive){
var v=118,V=86,cc=evt.charCode;
if(!evt.altKey&&cc==_2.keys.SPACE){
this._submitContent();
}else{
if((evt.ctrlKey&&(cc==v||cc==V))||(!evt.ctrlKey&&evt.charCode)){
this._submitContent(true);
}
}
}
},_loadData:function(_33){
var _34=this._cache,_35=this._editor.get("value"),_36=this._dialogContent;
this._iterator=0;
_2.forEach(_33,function(d){
_34[d.text]=d.suggestion;
_34[d.text].correct=false;
});
if(this._enabled){
_36.closable=false;
this._markIncorrectWords(_35,_34);
_36.closable=true;
if(this._dialogContent.isOpen){
this._iterator=-1;
this._skip();
}
}
},_openDialog:function(){
var _37=this._dialogContent;
_37.ignoreChange=true;
_37.set("unfoundWord","");
_37.set("suggestionList",null);
_37.set("disabled",true);
_37.set("inProgress",true);
_37.isOpen=true;
_37.closable=false;
this._submitContent();
_37.closable=true;
},_skip:function(evt,_38){
var _39=this._dialogContent,_3a=this._spanList||[],len=_3a.length,_3b=this._iterator;
_39.closable=false;
_39.isChanged=false;
_39.ignoreChange=true;
if(!_38&&_3b>=0&&_3b<len){
this._skipWord(_3b);
}
while(++_3b<len&&_3a[_3b].edited==true){
}
if(_3b<len){
this._iterator=_3b;
this._populateDialog(_3b);
this._selectWord(_3b);
}else{
this._iterator=-1;
_39.set("unfoundWord",this._strings["msg"]);
_39.set("suggestionList",null);
_39.set("disabled",true);
_39.set("inProgress",false);
}
setTimeout(function(){
if(_2.isWebKit){
_39.skipButton.focus();
}
_39.focus();
_39.ignoreChange=false;
_39.closable=true;
},0);
},_skipAll:function(){
this._dialogContent.closable=false;
this._skipWordAll(this._iterator);
this._skip();
},_add:function(){
var _3c=this._dialogContent;
_3c.closable=false;
_3c.isOpen=true;
this._addWord(this._iterator,_3c.get("unfoundWord"));
this._skip();
},_replace:function(){
var _3d=this._dialogContent,_3e=this._iterator,_3f=_3d.get("selectedWord");
_3d.closable=false;
this._replaceWord(_3e,_3f);
this._skip(null,true);
},_replaceAll:function(){
var _40=this._dialogContent,_41=this._spanList,len=_41.length,_42=_41[this._iterator].innerHTML.toLowerCase(),_43=_40.get("selectedWord");
_40.closable=false;
for(var _44=0;_44<len;_44++){
if(_41[_44].innerHTML.toLowerCase()==_42){
this._replaceWord(_44,_43);
}
}
this._skip(null,true);
},_cancel:function(){
this._dialogContent.closable=true;
this._editor.focus();
},_enter:function(){
if(this._dialogContent.isChanged){
this._replace();
}else{
this._skip();
}
},_query:function(_45){
var _46=this._service,_47=this._cache,_48=this.parser.parseIntoWords(this._html2Text(_45))||[];
var _49=[];
_2.forEach(_48,function(_4a){
_4a=_4a.toLowerCase();
if(!_47[_4a]){
_47[_4a]=[];
_47[_4a].correct=true;
_49.push(_4a);
}
});
if(_49.length>0){
_46.send(_49.join(" "));
}else{
if(!_46.isWorking){
this._loadData([]);
}
}
},_html2Text:function(_4b){
var _4c=[],_4d=false,len=_4b?_4b.length:0;
for(var i=0;i<len;i++){
if(_4b.charAt(i)=="<"){
_4d=true;
}
if(_4d==true){
_4c.push(" ");
}else{
_4c.push(_4b.charAt(i));
}
if(_4b.charAt(i)==">"){
_4d=false;
}
}
return _4c.join("");
},_getBookmark:function(_4e){
var ed=this._editor,cp=this._cursorSpan;
ed.execCommand("inserthtml",cp);
var nv=ed.get("value"),_4f=nv.indexOf(cp),i=-1;
while(++i<_4f&&_4e.charAt(i)==nv.charAt(i)){
}
return i;
},_moveToBookmark:function(){
var ed=this._editor,cps=_2.withGlobal(ed.window,"query",_2,["."+this._cursorSelector]),_50=cps&&cps[0];
if(_50){
ed._sCall("selectElement",[_50]);
ed._sCall("collapse",[true]);
var _51=_50.parentNode;
if(_51){
_51.removeChild(_50);
}
}
},_submitContent:function(_52){
if(_52){
var _53=this,_54=3000;
if(this._delayHandler){
clearTimeout(this._delayHandler);
this._delayHandler=null;
}
setTimeout(function(){
_53._query(_53._editor.get("value"));
},_54);
}else{
this._query(this._editor.get("value"));
}
},_populateDialog:function(_55){
var _56=this._spanList,_57=this._cache,_58=this._dialogContent;
_58.set("disabled",false);
if(_55<_56.length&&_56.length>0){
var _59=_56[_55].innerHTML;
_58.set("unfoundWord",_59);
_58.set("suggestionList",_57[_59.toLowerCase()]);
_58.set("inProgress",false);
}
},_markIncorrectWords:function(_5a,_5b){
var _5c=this,_5d=this.parser,_5e=this._editor,_5f=this._incorrectWordsSpan,_60=this._normalIncorrectStyle,_61=this._selector,_62=_5d.parseIntoWords(this._html2Text(_5a).toLowerCase()),_63=_5d.getIndices(),_64=this._cursorSpan,_65=this._getBookmark(_5a),_66="<span class='incorrectWordPlaceHolder'>".length,_67=false,_68=_5a.split(""),_69=null;
for(var i=_62.length-1;i>=0;i--){
var _6a=_62[i];
if(_5b[_6a]&&!_5b[_6a].correct){
var _6b=_63[i],len=_62[i].length,end=_6b+len;
if(end<=_65&&!_67){
_68.splice(_65,0,_64);
_67=true;
}
_68.splice(_6b,len,_2.string.substitute(_5f,{text:_5a.substring(_6b,end)}));
if(_6b<_65&&_65<end&&!_67){
var tmp=_68[_6b].split("");
tmp.splice(_66+_65-_6b,0,_64);
_68[_6b]=tmp.join("");
_67=true;
}
}
}
if(!_67){
_68.splice(_65,0,_64);
_67=true;
}
_5e.set("value",_68.join(""));
_5e._cursorToStart=false;
this._moveToBookmark();
_69=this._spanList=_2.withGlobal(_5e.window,"query",_2,["."+this._selector]);
_2.forEach(_69,function(_6c,i){
_6c.id=_61+i;
});
if(!this.interactive){
delete _60.cursor;
}
_69.style(_60);
if(this.interactive){
if(_5c._contextMenu){
_5c._contextMenu.uninitialize();
_5c._contextMenu=null;
}
_5c._contextMenu=new _1.Menu({targetNodeIds:[_5e.iframe],bindDomNode:function(_6d){
_6d=_2.byId(_6d);
var cn;
var _6e,win;
if(_6d.tagName.toLowerCase()=="iframe"){
_6e=_6d;
win=this._iframeContentWindow(_6e);
cn=_2.withGlobal(win,_2.body);
}else{
cn=(_6d==_2.body()?_2.doc.documentElement:_6d);
}
var _6f={node:_6d,iframe:_6e};
_2.attr(_6d,"_dijitMenu"+this.id,this._bindings.push(_6f));
var _70=_2.hitch(this,function(cn){
return [_2.connect(cn,this.leftClickToOpen?"onclick":"oncontextmenu",this,function(evt){
var _71=evt.target,_72=_5c._strings;
if(_2.hasClass(_71,_61)&&!_71.edited){
_2.stopEvent(evt);
var _73=_5c._maxItemNumber,id=_71.id,_74=id.substring(_61.length),_75=_5b[_71.innerHTML.toLowerCase()],_76=_75.length;
this.destroyDescendants();
if(_76==0){
this.addChild(new _1.MenuItem({label:_72["iMsg"],disabled:true}));
}else{
for(var i=0;i<_73&&i<_76;i++){
this.addChild(new _1.MenuItem({label:_75[i],onClick:(function(){
var idx=_74,txt=_75[i];
return function(){
_5c._replaceWord(idx,txt);
_5e.focus();
};
})()}));
}
}
this.addChild(new _1.MenuSeparator());
this.addChild(new _1.MenuItem({label:_72["iSkip"],onClick:function(){
_5c._skipWord(_74);
_5e.focus();
}}));
this.addChild(new _1.MenuItem({label:_72["iSkipAll"],onClick:function(){
_5c._skipWordAll(_74);
_5e.focus();
}}));
this.addChild(new _1.MenuSeparator());
this.addChild(new _1.MenuItem({label:_72["toDic"],onClick:function(){
_5c._addWord(_74);
_5e.focus();
}}));
this._scheduleOpen(_71,_6e,{x:evt.pageX,y:evt.pageY});
}
}),_2.connect(cn,"onkeydown",this,function(evt){
if(evt.shiftKey&&evt.keyCode==_2.keys.F10){
_2.stopEvent(evt);
this._scheduleOpen(evt.target,_6e);
}
})];
});
_6f.connects=cn?_70(cn):[];
if(_6e){
_6f.onloadHandler=_2.hitch(this,function(){
var win=this._iframeContentWindow(_6e);
cn=_2.withGlobal(win,_2.body);
_6f.connects=_70(cn);
});
if(_6e.addEventListener){
_6e.addEventListener("load",_6f.onloadHandler,false);
}else{
_6e.attachEvent("onload",_6f.onloadHandler);
}
}
}});
}
},_selectWord:function(_77){
var _78=this._spanList,win=this._editor.window;
if(_77<_78.length&&_78.length>0){
_2.withGlobal(win,"selectElement",_1._editor.selection,[_78[_77]]);
_2.withGlobal(win,"collapse",_1._editor.selection,[true]);
this._findText(_78[_77].innerHTML,false,false);
if(_2.isIE){
_2.style(_78[_77],this._highlightedIncorrectStyle);
}
}
},_replaceWord:function(_79,_7a){
var _7b=this._spanList;
_7b[_79].innerHTML=_7a;
_2.style(_7b[_79],this._ignoredIncorrectStyle);
_7b[_79].edited=true;
},_skipWord:function(_7c){
var _7d=this._spanList;
_2.style(_7d[_7c],this._ignoredIncorrectStyle);
this._cache[_7d[_7c].innerHTML.toLowerCase()].correct=true;
_7d[_7c].edited=true;
},_skipWordAll:function(_7e,_7f){
var _80=this._spanList,len=_80.length;
_7f=_7f||_80[_7e].innerHTML.toLowerCase();
for(var i=0;i<len;i++){
if(!_80[i].edited&&_80[i].innerHTML.toLowerCase()==_7f){
this._skipWord(i);
}
}
},_addWord:function(_81,_82){
var _83=this._service;
_83.send(_82||this._spanList[_81].innerHTML.toLowerCase(),_83.ACTION_UPDATE);
this._skipWordAll(_81,_82);
},_findText:function(txt,_84,_85){
var ed=this._editor,win=ed.window,_86=false;
if(txt){
if(win.find){
_86=win.find(txt,_84,_85,false,false,false,false);
}else{
var doc=ed.document;
if(doc.selection){
this._editor.focus();
var _87=doc.body.createTextRange();
var _88=doc.selection?doc.selection.createRange():null;
if(_88){
if(_85){
_87.setEndPoint("EndToStart",_88);
}else{
_87.setEndPoint("StartToEnd",_88);
}
}
var _89=_84?4:0;
if(_85){
_89=_89|1;
}
_86=_87.findText(txt,_87.text.length,_89);
if(_86){
_87.select();
}
}
}
}
return _86;
},_spellCheckFilter:function(_8a){
var _8b=/<span class=["']incorrectWordPlaceHolder["'].*?>(.*?)<\/span>/g;
return _8a.replace(_8b,"$1");
}});
_2.subscribe(_1._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _8c=o.args.name.toLowerCase();
if(_8c==="spellcheck"){
o.plugin=new _3.editor.plugins.SpellCheck({url:("url" in o.args)?o.args.url:"",interactive:("interactive" in o.args)?o.args.interactive:false,bufferLength:("bufferLength" in o.args)?o.args.bufferLength:100,timeout:("timeout" in o.args)?o.args.timeout:30,exArgs:o.args});
}
});
});
