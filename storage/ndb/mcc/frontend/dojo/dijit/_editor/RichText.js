//>>built
define("dijit/_editor/RichText",["dojo/_base/array","dojo/_base/config","dojo/_base/declare","dojo/_base/Deferred","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/_base/event","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/on","dojo/query","dojo/ready","dojo/_base/sniff","dojo/topic","dojo/_base/unload","dojo/_base/url","dojo/_base/window","../_Widget","../_CssStateMixin","./selection","./range","./html","../focus",".."],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,on,_f,_10,has,_11,_12,_13,win,_14,_15,_16,_17,_18,_19,_1a){
var _1b=_3("dijit._editor.RichText",[_14,_15],{constructor:function(_1c){
this.contentPreFilters=[];
this.contentPostFilters=[];
this.contentDomPreFilters=[];
this.contentDomPostFilters=[];
this.editingAreaStyleSheets=[];
this.events=[].concat(this.events);
this._keyHandlers={};
if(_1c&&_e.isString(_1c.value)){
this.value=_1c.value;
}
this.onLoadDeferred=new _4();
},baseClass:"dijitEditor",inheritWidth:false,focusOnLoad:false,name:"",styleSheets:"",height:"300px",minHeight:"1em",isClosed:true,isLoaded:false,_SEPARATOR:"@@**%%__RICHTEXTBOUNDRY__%%**@@",_NAME_CONTENT_SEP:"@@**%%:%%**@@",onLoadDeferred:null,isTabIndent:false,disableSpellCheck:false,postCreate:function(){
if("textarea"===this.domNode.tagName.toLowerCase()){
console.warn("RichText should not be used with the TEXTAREA tag.  See dijit._editor.RichText docs.");
}
this.contentPreFilters=[_e.hitch(this,"_preFixUrlAttributes")].concat(this.contentPreFilters);
if(has("mozilla")){
this.contentPreFilters=[this._normalizeFontStyle].concat(this.contentPreFilters);
this.contentPostFilters=[this._removeMozBogus].concat(this.contentPostFilters);
}
if(has("webkit")){
this.contentPreFilters=[this._removeWebkitBogus].concat(this.contentPreFilters);
this.contentPostFilters=[this._removeWebkitBogus].concat(this.contentPostFilters);
}
if(has("ie")){
this.contentPostFilters=[this._normalizeFontStyle].concat(this.contentPostFilters);
this.contentDomPostFilters=[_e.hitch(this,this._stripBreakerNodes)].concat(this.contentDomPostFilters);
}
this.inherited(arguments);
_11.publish(_1a._scopeName+"._editor.RichText::init",this);
this.open();
this.setupDefaultShortcuts();
},setupDefaultShortcuts:function(){
var _1d=_e.hitch(this,function(cmd,arg){
return function(){
return !this.execCommand(cmd,arg);
};
});
var _1e={b:_1d("bold"),i:_1d("italic"),u:_1d("underline"),a:_1d("selectall"),s:function(){
this.save(true);
},m:function(){
this.isTabIndent=!this.isTabIndent;
},"1":_1d("formatblock","h1"),"2":_1d("formatblock","h2"),"3":_1d("formatblock","h3"),"4":_1d("formatblock","h4"),"\\":_1d("insertunorderedlist")};
if(!has("ie")){
_1e.Z=_1d("redo");
}
var key;
for(key in _1e){
this.addKeyHandler(key,true,false,_1e[key]);
}
},events:["onKeyPress","onKeyDown","onKeyUp"],captureEvents:[],_editorCommandsLocalized:false,_localizeEditorCommands:function(){
if(_1b._editorCommandsLocalized){
this._local2NativeFormatNames=_1b._local2NativeFormatNames;
this._native2LocalFormatNames=_1b._native2LocalFormatNames;
return;
}
_1b._editorCommandsLocalized=true;
_1b._local2NativeFormatNames={};
_1b._native2LocalFormatNames={};
this._local2NativeFormatNames=_1b._local2NativeFormatNames;
this._native2LocalFormatNames=_1b._native2LocalFormatNames;
var _1f=["div","p","pre","h1","h2","h3","h4","h5","h6","ol","ul","address"];
var _20="",_21,i=0;
while((_21=_1f[i++])){
if(_21.charAt(1)!=="l"){
_20+="<"+_21+"><span>content</span></"+_21+"><br/>";
}else{
_20+="<"+_21+"><li>content</li></"+_21+"><br/>";
}
}
var _22={position:"absolute",top:"0px",zIndex:10,opacity:0.01};
var div=_8.create("div",{style:_22,innerHTML:_20});
win.body().appendChild(div);
var _23=_e.hitch(this,function(){
var _24=div.firstChild;
while(_24){
try{
_16.selectElement(_24.firstChild);
var _25=_24.tagName.toLowerCase();
this._local2NativeFormatNames[_25]=document.queryCommandValue("formatblock");
this._native2LocalFormatNames[this._local2NativeFormatNames[_25]]=_25;
_24=_24.nextSibling.nextSibling;
}
catch(e){
}
}
div.parentNode.removeChild(div);
div.innerHTML="";
});
setTimeout(_23,0);
},open:function(_26){
if(!this.onLoadDeferred||this.onLoadDeferred.fired>=0){
this.onLoadDeferred=new _4();
}
if(!this.isClosed){
this.close();
}
_11.publish(_1a._scopeName+"._editor.RichText::open",this);
if(arguments.length===1&&_26.nodeName){
this.domNode=_26;
}
var dn=this.domNode;
var _27;
if(_e.isString(this.value)){
_27=this.value;
delete this.value;
dn.innerHTML="";
}else{
if(dn.nodeName&&dn.nodeName.toLowerCase()=="textarea"){
var ta=(this.textarea=dn);
this.name=ta.name;
_27=ta.value;
dn=this.domNode=win.doc.createElement("div");
dn.setAttribute("widgetId",this.id);
ta.removeAttribute("widgetId");
dn.cssText=ta.cssText;
dn.className+=" "+ta.className;
_8.place(dn,ta,"before");
var _28=_e.hitch(this,function(){
_a.set(ta,{display:"block",position:"absolute",top:"-1000px"});
if(has("ie")){
var s=ta.style;
this.__overflow=s.overflow;
s.overflow="hidden";
}
});
if(has("ie")){
setTimeout(_28,10);
}else{
_28();
}
if(ta.form){
var _29=ta.value;
this.reset=function(){
var _2a=this.getValue();
if(_2a!==_29){
this.replaceValue(_29);
}
};
on(ta.form,"submit",_e.hitch(this,function(){
_6.set(ta,"disabled",this.disabled);
ta.value=this.getValue();
}));
}
}else{
_27=_18.getChildrenHtml(dn);
dn.innerHTML="";
}
}
this.value=_27;
if(dn.nodeName&&dn.nodeName==="LI"){
dn.innerHTML=" <br>";
}
this.header=dn.ownerDocument.createElement("div");
dn.appendChild(this.header);
this.editingArea=dn.ownerDocument.createElement("div");
dn.appendChild(this.editingArea);
this.footer=dn.ownerDocument.createElement("div");
dn.appendChild(this.footer);
if(!this.name){
this.name=this.id+"_AUTOGEN";
}
if(this.name!==""&&(!_2["useXDomain"]||_2["allowXdRichTextSave"])){
var _2b=_5.byId(_1a._scopeName+"._editor.RichText.value");
if(_2b&&_2b.value!==""){
var _2c=_2b.value.split(this._SEPARATOR),i=0,dat;
while((dat=_2c[i++])){
var _2d=dat.split(this._NAME_CONTENT_SEP);
if(_2d[0]===this.name){
_27=_2d[1];
_2c=_2c.splice(i,1);
_2b.value=_2c.join(this._SEPARATOR);
break;
}
}
}
if(!_1b._globalSaveHandler){
_1b._globalSaveHandler={};
_12.addOnUnload(function(){
var id;
for(id in _1b._globalSaveHandler){
var f=_1b._globalSaveHandler[id];
if(_e.isFunction(f)){
f();
}
}
});
}
_1b._globalSaveHandler[this.id]=_e.hitch(this,"_saveContent");
}
this.isClosed=false;
var ifr=(this.editorObject=this.iframe=win.doc.createElement("iframe"));
ifr.id=this.id+"_iframe";
this._iframeSrc=this._getIframeDocTxt();
ifr.style.border="none";
ifr.style.width="100%";
if(this._layoutMode){
ifr.style.height="100%";
}else{
if(has("ie")>=7){
if(this.height){
ifr.style.height=this.height;
}
if(this.minHeight){
ifr.style.minHeight=this.minHeight;
}
}else{
ifr.style.height=this.height?this.height:this.minHeight;
}
}
ifr.frameBorder=0;
ifr._loadFunc=_e.hitch(this,function(w){
this.window=w;
this.document=this.window.document;
if(has("ie")){
this._localizeEditorCommands();
}
this.onLoad(_27);
});
var _2e="parent."+_1a._scopeName+".byId(\""+this.id+"\")._iframeSrc";
var s="javascript:(function(){try{return "+_2e+"}catch(e){document.open();document.domain=\""+document.domain+"\";document.write("+_2e+");document.close();}})()";
ifr.setAttribute("src",s);
this.editingArea.appendChild(ifr);
if(has("safari")<=4){
var src=ifr.getAttribute("src");
if(!src||src.indexOf("javascript")===-1){
setTimeout(function(){
ifr.setAttribute("src",s);
},0);
}
}
if(dn.nodeName==="LI"){
dn.lastChild.style.marginTop="-1.2em";
}
_7.add(this.domNode,this.baseClass);
},_local2NativeFormatNames:{},_native2LocalFormatNames:{},_getIframeDocTxt:function(){
var _2f=_a.getComputedStyle(this.domNode);
var _30="";
var _31=true;
if(has("ie")||has("webkit")||(!this.height&&!has("mozilla"))){
_30="<div id='dijitEditorBody'></div>";
_31=false;
}else{
if(has("mozilla")){
this._cursorToStart=true;
_30="&#160;";
}
}
var _32=[_2f.fontWeight,_2f.fontSize,_2f.fontFamily].join(" ");
var _33=_2f.lineHeight;
if(_33.indexOf("px")>=0){
_33=parseFloat(_33)/parseFloat(_2f.fontSize);
}else{
if(_33.indexOf("em")>=0){
_33=parseFloat(_33);
}else{
_33="normal";
}
}
var _34="";
var _35=this;
this.style.replace(/(^|;)\s*(line-|font-?)[^;]+/ig,function(_36){
_36=_36.replace(/^;/ig,"")+";";
var s=_36.split(":")[0];
if(s){
s=_e.trim(s);
s=s.toLowerCase();
var i;
var sC="";
for(i=0;i<s.length;i++){
var c=s.charAt(i);
switch(c){
case "-":
i++;
c=s.charAt(i).toUpperCase();
default:
sC+=c;
}
}
_a.set(_35.domNode,sC,"");
}
_34+=_36+";";
});
var _37=_f("label[for=\""+this.id+"\"]");
return [this.isLeftToRight()?"<html>\n<head>\n":"<html dir='rtl'>\n<head>\n",(has("mozilla")&&_37.length?"<title>"+_37[0].innerHTML+"</title>\n":""),"<meta http-equiv='Content-Type' content='text/html'>\n","<style>\n","\tbody,html {\n","\t\tbackground:transparent;\n","\t\tpadding: 1px 0 0 0;\n","\t\tmargin: -1px 0 0 0;\n",((has("webkit"))?"\t\twidth: 100%;\n":""),((has("webkit"))?"\t\theight: 100%;\n":""),"\t}\n","\tbody{\n","\t\ttop:0px;\n","\t\tleft:0px;\n","\t\tright:0px;\n","\t\tfont:",_32,";\n",((this.height||has("opera"))?"":"\t\tposition: fixed;\n"),"\t\tmin-height:",this.minHeight,";\n","\t\tline-height:",_33,";\n","\t}\n","\tp{ margin: 1em 0; }\n",(!_31&&!this.height?"\tbody,html {overflow-y: hidden;}\n":""),"\t#dijitEditorBody{overflow-x: auto; overflow-y:"+(this.height?"auto;":"hidden;")+" outline: 0px;}\n","\tli > ul:-moz-first-node, li > ol:-moz-first-node{ padding-top: 1.2em; }\n",(!has("ie")?"\tli{ min-height:1.2em; }\n":""),"</style>\n",this._applyEditingAreaStyleSheets(),"\n","</head>\n<body ",(_31?"id='dijitEditorBody' ":""),"onload='frameElement._loadFunc(window,document)' style='"+_34+"'>",_30,"</body>\n</html>"].join("");
},_applyEditingAreaStyleSheets:function(){
var _38=[];
if(this.styleSheets){
_38=this.styleSheets.split(";");
this.styleSheets="";
}
_38=_38.concat(this.editingAreaStyleSheets);
this.editingAreaStyleSheets=[];
var _39="",i=0,url;
while((url=_38[i++])){
var _3a=(new _13(win.global.location,url)).toString();
this.editingAreaStyleSheets.push(_3a);
_39+="<link rel=\"stylesheet\" type=\"text/css\" href=\""+_3a+"\"/>";
}
return _39;
},addStyleSheet:function(uri){
var url=uri.toString();
if(url.charAt(0)==="."||(url.charAt(0)!=="/"&&!uri.host)){
url=(new _13(win.global.location,url)).toString();
}
if(_1.indexOf(this.editingAreaStyleSheets,url)>-1){
return;
}
this.editingAreaStyleSheets.push(url);
this.onLoadDeferred.addCallback(_e.hitch(this,function(){
if(this.document.createStyleSheet){
this.document.createStyleSheet(url);
}else{
var _3b=this.document.getElementsByTagName("head")[0];
var _3c=this.document.createElement("link");
_3c.rel="stylesheet";
_3c.type="text/css";
_3c.href=url;
_3b.appendChild(_3c);
}
}));
},removeStyleSheet:function(uri){
var url=uri.toString();
if(url.charAt(0)==="."||(url.charAt(0)!=="/"&&!uri.host)){
url=(new _13(win.global.location,url)).toString();
}
var _3d=_1.indexOf(this.editingAreaStyleSheets,url);
if(_3d===-1){
return;
}
delete this.editingAreaStyleSheets[_3d];
win.withGlobal(this.window,"query",dojo,["link:[href=\""+url+"\"]"]).orphan();
},disabled:false,_mozSettingProps:{"styleWithCSS":false},_setDisabledAttr:function(_3e){
_3e=!!_3e;
this._set("disabled",_3e);
if(!this.isLoaded){
return;
}
if(has("ie")||has("webkit")||has("opera")){
var _3f=has("ie")&&(this.isLoaded||!this.focusOnLoad);
if(_3f){
this.editNode.unselectable="on";
}
this.editNode.contentEditable=!_3e;
if(_3f){
var _40=this;
setTimeout(function(){
if(_40.editNode){
_40.editNode.unselectable="off";
}
},0);
}
}else{
try{
this.document.designMode=(_3e?"off":"on");
}
catch(e){
return;
}
if(!_3e&&this._mozSettingProps){
var ps=this._mozSettingProps;
var n;
for(n in ps){
if(ps.hasOwnProperty(n)){
try{
this.document.execCommand(n,false,ps[n]);
}
catch(e2){
}
}
}
}
}
this._disabledOK=true;
},onLoad:function(_41){
if(!this.window.__registeredWindow){
this.window.__registeredWindow=true;
this._iframeRegHandle=_19.registerIframe(this.iframe);
}
if(!has("ie")&&!has("webkit")&&(this.height||has("mozilla"))){
this.editNode=this.document.body;
}else{
this.editNode=this.document.body.firstChild;
var _42=this;
if(has("ie")){
this.tabStop=_8.create("div",{tabIndex:-1},this.editingArea);
this.iframe.onfocus=function(){
_42.editNode.setActive();
};
}
}
this.focusNode=this.editNode;
var _43=this.events.concat(this.captureEvents);
var ap=this.iframe?this.document:this.editNode;
_1.forEach(_43,function(_44){
this.connect(ap,_44.toLowerCase(),_44);
},this);
this.connect(ap,"onmouseup","onClick");
if(has("ie")){
this.connect(this.document,"onmousedown","_onIEMouseDown");
this.editNode.style.zoom=1;
}else{
this.connect(this.document,"onmousedown",function(){
delete this._cursorToStart;
});
}
if(has("webkit")){
this._webkitListener=this.connect(this.document,"onmouseup","onDisplayChanged");
this.connect(this.document,"onmousedown",function(e){
var t=e.target;
if(t&&(t===this.document.body||t===this.document)){
setTimeout(_e.hitch(this,"placeCursorAtEnd"),0);
}
});
}
if(has("ie")){
try{
this.document.execCommand("RespectVisibilityInDesign",true,null);
}
catch(e){
}
}
this.isLoaded=true;
this.set("disabled",this.disabled);
var _45=_e.hitch(this,function(){
this.setValue(_41);
if(this.onLoadDeferred){
this.onLoadDeferred.callback(true);
}
this.onDisplayChanged();
if(this.focusOnLoad){
_10(_e.hitch(this,function(){
setTimeout(_e.hitch(this,"focus"),this.updateInterval);
}));
}
this.value=this.getValue(true);
});
if(this.setValueDeferred){
this.setValueDeferred.addCallback(_45);
}else{
_45();
}
},onKeyDown:function(e){
if(e.keyCode===_d.TAB&&this.isTabIndent){
_b.stop(e);
if(this.queryCommandEnabled((e.shiftKey?"outdent":"indent"))){
this.execCommand((e.shiftKey?"outdent":"indent"));
}
}
if(has("ie")){
if(e.keyCode==_d.TAB&&!this.isTabIndent){
if(e.shiftKey&&!e.ctrlKey&&!e.altKey){
this.iframe.focus();
}else{
if(!e.shiftKey&&!e.ctrlKey&&!e.altKey){
this.tabStop.focus();
}
}
}else{
if(e.keyCode===_d.BACKSPACE&&this.document.selection.type==="Control"){
_b.stop(e);
this.execCommand("delete");
}else{
if((65<=e.keyCode&&e.keyCode<=90)||(e.keyCode>=37&&e.keyCode<=40)){
e.charCode=e.keyCode;
this.onKeyPress(e);
}
}
}
}
if(has("ff")){
if(e.keyCode===_d.PAGE_UP||e.keyCode===_d.PAGE_DOWN){
if(this.editNode.clientHeight>=this.editNode.scrollHeight){
e.preventDefault();
}
}
}
return true;
},onKeyUp:function(){
},setDisabled:function(_46){
_c.deprecated("dijit.Editor::setDisabled is deprecated","use dijit.Editor::attr(\"disabled\",boolean) instead",2);
this.set("disabled",_46);
},_setValueAttr:function(_47){
this.setValue(_47);
},_setDisableSpellCheckAttr:function(_48){
if(this.document){
_6.set(this.document.body,"spellcheck",!_48);
}else{
this.onLoadDeferred.addCallback(_e.hitch(this,function(){
_6.set(this.document.body,"spellcheck",!_48);
}));
}
this._set("disableSpellCheck",_48);
},onKeyPress:function(e){
var c=(e.keyChar&&e.keyChar.toLowerCase())||e.keyCode,_49=this._keyHandlers[c],_4a=arguments;
if(_49&&!e.altKey){
_1.some(_49,function(h){
if(!(h.shift^e.shiftKey)&&!(h.ctrl^(e.ctrlKey||e.metaKey))){
if(!h.handler.apply(this,_4a)){
e.preventDefault();
}
return true;
}
},this);
}
if(!this._onKeyHitch){
this._onKeyHitch=_e.hitch(this,"onKeyPressed");
}
setTimeout(this._onKeyHitch,1);
return true;
},addKeyHandler:function(key,_4b,_4c,_4d){
if(!_e.isArray(this._keyHandlers[key])){
this._keyHandlers[key]=[];
}
this._keyHandlers[key].push({shift:_4c||false,ctrl:_4b||false,handler:_4d});
},onKeyPressed:function(){
this.onDisplayChanged();
},onClick:function(e){
this.onDisplayChanged(e);
},_onIEMouseDown:function(){
if(!this.focused&&!this.disabled){
this.focus();
}
},_onBlur:function(e){
this.inherited(arguments);
var _4e=this.getValue(true);
if(_4e!==this.value){
this.onChange(_4e);
}
this._set("value",_4e);
},_onFocus:function(e){
if(!this.disabled){
if(!this._disabledOK){
this.set("disabled",false);
}
this.inherited(arguments);
}
},blur:function(){
if(!has("ie")&&this.window.document.documentElement&&this.window.document.documentElement.focus){
this.window.document.documentElement.focus();
}else{
if(win.doc.body.focus){
win.doc.body.focus();
}
}
},focus:function(){
if(!this.isLoaded){
this.focusOnLoad=true;
return;
}
if(this._cursorToStart){
delete this._cursorToStart;
if(this.editNode.childNodes){
this.placeCursorAtStart();
return;
}
}
if(!has("ie")){
_19.focus(this.iframe);
}else{
if(this.editNode&&this.editNode.focus){
this.iframe.fireEvent("onfocus",document.createEventObject());
}
}
},updateInterval:200,_updateTimer:null,onDisplayChanged:function(){
if(this._updateTimer){
clearTimeout(this._updateTimer);
}
if(!this._updateHandler){
this._updateHandler=_e.hitch(this,"onNormalizedDisplayChanged");
}
this._updateTimer=setTimeout(this._updateHandler,this.updateInterval);
},onNormalizedDisplayChanged:function(){
delete this._updateTimer;
},onChange:function(){
},_normalizeCommand:function(cmd,_4f){
var _50=cmd.toLowerCase();
if(_50==="formatblock"){
if(has("safari")&&_4f===undefined){
_50="heading";
}
}else{
if(_50==="hilitecolor"&&!has("mozilla")){
_50="backcolor";
}
}
return _50;
},_qcaCache:{},queryCommandAvailable:function(_51){
var ca=this._qcaCache[_51];
if(ca!==undefined){
return ca;
}
return (this._qcaCache[_51]=this._queryCommandAvailable(_51));
},_queryCommandAvailable:function(_52){
var ie=1;
var _53=1<<1;
var _54=1<<2;
var _55=1<<3;
function _56(_57){
return {ie:Boolean(_57&ie),mozilla:Boolean(_57&_53),webkit:Boolean(_57&_54),opera:Boolean(_57&_55)};
};
var _58=null;
switch(_52.toLowerCase()){
case "bold":
case "italic":
case "underline":
case "subscript":
case "superscript":
case "fontname":
case "fontsize":
case "forecolor":
case "hilitecolor":
case "justifycenter":
case "justifyfull":
case "justifyleft":
case "justifyright":
case "delete":
case "selectall":
case "toggledir":
_58=_56(_53|ie|_54|_55);
break;
case "createlink":
case "unlink":
case "removeformat":
case "inserthorizontalrule":
case "insertimage":
case "insertorderedlist":
case "insertunorderedlist":
case "indent":
case "outdent":
case "formatblock":
case "inserthtml":
case "undo":
case "redo":
case "strikethrough":
case "tabindent":
_58=_56(_53|ie|_55|_54);
break;
case "blockdirltr":
case "blockdirrtl":
case "dirltr":
case "dirrtl":
case "inlinedirltr":
case "inlinedirrtl":
_58=_56(ie);
break;
case "cut":
case "copy":
case "paste":
_58=_56(ie|_53|_54);
break;
case "inserttable":
_58=_56(_53|ie);
break;
case "insertcell":
case "insertcol":
case "insertrow":
case "deletecells":
case "deletecols":
case "deleterows":
case "mergecells":
case "splitcell":
_58=_56(ie|_53);
break;
default:
return false;
}
return (has("ie")&&_58.ie)||(has("mozilla")&&_58.mozilla)||(has("webkit")&&_58.webkit)||(has("opera")&&_58.opera);
},execCommand:function(_59,_5a){
var _5b;
this.focus();
_59=this._normalizeCommand(_59,_5a);
if(_5a!==undefined){
if(_59==="heading"){
throw new Error("unimplemented");
}else{
if((_59==="formatblock")&&has("ie")){
_5a="<"+_5a+">";
}
}
}
var _5c="_"+_59+"Impl";
if(this[_5c]){
_5b=this[_5c](_5a);
}else{
_5a=arguments.length>1?_5a:null;
if(_5a||_59!=="createlink"){
_5b=this.document.execCommand(_59,false,_5a);
}
}
this.onDisplayChanged();
return _5b;
},queryCommandEnabled:function(_5d){
if(this.disabled||!this._disabledOK){
return false;
}
_5d=this._normalizeCommand(_5d);
var _5e="_"+_5d+"EnabledImpl";
if(this[_5e]){
return this[_5e](_5d);
}else{
return this._browserQueryCommandEnabled(_5d);
}
},queryCommandState:function(_5f){
if(this.disabled||!this._disabledOK){
return false;
}
_5f=this._normalizeCommand(_5f);
try{
return this.document.queryCommandState(_5f);
}
catch(e){
return false;
}
},queryCommandValue:function(_60){
if(this.disabled||!this._disabledOK){
return false;
}
var r;
_60=this._normalizeCommand(_60);
if(has("ie")&&_60==="formatblock"){
r=this._native2LocalFormatNames[this.document.queryCommandValue(_60)];
}else{
if(has("mozilla")&&_60==="hilitecolor"){
var _61;
try{
_61=this.document.queryCommandValue("styleWithCSS");
}
catch(e){
_61=false;
}
this.document.execCommand("styleWithCSS",false,true);
r=this.document.queryCommandValue(_60);
this.document.execCommand("styleWithCSS",false,_61);
}else{
r=this.document.queryCommandValue(_60);
}
}
return r;
},_sCall:function(_62,_63){
return win.withGlobal(this.window,_62,_16,_63);
},placeCursorAtStart:function(){
this.focus();
var _64=false;
if(has("mozilla")){
var _65=this.editNode.firstChild;
while(_65){
if(_65.nodeType===3){
if(_65.nodeValue.replace(/^\s+|\s+$/g,"").length>0){
_64=true;
this._sCall("selectElement",[_65]);
break;
}
}else{
if(_65.nodeType===1){
_64=true;
var tg=_65.tagName?_65.tagName.toLowerCase():"";
if(/br|input|img|base|meta|area|basefont|hr|link/.test(tg)){
this._sCall("selectElement",[_65]);
}else{
this._sCall("selectElementChildren",[_65]);
}
break;
}
}
_65=_65.nextSibling;
}
}else{
_64=true;
this._sCall("selectElementChildren",[this.editNode]);
}
if(_64){
this._sCall("collapse",[true]);
}
},placeCursorAtEnd:function(){
this.focus();
var _66=false;
if(has("mozilla")){
var _67=this.editNode.lastChild;
while(_67){
if(_67.nodeType===3){
if(_67.nodeValue.replace(/^\s+|\s+$/g,"").length>0){
_66=true;
this._sCall("selectElement",[_67]);
break;
}
}else{
if(_67.nodeType===1){
_66=true;
if(_67.lastChild){
this._sCall("selectElement",[_67.lastChild]);
}else{
this._sCall("selectElement",[_67]);
}
break;
}
}
_67=_67.previousSibling;
}
}else{
_66=true;
this._sCall("selectElementChildren",[this.editNode]);
}
if(_66){
this._sCall("collapse",[false]);
}
},getValue:function(_68){
if(this.textarea){
if(this.isClosed||!this.isLoaded){
return this.textarea.value;
}
}
return this._postFilterContent(null,_68);
},_getValueAttr:function(){
return this.getValue(true);
},setValue:function(_69){
if(!this.isLoaded){
this.onLoadDeferred.addCallback(_e.hitch(this,function(){
this.setValue(_69);
}));
return;
}
this._cursorToStart=true;
if(this.textarea&&(this.isClosed||!this.isLoaded)){
this.textarea.value=_69;
}else{
_69=this._preFilterContent(_69);
var _6a=this.isClosed?this.domNode:this.editNode;
if(_69&&has("mozilla")&&_69.toLowerCase()==="<p></p>"){
_69="<p>&#160;</p>";
}
if(!_69&&has("webkit")){
_69="&#160;";
}
_6a.innerHTML=_69;
this._preDomFilterContent(_6a);
}
this.onDisplayChanged();
this._set("value",this.getValue(true));
},replaceValue:function(_6b){
if(this.isClosed){
this.setValue(_6b);
}else{
if(this.window&&this.window.getSelection&&!has("mozilla")){
this.setValue(_6b);
}else{
if(this.window&&this.window.getSelection){
_6b=this._preFilterContent(_6b);
this.execCommand("selectall");
if(!_6b){
this._cursorToStart=true;
_6b="&#160;";
}
this.execCommand("inserthtml",_6b);
this._preDomFilterContent(this.editNode);
}else{
if(this.document&&this.document.selection){
this.setValue(_6b);
}
}
}
}
this._set("value",this.getValue(true));
},_preFilterContent:function(_6c){
var ec=_6c;
_1.forEach(this.contentPreFilters,function(ef){
if(ef){
ec=ef(ec);
}
});
return ec;
},_preDomFilterContent:function(dom){
dom=dom||this.editNode;
_1.forEach(this.contentDomPreFilters,function(ef){
if(ef&&_e.isFunction(ef)){
ef(dom);
}
},this);
},_postFilterContent:function(dom,_6d){
var ec;
if(!_e.isString(dom)){
dom=dom||this.editNode;
if(this.contentDomPostFilters.length){
if(_6d){
dom=_e.clone(dom);
}
_1.forEach(this.contentDomPostFilters,function(ef){
dom=ef(dom);
});
}
ec=_18.getChildrenHtml(dom);
}else{
ec=dom;
}
if(!_e.trim(ec.replace(/^\xA0\xA0*/,"").replace(/\xA0\xA0*$/,"")).length){
ec="";
}
_1.forEach(this.contentPostFilters,function(ef){
ec=ef(ec);
});
return ec;
},_saveContent:function(){
var _6e=_5.byId(_1a._scopeName+"._editor.RichText.value");
if(_6e){
if(_6e.value){
_6e.value+=this._SEPARATOR;
}
_6e.value+=this.name+this._NAME_CONTENT_SEP+this.getValue(true);
}
},escapeXml:function(str,_6f){
str=str.replace(/&/gm,"&amp;").replace(/</gm,"&lt;").replace(/>/gm,"&gt;").replace(/"/gm,"&quot;");
if(!_6f){
str=str.replace(/'/gm,"&#39;");
}
return str;
},getNodeHtml:function(_70){
_c.deprecated("dijit.Editor::getNodeHtml is deprecated","use dijit/_editor/html::getNodeHtml instead",2);
return _18.getNodeHtml(_70);
},getNodeChildrenHtml:function(dom){
_c.deprecated("dijit.Editor::getNodeChildrenHtml is deprecated","use dijit/_editor/html::getChildrenHtml instead",2);
return _18.getChildrenHtml(dom);
},close:function(_71){
if(this.isClosed){
return;
}
if(!arguments.length){
_71=true;
}
if(_71){
this._set("value",this.getValue(true));
}
if(this.interval){
clearInterval(this.interval);
}
if(this._webkitListener){
this.disconnect(this._webkitListener);
delete this._webkitListener;
}
if(has("ie")){
this.iframe.onfocus=null;
}
this.iframe._loadFunc=null;
if(this._iframeRegHandle){
this._iframeRegHandle.remove();
delete this._iframeRegHandle;
}
if(this.textarea){
var s=this.textarea.style;
s.position="";
s.left=s.top="";
if(has("ie")){
s.overflow=this.__overflow;
this.__overflow=null;
}
this.textarea.value=this.value;
_8.destroy(this.domNode);
this.domNode=this.textarea;
}else{
this.domNode.innerHTML=this.value;
}
delete this.iframe;
_7.remove(this.domNode,this.baseClass);
this.isClosed=true;
this.isLoaded=false;
delete this.editNode;
delete this.focusNode;
if(this.window&&this.window._frameElement){
this.window._frameElement=null;
}
this.window=null;
this.document=null;
this.editingArea=null;
this.editorObject=null;
},destroy:function(){
if(!this.isClosed){
this.close(false);
}
if(this._updateTimer){
clearTimeout(this._updateTimer);
}
this.inherited(arguments);
if(_1b._globalSaveHandler){
delete _1b._globalSaveHandler[this.id];
}
},_removeMozBogus:function(_72){
return _72.replace(/\stype="_moz"/gi,"").replace(/\s_moz_dirty=""/gi,"").replace(/_moz_resizing="(true|false)"/gi,"");
},_removeWebkitBogus:function(_73){
_73=_73.replace(/\sclass="webkit-block-placeholder"/gi,"");
_73=_73.replace(/\sclass="apple-style-span"/gi,"");
_73=_73.replace(/<meta charset=\"utf-8\" \/>/gi,"");
return _73;
},_normalizeFontStyle:function(_74){
return _74.replace(/<(\/)?strong([ \>])/gi,"<$1b$2").replace(/<(\/)?em([ \>])/gi,"<$1i$2");
},_preFixUrlAttributes:function(_75){
return _75.replace(/(?:(<a(?=\s).*?\shref=)("|')(.*?)\2)|(?:(<a\s.*?href=)([^"'][^ >]+))/gi,"$1$4$2$3$5$2 _djrealurl=$2$3$5$2").replace(/(?:(<img(?=\s).*?\ssrc=)("|')(.*?)\2)|(?:(<img\s.*?src=)([^"'][^ >]+))/gi,"$1$4$2$3$5$2 _djrealurl=$2$3$5$2");
},_browserQueryCommandEnabled:function(_76){
if(!_76){
return false;
}
var _77=has("ie")?this.document.selection.createRange():this.document;
try{
return _77.queryCommandEnabled(_76);
}
catch(e){
return false;
}
},_createlinkEnabledImpl:function(){
var _78=true;
if(has("opera")){
var sel=this.window.getSelection();
if(sel.isCollapsed){
_78=true;
}else{
_78=this.document.queryCommandEnabled("createlink");
}
}else{
_78=this._browserQueryCommandEnabled("createlink");
}
return _78;
},_unlinkEnabledImpl:function(){
var _79=true;
if(has("mozilla")||has("webkit")){
_79=this._sCall("hasAncestorElement",["a"]);
}else{
_79=this._browserQueryCommandEnabled("unlink");
}
return _79;
},_inserttableEnabledImpl:function(){
var _7a=true;
if(has("mozilla")||has("webkit")){
_7a=true;
}else{
_7a=this._browserQueryCommandEnabled("inserttable");
}
return _7a;
},_cutEnabledImpl:function(){
var _7b=true;
if(has("webkit")){
var sel=this.window.getSelection();
if(sel){
sel=sel.toString();
}
_7b=!!sel;
}else{
_7b=this._browserQueryCommandEnabled("cut");
}
return _7b;
},_copyEnabledImpl:function(){
var _7c=true;
if(has("webkit")){
var sel=this.window.getSelection();
if(sel){
sel=sel.toString();
}
_7c=!!sel;
}else{
_7c=this._browserQueryCommandEnabled("copy");
}
return _7c;
},_pasteEnabledImpl:function(){
var _7d=true;
if(has("webkit")){
return true;
}else{
_7d=this._browserQueryCommandEnabled("paste");
}
return _7d;
},_inserthorizontalruleImpl:function(_7e){
if(has("ie")){
return this._inserthtmlImpl("<hr>");
}
return this.document.execCommand("inserthorizontalrule",false,_7e);
},_unlinkImpl:function(_7f){
if((this.queryCommandEnabled("unlink"))&&(has("mozilla")||has("webkit"))){
var a=this._sCall("getAncestorElement",["a"]);
this._sCall("selectElement",[a]);
return this.document.execCommand("unlink",false,null);
}
return this.document.execCommand("unlink",false,_7f);
},_hilitecolorImpl:function(_80){
var _81;
var _82=this._handleTextColorOrProperties("hilitecolor",_80);
if(!_82){
if(has("mozilla")){
this.document.execCommand("styleWithCSS",false,true);
_81=this.document.execCommand("hilitecolor",false,_80);
this.document.execCommand("styleWithCSS",false,false);
}else{
_81=this.document.execCommand("hilitecolor",false,_80);
}
}
return _81;
},_backcolorImpl:function(_83){
if(has("ie")){
_83=_83?_83:null;
}
var _84=this._handleTextColorOrProperties("backcolor",_83);
if(!_84){
_84=this.document.execCommand("backcolor",false,_83);
}
return _84;
},_forecolorImpl:function(_85){
if(has("ie")){
_85=_85?_85:null;
}
var _86=false;
_86=this._handleTextColorOrProperties("forecolor",_85);
if(!_86){
_86=this.document.execCommand("forecolor",false,_85);
}
return _86;
},_inserthtmlImpl:function(_87){
_87=this._preFilterContent(_87);
var rv=true;
if(has("ie")){
var _88=this.document.selection.createRange();
if(this.document.selection.type.toUpperCase()==="CONTROL"){
var n=_88.item(0);
while(_88.length){
_88.remove(_88.item(0));
}
n.outerHTML=_87;
}else{
_88.pasteHTML(_87);
}
_88.select();
}else{
if(has("mozilla")&&!_87.length){
this._sCall("remove");
}else{
rv=this.document.execCommand("inserthtml",false,_87);
}
}
return rv;
},_boldImpl:function(_89){
var _8a=false;
if(has("ie")){
this._adaptIESelection();
_8a=this._adaptIEFormatAreaAndExec("bold");
}
if(!_8a){
_8a=this.document.execCommand("bold",false,_89);
}
return _8a;
},_italicImpl:function(_8b){
var _8c=false;
if(has("ie")){
this._adaptIESelection();
_8c=this._adaptIEFormatAreaAndExec("italic");
}
if(!_8c){
_8c=this.document.execCommand("italic",false,_8b);
}
return _8c;
},_underlineImpl:function(_8d){
var _8e=false;
if(has("ie")){
this._adaptIESelection();
_8e=this._adaptIEFormatAreaAndExec("underline");
}
if(!_8e){
_8e=this.document.execCommand("underline",false,_8d);
}
return _8e;
},_strikethroughImpl:function(_8f){
var _90=false;
if(has("ie")){
this._adaptIESelection();
_90=this._adaptIEFormatAreaAndExec("strikethrough");
}
if(!_90){
_90=this.document.execCommand("strikethrough",false,_8f);
}
return _90;
},_superscriptImpl:function(_91){
var _92=false;
if(has("ie")){
this._adaptIESelection();
_92=this._adaptIEFormatAreaAndExec("superscript");
}
if(!_92){
_92=this.document.execCommand("superscript",false,_91);
}
return _92;
},_subscriptImpl:function(_93){
var _94=false;
if(has("ie")){
this._adaptIESelection();
_94=this._adaptIEFormatAreaAndExec("subscript");
}
if(!_94){
_94=this.document.execCommand("subscript",false,_93);
}
return _94;
},_fontnameImpl:function(_95){
var _96;
if(has("ie")){
_96=this._handleTextColorOrProperties("fontname",_95);
}
if(!_96){
_96=this.document.execCommand("fontname",false,_95);
}
return _96;
},_fontsizeImpl:function(_97){
var _98;
if(has("ie")){
_98=this._handleTextColorOrProperties("fontsize",_97);
}
if(!_98){
_98=this.document.execCommand("fontsize",false,_97);
}
return _98;
},_insertorderedlistImpl:function(_99){
var _9a=false;
if(has("ie")){
_9a=this._adaptIEList("insertorderedlist",_99);
}
if(!_9a){
_9a=this.document.execCommand("insertorderedlist",false,_99);
}
return _9a;
},_insertunorderedlistImpl:function(_9b){
var _9c=false;
if(has("ie")){
_9c=this._adaptIEList("insertunorderedlist",_9b);
}
if(!_9c){
_9c=this.document.execCommand("insertunorderedlist",false,_9b);
}
return _9c;
},getHeaderHeight:function(){
return this._getNodeChildrenHeight(this.header);
},getFooterHeight:function(){
return this._getNodeChildrenHeight(this.footer);
},_getNodeChildrenHeight:function(_9d){
var h=0;
if(_9d&&_9d.childNodes){
var i;
for(i=0;i<_9d.childNodes.length;i++){
var _9e=_9.position(_9d.childNodes[i]);
h+=_9e.h;
}
}
return h;
},_isNodeEmpty:function(_9f,_a0){
if(_9f.nodeType===1){
if(_9f.childNodes.length>0){
return this._isNodeEmpty(_9f.childNodes[0],_a0);
}
return true;
}else{
if(_9f.nodeType===3){
return (_9f.nodeValue.substring(_a0)==="");
}
}
return false;
},_removeStartingRangeFromRange:function(_a1,_a2){
if(_a1.nextSibling){
_a2.setStart(_a1.nextSibling,0);
}else{
var _a3=_a1.parentNode;
while(_a3&&_a3.nextSibling==null){
_a3=_a3.parentNode;
}
if(_a3){
_a2.setStart(_a3.nextSibling,0);
}
}
return _a2;
},_adaptIESelection:function(){
var _a4=_17.getSelection(this.window);
if(_a4&&_a4.rangeCount&&!_a4.isCollapsed){
var _a5=_a4.getRangeAt(0);
var _a6=_a5.startContainer;
var _a7=_a5.startOffset;
while(_a6.nodeType===3&&_a7>=_a6.length&&_a6.nextSibling){
_a7=_a7-_a6.length;
_a6=_a6.nextSibling;
}
var _a8=null;
while(this._isNodeEmpty(_a6,_a7)&&_a6!==_a8){
_a8=_a6;
_a5=this._removeStartingRangeFromRange(_a6,_a5);
_a6=_a5.startContainer;
_a7=0;
}
_a4.removeAllRanges();
_a4.addRange(_a5);
}
},_adaptIEFormatAreaAndExec:function(_a9){
var _aa=_17.getSelection(this.window);
var doc=this.document;
var rs,ret,_ab,txt,_ac,_ad,_ae,_af;
if(_a9&&_aa&&_aa.isCollapsed){
var _b0=this.queryCommandValue(_a9);
if(_b0){
var _b1=this._tagNamesForCommand(_a9);
_ab=_aa.getRangeAt(0);
var fs=_ab.startContainer;
if(fs.nodeType===3){
var _b2=_ab.endOffset;
if(fs.length<_b2){
ret=this._adjustNodeAndOffset(rs,_b2);
fs=ret.node;
_b2=ret.offset;
}
}
var _b3;
while(fs&&fs!==this.editNode){
var _b4=fs.tagName?fs.tagName.toLowerCase():"";
if(_1.indexOf(_b1,_b4)>-1){
_b3=fs;
break;
}
fs=fs.parentNode;
}
if(_b3){
rs=_ab.startContainer;
var _b5=doc.createElement(_b3.tagName);
_8.place(_b5,_b3,"after");
if(rs&&rs.nodeType===3){
var _b6,_b7;
var _b8=_ab.endOffset;
if(rs.length<_b8){
ret=this._adjustNodeAndOffset(rs,_b8);
rs=ret.node;
_b8=ret.offset;
}
txt=rs.nodeValue;
_ac=doc.createTextNode(txt.substring(0,_b8));
var _b9=txt.substring(_b8,txt.length);
if(_b9){
_ad=doc.createTextNode(_b9);
}
_8.place(_ac,rs,"before");
if(_ad){
_ae=doc.createElement("span");
_ae.className="ieFormatBreakerSpan";
_8.place(_ae,rs,"after");
_8.place(_ad,_ae,"after");
_ad=_ae;
}
_8.destroy(rs);
var _ba=_ac.parentNode;
var _bb=[];
var _bc;
while(_ba!==_b3){
var tg=_ba.tagName;
_bc={tagName:tg};
_bb.push(_bc);
var _bd=doc.createElement(tg);
if(_ba.style){
if(_bd.style){
if(_ba.style.cssText){
_bd.style.cssText=_ba.style.cssText;
_bc.cssText=_ba.style.cssText;
}
}
}
if(_ba.tagName==="FONT"){
if(_ba.color){
_bd.color=_ba.color;
_bc.color=_ba.color;
}
if(_ba.face){
_bd.face=_ba.face;
_bc.face=_ba.face;
}
if(_ba.size){
_bd.size=_ba.size;
_bc.size=_ba.size;
}
}
if(_ba.className){
_bd.className=_ba.className;
_bc.className=_ba.className;
}
if(_ad){
_b6=_ad;
while(_b6){
_b7=_b6.nextSibling;
_bd.appendChild(_b6);
_b6=_b7;
}
}
if(_bd.tagName==_ba.tagName){
_ae=doc.createElement("span");
_ae.className="ieFormatBreakerSpan";
_8.place(_ae,_ba,"after");
_8.place(_bd,_ae,"after");
}else{
_8.place(_bd,_ba,"after");
}
_ac=_ba;
_ad=_bd;
_ba=_ba.parentNode;
}
if(_ad){
_b6=_ad;
if(_b6.nodeType===1||(_b6.nodeType===3&&_b6.nodeValue)){
_b5.innerHTML="";
}
while(_b6){
_b7=_b6.nextSibling;
_b5.appendChild(_b6);
_b6=_b7;
}
}
if(_bb.length){
_bc=_bb.pop();
var _be=doc.createElement(_bc.tagName);
if(_bc.cssText&&_be.style){
_be.style.cssText=_bc.cssText;
}
if(_bc.className){
_be.className=_bc.className;
}
if(_bc.tagName==="FONT"){
if(_bc.color){
_be.color=_bc.color;
}
if(_bc.face){
_be.face=_bc.face;
}
if(_bc.size){
_be.size=_bc.size;
}
}
_8.place(_be,_b5,"before");
while(_bb.length){
_bc=_bb.pop();
var _bf=doc.createElement(_bc.tagName);
if(_bc.cssText&&_bf.style){
_bf.style.cssText=_bc.cssText;
}
if(_bc.className){
_bf.className=_bc.className;
}
if(_bc.tagName==="FONT"){
if(_bc.color){
_bf.color=_bc.color;
}
if(_bc.face){
_bf.face=_bc.face;
}
if(_bc.size){
_bf.size=_bc.size;
}
}
_be.appendChild(_bf);
_be=_bf;
}
_af=doc.createTextNode(".");
_ae.appendChild(_af);
_be.appendChild(_af);
win.withGlobal(this.window,_e.hitch(this,function(){
var _c0=_17.create();
_c0.setStart(_af,0);
_c0.setEnd(_af,_af.length);
_aa.removeAllRanges();
_aa.addRange(_c0);
_16.collapse(false);
_af.parentNode.innerHTML="";
}));
}else{
_ae=doc.createElement("span");
_ae.className="ieFormatBreakerSpan";
_af=doc.createTextNode(".");
_ae.appendChild(_af);
_8.place(_ae,_b5,"before");
win.withGlobal(this.window,_e.hitch(this,function(){
var _c1=_17.create();
_c1.setStart(_af,0);
_c1.setEnd(_af,_af.length);
_aa.removeAllRanges();
_aa.addRange(_c1);
_16.collapse(false);
_af.parentNode.innerHTML="";
}));
}
if(!_b5.firstChild){
_8.destroy(_b5);
}
return true;
}
}
return false;
}else{
_ab=_aa.getRangeAt(0);
rs=_ab.startContainer;
if(rs&&rs.nodeType===3){
win.withGlobal(this.window,_e.hitch(this,function(){
var _c2=_ab.startOffset;
if(rs.length<_c2){
ret=this._adjustNodeAndOffset(rs,_c2);
rs=ret.node;
_c2=ret.offset;
}
txt=rs.nodeValue;
_ac=doc.createTextNode(txt.substring(0,_c2));
var _c3=txt.substring(_c2);
if(_c3!==""){
_ad=doc.createTextNode(txt.substring(_c2));
}
_ae=doc.createElement("span");
_af=doc.createTextNode(".");
_ae.appendChild(_af);
if(_ac.length){
_8.place(_ac,rs,"after");
}else{
_ac=rs;
}
_8.place(_ae,_ac,"after");
if(_ad){
_8.place(_ad,_ae,"after");
}
_8.destroy(rs);
var _c4=_17.create();
_c4.setStart(_af,0);
_c4.setEnd(_af,_af.length);
_aa.removeAllRanges();
_aa.addRange(_c4);
doc.execCommand(_a9);
_8.place(_ae.firstChild,_ae,"before");
_8.destroy(_ae);
_c4.setStart(_af,0);
_c4.setEnd(_af,_af.length);
_aa.removeAllRanges();
_aa.addRange(_c4);
_16.collapse(false);
_af.parentNode.innerHTML="";
}));
return true;
}
}
}else{
return false;
}
},_adaptIEList:function(_c5){
var _c6=_17.getSelection(this.window);
if(_c6.isCollapsed){
if(_c6.rangeCount&&!this.queryCommandValue(_c5)){
var _c7=_c6.getRangeAt(0);
var sc=_c7.startContainer;
if(sc&&sc.nodeType==3){
if(!_c7.startOffset){
win.withGlobal(this.window,_e.hitch(this,function(){
var _c8="ul";
if(_c5==="insertorderedlist"){
_c8="ol";
}
var _c9=_8.create(_c8);
var li=_8.create("li",null,_c9);
_8.place(_c9,sc,"before");
li.appendChild(sc);
_8.create("br",null,_c9,"after");
var _ca=_17.create();
_ca.setStart(sc,0);
_ca.setEnd(sc,sc.length);
_c6.removeAllRanges();
_c6.addRange(_ca);
_16.collapse(true);
}));
return true;
}
}
}
}
return false;
},_handleTextColorOrProperties:function(_cb,_cc){
var _cd=_17.getSelection(this.window);
var doc=this.document;
var rs,ret,_ce,txt,_cf,_d0,_d1,_d2;
_cc=_cc||null;
if(_cb&&_cd&&_cd.isCollapsed){
if(_cd.rangeCount){
_ce=_cd.getRangeAt(0);
rs=_ce.startContainer;
if(rs&&rs.nodeType===3){
win.withGlobal(this.window,_e.hitch(this,function(){
var _d3=_ce.startOffset;
if(rs.length<_d3){
ret=this._adjustNodeAndOffset(rs,_d3);
rs=ret.node;
_d3=ret.offset;
}
txt=rs.nodeValue;
_cf=doc.createTextNode(txt.substring(0,_d3));
var _d4=txt.substring(_d3);
if(_d4!==""){
_d0=doc.createTextNode(txt.substring(_d3));
}
_d1=_8.create("span");
_d2=doc.createTextNode(".");
_d1.appendChild(_d2);
var _d5=_8.create("span");
_d1.appendChild(_d5);
if(_cf.length){
_8.place(_cf,rs,"after");
}else{
_cf=rs;
}
_8.place(_d1,_cf,"after");
if(_d0){
_8.place(_d0,_d1,"after");
}
_8.destroy(rs);
var _d6=_17.create();
_d6.setStart(_d2,0);
_d6.setEnd(_d2,_d2.length);
_cd.removeAllRanges();
_cd.addRange(_d6);
if(has("webkit")){
var _d7="color";
if(_cb==="hilitecolor"||_cb==="backcolor"){
_d7="backgroundColor";
}
_a.set(_d1,_d7,_cc);
_16.remove();
_8.destroy(_d5);
_d1.innerHTML="&#160;";
_16.selectElement(_d1);
this.focus();
}else{
this.execCommand(_cb,_cc);
_8.place(_d1.firstChild,_d1,"before");
_8.destroy(_d1);
_d6.setStart(_d2,0);
_d6.setEnd(_d2,_d2.length);
_cd.removeAllRanges();
_cd.addRange(_d6);
_16.collapse(false);
_d2.parentNode.removeChild(_d2);
}
}));
return true;
}
}
}
return false;
},_adjustNodeAndOffset:function(_d8,_d9){
while(_d8.length<_d9&&_d8.nextSibling&&_d8.nextSibling.nodeType===3){
_d9=_d9-_d8.length;
_d8=_d8.nextSibling;
}
return {"node":_d8,"offset":_d9};
},_tagNamesForCommand:function(_da){
if(_da==="bold"){
return ["b","strong"];
}else{
if(_da==="italic"){
return ["i","em"];
}else{
if(_da==="strikethrough"){
return ["s","strike"];
}else{
if(_da==="superscript"){
return ["sup"];
}else{
if(_da==="subscript"){
return ["sub"];
}else{
if(_da==="underline"){
return ["u"];
}
}
}
}
}
}
return [];
},_stripBreakerNodes:function(_db){
win.withGlobal(this.window,_e.hitch(this,function(){
var _dc=_f(".ieFormatBreakerSpan",_db);
var i;
for(i=0;i<_dc.length;i++){
var b=_dc[i];
while(b.firstChild){
_8.place(b.firstChild,b,"before");
}
_8.destroy(b);
}
}));
return _db;
}});
return _1b;
});
