//>>built
define("dojox/editor/plugins/SpellCheck",["dojo","dijit","dojo/io/script","dijit/popup","dijit/_Widget","dijit/_Templated","dijit/_editor/_Plugin","dijit/form/TextBox","dijit/form/DropDownButton","dijit/TooltipDialog","dijit/form/MultiSelect","dijit/Menu","dojo/i18n!dojox/editor/plugins/nls/SpellCheck"],function(_1,_2,_3,_4,_5,_6,_7){
_1.experimental("dojox.editor.plugins.SpellCheck");
var _8=_1.declare("dojox.editor.plugins._spellCheckControl",[_5,_6],{widgetsInTemplate:true,templateString:"<table role='presentation' class='dijitEditorSpellCheckTable'>"+"<tr><td colspan='3' class='alignBottom'><label for='${textId}' id='${textId}_label'>${unfound}</label>"+"<div class='dijitEditorSpellCheckBusyIcon' id='${id}_progressIcon'></div></td></tr>"+"<tr>"+"<td class='dijitEditorSpellCheckBox'><input dojoType='dijit.form.TextBox' required='false' intermediateChanges='true' "+"class='dijitEditorSpellCheckBox' dojoAttachPoint='unfoundTextBox' id='${textId}'/></td>"+"<td><button dojoType='dijit.form.Button' class='blockButton' dojoAttachPoint='skipButton'>${skip}</button></td>"+"<td><button dojoType='dijit.form.Button' class='blockButton' dojoAttachPoint='skipAllButton'>${skipAll}</button></td>"+"</tr>"+"<tr>"+"<td class='alignBottom'><label for='${selectId}'>${suggestions}</td></label>"+"<td colspan='2'><button dojoType='dijit.form.Button' class='blockButton' dojoAttachPoint='toDicButton'>${toDic}</button></td>"+"</tr>"+"<tr>"+"<td>"+"<select dojoType='dijit.form.MultiSelect' id='${selectId}' "+"class='dijitEditorSpellCheckBox listHeight' dojoAttachPoint='suggestionSelect'></select>"+"</td>"+"<td colspan='2'>"+"<button dojoType='dijit.form.Button' class='blockButton' dojoAttachPoint='replaceButton'>${replace}</button>"+"<div class='topMargin'><button dojoType='dijit.form.Button' class='blockButton' "+"dojoAttachPoint='replaceAllButton'>${replaceAll}</button><div>"+"</td>"+"</tr>"+"<tr>"+"<td><div class='topMargin'><button dojoType='dijit.form.Button' dojoAttachPoint='cancelButton'>${cancel}</button></div></td>"+"<td></td>"+"<td></td>"+"</tr>"+"</table>",constructor:function(){
this.ignoreChange=false;
this.isChanged=false;
this.isOpen=false;
this.closable=true;
},postMixInProperties:function(){
this.id=_2.getUniqueId(this.declaredClass.replace(/\./g,"_"));
this.textId=this.id+"_textBox";
this.selectId=this.id+"_select";
},postCreate:function(){
var _9=this.suggestionSelect;
_1.removeAttr(_9.domNode,"multiple");
_9.addItems=function(_a){
var _b=this;
var o=null;
if(_a&&_a.length>0){
_1.forEach(_a,function(_c,i){
o=_1.create("option",{innerHTML:_c,value:_c},_b.domNode);
if(i==0){
o.selected=true;
}
});
}
};
_9.removeItems=function(){
_1.empty(this.domNode);
};
_9.deselectAll=function(){
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
},_cancel:function(_d){
if(_d.keyCode==_1.keys.ESCAPE){
this.onCancel();
_1.stopEvent(_d);
}
},_enter:function(_e){
if(_e.keyCode==_1.keys.ENTER){
this.onEnter();
_1.stopEvent(_e);
}
},_unfoundTextBoxChange:function(){
var id=this.textId+"_label";
if(!this.ignoreChange){
_1.byId(id).innerHTML=this["replaceWith"];
this.isChanged=true;
this.suggestionSelect.deselectAll();
}else{
_1.byId(id).innerHTML=this["unfound"];
}
},_setUnfoundWordAttr:function(_f){
_f=_f||"";
this.unfoundTextBox.set("value",_f);
},_getUnfoundWordAttr:function(){
return this.unfoundTextBox.get("value");
},_setSuggestionListAttr:function(_10){
var _11=this.suggestionSelect;
_10=_10||[];
_11.removeItems();
_11.addItems(_10);
},_getSelectedWordAttr:function(){
var _12=this.suggestionSelect.getSelected();
if(_12&&_12.length>0){
return _12[0].value;
}else{
return this.unfoundTextBox.get("value");
}
},_setDisabledAttr:function(_13){
this.skipButton.set("disabled",_13);
this.skipAllButton.set("disabled",_13);
this.toDicButton.set("disabled",_13);
this.replaceButton.set("disabled",_13);
this.replaceAllButton.set("disabled",_13);
},_setInProgressAttr:function(_14){
var id=this.id+"_progressIcon";
_1.toggleClass(id,"hidden",!_14);
}});
var _15=_1.declare("dojox.editor.plugins._SpellCheckScriptMultiPart",null,{ACTION_QUERY:"query",ACTION_UPDATE:"update",callbackHandle:"callback",maxBufferLength:100,delimiter:" ",label:"response",_timeout:30000,SEC:1000,constructor:function(){
this.serviceEndPoint="";
this._queue=[];
this.isWorking=false;
this.exArgs=null;
this._counter=0;
},send:function(_16,_17){
var _18=this,dt=this.delimiter,mbl=this.maxBufferLength,_19=this.label,_1a=this.serviceEndPoint,_1b=this.callbackHandle,_1c=this.exArgs,_1d=this._timeout,l=0,r=0;
if(!this._result){
this._result=[];
}
_17=_17||this.ACTION_QUERY;
var _1e=function(){
var _1f=[];
var _20=0;
if(_16&&_16.length>0){
_18.isWorking=true;
var len=_16.length;
do{
l=r+1;
if((r+=mbl)>len){
r=len;
}else{
while(dt&&_16.charAt(r)!=dt&&r<=len){
r++;
}
}
_1f.push({l:l,r:r});
_20++;
}while(r<len);
_1.forEach(_1f,function(_21,_22){
var _23={url:_1a,action:_17,timeout:_1d,callbackParamName:_1b,handle:function(_24,_25){
if(++_18._counter<=this.size&&!(_24 instanceof Error)&&_24[_19]&&_1.isArray(_24[_19])){
var _26=this.offset;
_1.forEach(_24[_19],function(_27){
_27.offset+=_26;
});
_18._result[this.number]=_24[_19];
}
if(_18._counter==this.size){
_18._finalizeCollection(this.action);
_18.isWorking=false;
if(_18._queue.length>0){
(_18._queue.shift())();
}
}
}};
_23.content=_1c?_1.mixin(_1c,{action:_17,content:_16.substring(_21.l-1,_21.r)}):{action:_17,content:_16.substring(_21.l-1,_21.r)};
_23.size=_20;
_23.number=_22;
_23.offset=_21.l-1;
_1.io.script.get(_23);
});
}
};
if(!_18.isWorking){
_1e();
}else{
_18._queue.push(_1e);
}
},_finalizeCollection:function(_28){
var _29=this._result,len=_29.length;
for(var i=0;i<len;i++){
var _2a=_29.shift();
_29=_29.concat(_2a);
}
if(_28==this.ACTION_QUERY){
this.onLoad(_29);
}
this._counter=0;
this._result=[];
},onLoad:function(_2b){
},setWaitingTime:function(_2c){
this._timeout=_2c*this.SEC;
}});
var _2d=_1.declare("dojox.editor.plugins.SpellCheck",[_7],{url:"",bufferLength:100,interactive:false,timeout:30,button:null,_editor:null,exArgs:null,_cursorSpan:"<span class=\"cursorPlaceHolder\"></span>",_cursorSelector:"cursorPlaceHolder",_incorrectWordsSpan:"<span class='incorrectWordPlaceHolder'>${text}</span>",_ignoredIncorrectStyle:{"cursor":"inherit","borderBottom":"none","backgroundColor":"transparent"},_normalIncorrectStyle:{"cursor":"pointer","borderBottom":"1px dotted red","backgroundColor":"yellow"},_highlightedIncorrectStyle:{"borderBottom":"1px dotted red","backgroundColor":"#b3b3ff"},_selector:"incorrectWordPlaceHolder",_maxItemNumber:3,constructor:function(){
this._spanList=[];
this._cache={};
this._enabled=true;
this._iterator=0;
},setEditor:function(_2e){
this._editor=_2e;
this._initButton();
this._setNetwork();
this._connectUp();
},_initButton:function(){
var _2f=this,_30=(this._strings=_1.i18n.getLocalization("dojox.editor.plugins","SpellCheck")),_31=(this._dialog=new _2.TooltipDialog());
_31.set("content",(this._dialogContent=new _8({unfound:_30["unfound"],skip:_30["skip"],skipAll:_30["skipAll"],toDic:_30["toDic"],suggestions:_30["suggestions"],replaceWith:_30["replaceWith"],replace:_30["replace"],replaceAll:_30["replaceAll"],cancel:_30["cancel"]})));
this.button=new _2.form.DropDownButton({label:_30["widgetLabel"],showLabel:false,iconClass:"dijitEditorSpellCheckIcon",dropDown:_31,id:_2.getUniqueId(this.declaredClass.replace(/\./g,"_"))+"_dialogPane",closeDropDown:function(_32){
if(_2f._dialogContent.closable){
_2f._dialogContent.isOpen=false;
if(_1.isIE){
var pos=_2f._iterator,_33=_2f._spanList;
if(pos<_33.length&&pos>=0){
_1.style(_33[pos],_2f._normalIncorrectStyle);
}
}
if(this._opened){
_4.close(this.dropDown);
if(_32){
this.focus();
}
this._opened=false;
this.state="";
}
}
}});
_2f._dialogContent.isOpen=false;
_31.domNode.setAttribute("aria-label",this._strings["widgetLabel"]);
},_setNetwork:function(){
var _34=this.exArgs;
if(!this._service){
var _35=(this._service=new _15());
_35.serviceEndPoint=this.url;
_35.maxBufferLength=this.bufferLength;
_35.setWaitingTime(this.timeout);
if(_34){
delete _34.name;
delete _34.url;
delete _34.interactive;
delete _34.timeout;
_35.exArgs=_34;
}
}
},_connectUp:function(){
var _36=this._editor,_37=this._dialogContent;
this.connect(this.button,"set","_disabled");
this.connect(this._service,"onLoad","_loadData");
this.connect(this._dialog,"onOpen","_openDialog");
this.connect(_36,"onKeyPress","_keyPress");
this.connect(_36,"onLoad","_submitContent");
this.connect(_37,"onSkip","_skip");
this.connect(_37,"onSkipAll","_skipAll");
this.connect(_37,"onAddToDic","_add");
this.connect(_37,"onReplace","_replace");
this.connect(_37,"onReplaceAll","_replaceAll");
this.connect(_37,"onCancel","_cancel");
this.connect(_37,"onEnter","_enter");
_36.contentPostFilters.push(this._spellCheckFilter);
_1.publish(_2._scopeName+".Editor.plugin.SpellCheck.getParser",[this]);
if(!this.parser){
console.error("Can not get the word parser!");
}
},_disabled:function(_38,_39){
if(_38=="disabled"){
if(_39){
this._iterator=0;
this._spanList=[];
}else{
if(this.interactive&&!_39&&this._service){
this._submitContent(true);
}
}
this._enabled=!_39;
}
},_keyPress:function(evt){
if(this.interactive){
var v=118,V=86,cc=evt.charCode;
if(!evt.altKey&&cc==_1.keys.SPACE){
this._submitContent();
}else{
if((evt.ctrlKey&&(cc==v||cc==V))||(!evt.ctrlKey&&evt.charCode)){
this._submitContent(true);
}
}
}
},_loadData:function(_3a){
var _3b=this._cache,_3c=this._editor.get("value"),_3d=this._dialogContent;
this._iterator=0;
_1.forEach(_3a,function(d){
_3b[d.text]=d.suggestion;
_3b[d.text].correct=false;
});
if(this._enabled){
_3d.closable=false;
this._markIncorrectWords(_3c,_3b);
_3d.closable=true;
if(this._dialogContent.isOpen){
this._iterator=-1;
this._skip();
}
}
},_openDialog:function(){
var _3e=this._dialogContent;
_3e.ignoreChange=true;
_3e.set("unfoundWord","");
_3e.set("suggestionList",null);
_3e.set("disabled",true);
_3e.set("inProgress",true);
_3e.isOpen=true;
_3e.closable=false;
this._submitContent();
_3e.closable=true;
},_skip:function(evt,_3f){
var _40=this._dialogContent,_41=this._spanList||[],len=_41.length,_42=this._iterator;
_40.closable=false;
_40.isChanged=false;
_40.ignoreChange=true;
if(!_3f&&_42>=0&&_42<len){
this._skipWord(_42);
}
while(++_42<len&&_41[_42].edited==true){
}
if(_42<len){
this._iterator=_42;
this._populateDialog(_42);
this._selectWord(_42);
}else{
this._iterator=-1;
_40.set("unfoundWord",this._strings["msg"]);
_40.set("suggestionList",null);
_40.set("disabled",true);
_40.set("inProgress",false);
}
setTimeout(function(){
if(_1.isWebKit){
_40.skipButton.focus();
}
_40.focus();
_40.ignoreChange=false;
_40.closable=true;
},0);
},_skipAll:function(){
this._dialogContent.closable=false;
this._skipWordAll(this._iterator);
this._skip();
},_add:function(){
var _43=this._dialogContent;
_43.closable=false;
_43.isOpen=true;
this._addWord(this._iterator,_43.get("unfoundWord"));
this._skip();
},_replace:function(){
var _44=this._dialogContent,_45=this._iterator,_46=_44.get("selectedWord");
_44.closable=false;
this._replaceWord(_45,_46);
this._skip(null,true);
},_replaceAll:function(){
var _47=this._dialogContent,_48=this._spanList,len=_48.length,_49=_48[this._iterator].innerHTML.toLowerCase(),_4a=_47.get("selectedWord");
_47.closable=false;
for(var _4b=0;_4b<len;_4b++){
if(_48[_4b].innerHTML.toLowerCase()==_49){
this._replaceWord(_4b,_4a);
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
},_query:function(_4c){
var _4d=this._service,_4e=this._cache,_4f=this.parser.parseIntoWords(this._html2Text(_4c))||[];
var _50=[];
_1.forEach(_4f,function(_51){
_51=_51.toLowerCase();
if(!_4e[_51]){
_4e[_51]=[];
_4e[_51].correct=true;
_50.push(_51);
}
});
if(_50.length>0){
_4d.send(_50.join(" "));
}else{
if(!_4d.isWorking){
this._loadData([]);
}
}
},_html2Text:function(_52){
var _53=[],_54=false,len=_52?_52.length:0;
for(var i=0;i<len;i++){
if(_52.charAt(i)=="<"){
_54=true;
}
if(_54==true){
_53.push(" ");
}else{
_53.push(_52.charAt(i));
}
if(_52.charAt(i)==">"){
_54=false;
}
}
return _53.join("");
},_getBookmark:function(_55){
var ed=this._editor,cp=this._cursorSpan;
ed.execCommand("inserthtml",cp);
var nv=ed.get("value"),_56=nv.indexOf(cp),i=-1;
while(++i<_56&&_55.charAt(i)==nv.charAt(i)){
}
return i;
},_moveToBookmark:function(){
var ed=this._editor,cps=_1.query("."+this._cursorSelector,ed.document),_57=cps&&cps[0];
if(_57){
ed._sCall("selectElement",[_57]);
ed._sCall("collapse",[true]);
var _58=_57.parentNode;
if(_58){
_58.removeChild(_57);
}
}
},_submitContent:function(_59){
if(_59){
var _5a=this,_5b=3000;
if(this._delayHandler){
clearTimeout(this._delayHandler);
this._delayHandler=null;
}
setTimeout(function(){
_5a._query(_5a._editor.get("value"));
},_5b);
}else{
this._query(this._editor.get("value"));
}
},_populateDialog:function(_5c){
var _5d=this._spanList,_5e=this._cache,_5f=this._dialogContent;
_5f.set("disabled",false);
if(_5c<_5d.length&&_5d.length>0){
var _60=_5d[_5c].innerHTML;
_5f.set("unfoundWord",_60);
_5f.set("suggestionList",_5e[_60.toLowerCase()]);
_5f.set("inProgress",false);
}
},_markIncorrectWords:function(_61,_62){
var _63=this,_64=this.parser,_65=this._editor,_66=this._incorrectWordsSpan,_67=this._normalIncorrectStyle,_68=this._selector,_69=_64.parseIntoWords(this._html2Text(_61).toLowerCase()),_6a=_64.getIndices(),_6b=this._cursorSpan,_6c=this._getBookmark(_61),_6d="<span class='incorrectWordPlaceHolder'>".length,_6e=false,_6f=_61.split(""),_70=null;
for(var i=_69.length-1;i>=0;i--){
var _71=_69[i];
if(_62[_71]&&!_62[_71].correct){
var _72=_6a[i],len=_69[i].length,end=_72+len;
if(end<=_6c&&!_6e){
_6f.splice(_6c,0,_6b);
_6e=true;
}
_6f.splice(_72,len,_1.string.substitute(_66,{text:_61.substring(_72,end)}));
if(_72<_6c&&_6c<end&&!_6e){
var tmp=_6f[_72].split("");
tmp.splice(_6d+_6c-_72,0,_6b);
_6f[_72]=tmp.join("");
_6e=true;
}
}
}
if(!_6e){
_6f.splice(_6c,0,_6b);
_6e=true;
}
_65.set("value",_6f.join(""));
_65._cursorToStart=false;
this._moveToBookmark();
_70=this._spanList=_1.query("."+this._selector,_65.document);
_70.forEach(function(_73,i){
_73.id=_68+i;
});
if(!this.interactive){
delete _67.cursor;
}
_70.style(_67);
if(this.interactive){
if(_63._contextMenu){
_63._contextMenu.uninitialize();
_63._contextMenu=null;
}
_63._contextMenu=new _2.Menu({targetNodeIds:[_65.iframe],bindDomNode:function(_74){
_74=_1.byId(_74);
var cn;
var _75,win;
if(_74.tagName.toLowerCase()=="iframe"){
_75=_74;
win=this._iframeContentWindow(_75);
cn=_1.body(_65.document);
}else{
cn=(_74==_1.body()?_1.doc.documentElement:_74);
}
var _76={node:_74,iframe:_75};
_1.attr(_74,"_dijitMenu"+this.id,this._bindings.push(_76));
var _77=_1.hitch(this,function(cn){
return [_1.connect(cn,this.leftClickToOpen?"onclick":"oncontextmenu",this,function(evt){
var _78=evt.target,_79=_63._strings;
if(_1.hasClass(_78,_68)&&!_78.edited){
_1.stopEvent(evt);
var _7a=_63._maxItemNumber,id=_78.id,_7b=id.substring(_68.length),_7c=_62[_78.innerHTML.toLowerCase()],_7d=_7c.length;
this.destroyDescendants();
if(_7d==0){
this.addChild(new _2.MenuItem({label:_79["iMsg"],disabled:true}));
}else{
for(var i=0;i<_7a&&i<_7d;i++){
this.addChild(new _2.MenuItem({label:_7c[i],onClick:(function(){
var idx=_7b,txt=_7c[i];
return function(){
_63._replaceWord(idx,txt);
_65.focus();
};
})()}));
}
}
this.addChild(new _2.MenuSeparator());
this.addChild(new _2.MenuItem({label:_79["iSkip"],onClick:function(){
_63._skipWord(_7b);
_65.focus();
}}));
this.addChild(new _2.MenuItem({label:_79["iSkipAll"],onClick:function(){
_63._skipWordAll(_7b);
_65.focus();
}}));
this.addChild(new _2.MenuSeparator());
this.addChild(new _2.MenuItem({label:_79["toDic"],onClick:function(){
_63._addWord(_7b);
_65.focus();
}}));
this._scheduleOpen(_78,_75,{x:evt.pageX,y:evt.pageY});
}
}),_1.connect(cn,"onkeydown",this,function(evt){
if(evt.shiftKey&&evt.keyCode==_1.keys.F10){
_1.stopEvent(evt);
this._scheduleOpen(evt.target,_75);
}
})];
});
_76.connects=cn?_77(cn):[];
if(_75){
_76.onloadHandler=_1.hitch(this,function(){
var win=this._iframeContentWindow(_75),cn=_1.body(_65.document);
_76.connects=_77(cn);
});
if(_75.addEventListener){
_75.addEventListener("load",_76.onloadHandler,false);
}else{
_75.attachEvent("onload",_76.onloadHandler);
}
}
}});
}
},_selectWord:function(_7e){
var ed=this._editor,_7f=this._spanList;
if(_7e<_7f.length&&_7f.length>0){
ed._sCall("selectElement",[_7f[_7e]]);
ed._sCall("collapse",[true]);
this._findText(_7f[_7e].innerHTML,false,false);
if(_1.isIE){
_1.style(_7f[_7e],this._highlightedIncorrectStyle);
}
}
},_replaceWord:function(_80,_81){
var _82=this._spanList;
_82[_80].innerHTML=_81;
_1.style(_82[_80],this._ignoredIncorrectStyle);
_82[_80].edited=true;
},_skipWord:function(_83){
var _84=this._spanList;
_1.style(_84[_83],this._ignoredIncorrectStyle);
this._cache[_84[_83].innerHTML.toLowerCase()].correct=true;
_84[_83].edited=true;
},_skipWordAll:function(_85,_86){
var _87=this._spanList,len=_87.length;
_86=_86||_87[_85].innerHTML.toLowerCase();
for(var i=0;i<len;i++){
if(!_87[i].edited&&_87[i].innerHTML.toLowerCase()==_86){
this._skipWord(i);
}
}
},_addWord:function(_88,_89){
var _8a=this._service;
_8a.send(_89||this._spanList[_88].innerHTML.toLowerCase(),_8a.ACTION_UPDATE);
this._skipWordAll(_88,_89);
},_findText:function(txt,_8b,_8c){
var ed=this._editor,win=ed.window,_8d=false;
if(txt){
if(win.find){
_8d=win.find(txt,_8b,_8c,false,false,false,false);
}else{
var doc=ed.document;
if(doc.selection){
this._editor.focus();
var _8e=doc.body.createTextRange();
var _8f=doc.selection?doc.selection.createRange():null;
if(_8f){
if(_8c){
_8e.setEndPoint("EndToStart",_8f);
}else{
_8e.setEndPoint("StartToEnd",_8f);
}
}
var _90=_8b?4:0;
if(_8c){
_90=_90|1;
}
_8d=_8e.findText(txt,_8e.text.length,_90);
if(_8d){
_8e.select();
}
}
}
}
return _8d;
},_spellCheckFilter:function(_91){
var _92=/<span class=["']incorrectWordPlaceHolder["'].*?>(.*?)<\/span>/g;
return _91.replace(_92,"$1");
}});
_2d._SpellCheckControl=_8;
_2d._SpellCheckScriptMultiPart=_15;
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _93=o.args.name.toLowerCase();
if(_93==="spellcheck"){
o.plugin=new _2d({url:("url" in o.args)?o.args.url:"",interactive:("interactive" in o.args)?o.args.interactive:false,bufferLength:("bufferLength" in o.args)?o.args.bufferLength:100,timeout:("timeout" in o.args)?o.args.timeout:30,exArgs:o.args});
}
});
return _2d;
});
