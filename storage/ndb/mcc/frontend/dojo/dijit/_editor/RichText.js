//>>built
define("dijit/_editor/RichText",["dojo/_base/array","dojo/_base/config","dojo/_base/declare","dojo/_base/Deferred","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/_base/event","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/on","dojo/query","dojo/ready","dojo/sniff","dojo/topic","dojo/_base/unload","dojo/_base/url","dojo/_base/window","../_Widget","../_CssStateMixin","./selection","./range","./html","../focus","../main"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,on,_f,_10,has,_11,_12,_13,win,_14,_15,_16,_17,_18,_19,_1a){
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
this.contentPreFilters=[_e.trim,_e.hitch(this,"_preFixUrlAttributes")].concat(this.contentPreFilters);
if(has("mozilla")){
this.contentPreFilters=[this._normalizeFontStyle].concat(this.contentPreFilters);
this.contentPostFilters=[this._removeMozBogus].concat(this.contentPostFilters);
}
if(has("webkit")){
this.contentPreFilters=[this._removeWebkitBogus].concat(this.contentPreFilters);
this.contentPostFilters=[this._removeWebkitBogus].concat(this.contentPostFilters);
}
if(has("ie")||has("trident")){
this.contentPostFilters=[this._normalizeFontStyle].concat(this.contentPostFilters);
this.contentDomPostFilters=[_e.hitch(this,"_stripBreakerNodes")].concat(this.contentDomPostFilters);
}
this.contentDomPostFilters=[_e.hitch(this,"_stripTrailingEmptyNodes")].concat(this.contentDomPostFilters);
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
this.ownerDocumentBody.appendChild(div);
var _23=_e.hitch(this,function(){
var _24=div.firstChild;
while(_24){
try{
this._sCall("selectElement",[_24.firstChild]);
var _25=_24.tagName.toLowerCase();
this._local2NativeFormatNames[_25]=document.queryCommandValue("formatblock");
this._native2LocalFormatNames[this._local2NativeFormatNames[_25]]=_25;
_24=_24.nextSibling.nextSibling;
}
catch(e){
}
}
_8.destroy(div);
});
this.defer(_23);
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
dn=this.domNode=this.ownerDocument.createElement("div");
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
this.defer(_28,10);
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
var ifr=(this.editorObject=this.iframe=this.ownerDocument.createElement("iframe"));
ifr.id=this.id+"_iframe";
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
this.document=w.document;
if(has("ie")){
this._localizeEditorCommands();
}
this.onLoad(_27);
});
var src=this._getIframeDocTxt().replace(/\\/g,"\\\\").replace(/'/g,"\\'"),s;
if(has("ie")<11){
s="javascript:document.open();try{parent.window;}catch(e){document.domain=\""+document.domain+"\";}"+"document.write('"+src+"');document.close()";
}else{
s="javascript: '"+src+"'";
}
if(has("ie")==9){
this.editingArea.appendChild(ifr);
ifr.src=s;
}else{
ifr.setAttribute("src",s);
this.editingArea.appendChild(ifr);
}
if(has("safari")<=4){
src=ifr.getAttribute("src");
if(!src||src.indexOf("javascript")===-1){
this.defer(function(){
ifr.setAttribute("src",s);
});
}
}
if(dn.nodeName==="LI"){
dn.lastChild.style.marginTop="-1.2em";
}
_7.add(this.domNode,this.baseClass);
},_local2NativeFormatNames:{},_native2LocalFormatNames:{},_getIframeDocTxt:function(){
var _2e=_a.getComputedStyle(this.domNode);
var _2f="<div id='dijitEditorBody' role='main'></div>";
var _30=[_2e.fontWeight,_2e.fontSize,_2e.fontFamily].join(" ");
var _31=_2e.lineHeight;
if(_31.indexOf("px")>=0){
_31=parseFloat(_31)/parseFloat(_2e.fontSize);
}else{
if(_31.indexOf("em")>=0){
_31=parseFloat(_31);
}else{
_31="normal";
}
}
var _32="";
var _33=this;
this.style.replace(/(^|;)\s*(line-|font-?)[^;]+/ig,function(_34){
_34=_34.replace(/^;/ig,"")+";";
var s=_34.split(":")[0];
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
_a.set(_33.domNode,sC,"");
}
_32+=_34+";";
});
var _35=_f("label[for=\""+this.id+"\"]");
var _36="";
if(_35.length){
_36=_35[0].innerHTML;
}else{
if(this["aria-label"]){
_36=this["aria-label"];
}else{
if(this["aria-labelledby"]){
_36=_5.byId(this["aria-labelledby"]).innerHTML;
}
}
}
this.iframe.setAttribute("title",_36);
return ["<!DOCTYPE html>",this.isLeftToRight()?"<html lang='"+this.lang+"'>\n<head>\n":"<html dir='rtl' lang='"+this.lang+"'>\n<head>\n",_36?"<title>"+_36+"</title>":"","<meta http-equiv='Content-Type' content='text/html'>\n","<style>\n","\tbody,html {\n","\t\tbackground:transparent;\n","\t\tpadding: 1px 0 0 0;\n","\t\tmargin: -1px 0 0 0;\n","\t}\n","\tbody,html,#dijitEditorBody { outline: none; }","html { height: 100%; width: 100%; overflow: hidden; }\n",this.height?"\tbody,#dijitEditorBody { height: 100%; width: 100%; overflow: auto; }\n":"\tbody,#dijitEditorBody { min-height: "+this.minHeight+"; width: 100%; overflow-x: auto; overflow-y: hidden; }\n","\tbody{\n","\t\ttop:0px;\n","\t\tleft:0px;\n","\t\tright:0px;\n","\t\tfont:",_30,";\n",((this.height||has("opera"))?"":"\t\tposition: fixed;\n"),"\t\tline-height:",_31,";\n","\t}\n","\tp{ margin: 1em 0; }\n","\tli > ul:-moz-first-node, li > ol:-moz-first-node{ padding-top: 1.2em; }\n",(has("ie")||has("trident")?"":"\tli{ min-height:1.2em; }\n"),"</style>\n",this._applyEditingAreaStyleSheets(),"\n","</head>\n<body ","onload='frameElement && frameElement._loadFunc(window,document)' ","style='"+_32+"'>",_2f,"</body>\n</html>"].join("");
},_applyEditingAreaStyleSheets:function(){
var _37=[];
if(this.styleSheets){
_37=this.styleSheets.split(";");
this.styleSheets="";
}
_37=_37.concat(this.editingAreaStyleSheets);
this.editingAreaStyleSheets=[];
var _38="",i=0,url;
while((url=_37[i++])){
var _39=(new _13(win.global.location,url)).toString();
this.editingAreaStyleSheets.push(_39);
_38+="<link rel=\"stylesheet\" type=\"text/css\" href=\""+_39+"\"/>";
}
return _38;
},addStyleSheet:function(uri){
var url=uri.toString();
if(url.charAt(0)==="."||(url.charAt(0)!=="/"&&!uri.host)){
url=(new _13(win.global.location,url)).toString();
}
if(_1.indexOf(this.editingAreaStyleSheets,url)>-1){
return;
}
this.editingAreaStyleSheets.push(url);
this.onLoadDeferred.then(_e.hitch(this,function(){
if(this.document.createStyleSheet){
this.document.createStyleSheet(url);
}else{
var _3a=this.document.getElementsByTagName("head")[0];
var _3b=this.document.createElement("link");
_3b.rel="stylesheet";
_3b.type="text/css";
_3b.href=url;
_3a.appendChild(_3b);
}
}));
},removeStyleSheet:function(uri){
var url=uri.toString();
if(url.charAt(0)==="."||(url.charAt(0)!=="/"&&!uri.host)){
url=(new _13(win.global.location,url)).toString();
}
var _3c=_1.indexOf(this.editingAreaStyleSheets,url);
if(_3c===-1){
return;
}
delete this.editingAreaStyleSheets[_3c];
_f("link[href=\""+url+"\"]",this.window.document).orphan();
},disabled:false,_mozSettingProps:{"styleWithCSS":false},_setDisabledAttr:function(_3d){
_3d=!!_3d;
this._set("disabled",_3d);
if(!this.isLoaded){
return;
}
var _3e=has("ie")&&(this.isLoaded||!this.focusOnLoad);
if(_3e){
this.editNode.unselectable="on";
}
this.editNode.contentEditable=!_3d;
this.editNode.tabIndex=_3d?"-1":this.tabIndex;
if(_3e){
this.defer(function(){
if(this.editNode){
this.editNode.unselectable="off";
}
});
}
if(has("mozilla")&&!_3d&&this._mozSettingProps){
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
this._disabledOK=true;
},onLoad:function(_3f){
if(!this.window.__registeredWindow){
this.window.__registeredWindow=true;
this._iframeRegHandle=_19.registerIframe(this.iframe);
}
this.editNode=this.document.body.firstChild;
var _40=this;
this.beforeIframeNode=_8.place("<div tabIndex=-1></div>",this.iframe,"before");
this.afterIframeNode=_8.place("<div tabIndex=-1></div>",this.iframe,"after");
this.iframe.onfocus=this.document.onfocus=function(){
_40.editNode.focus();
};
this.focusNode=this.editNode;
var _41=this.events.concat(this.captureEvents);
var ap=this.iframe?this.document:this.editNode;
_1.forEach(_41,function(_42){
this.connect(ap,_42.toLowerCase(),_42);
},this);
this.own(on(ap,"mouseup",_e.hitch(this,"onClick")));
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
this.defer("placeCursorAtEnd");
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
var _43=_e.hitch(this,function(){
this.setValue(_3f);
if(this.onLoadDeferred){
this.onLoadDeferred.resolve(true);
}
this.onDisplayChanged();
if(this.focusOnLoad){
_10(_e.hitch(this,"defer","focus",this.updateInterval));
}
this.value=this.getValue(true);
});
if(this.setValueDeferred){
this.setValueDeferred.then(_43);
}else{
_43();
}
},onKeyDown:function(e){
if(e.keyCode===_d.TAB&&this.isTabIndent){
_b.stop(e);
if(this.queryCommandEnabled((e.shiftKey?"outdent":"indent"))){
this.execCommand((e.shiftKey?"outdent":"indent"));
}
}
if(e.keyCode==_d.TAB&&!this.isTabIndent&&!e.ctrlKey&&!e.altKey){
if(e.shiftKey){
this.beforeIframeNode.focus();
}else{
this.afterIframeNode.focus();
}
}
if(has("ie")<9&&e.keyCode===_d.BACKSPACE&&this.document.selection.type==="Control"){
e.stopPropagation();
e.preventDefault();
this.execCommand("delete");
}else{
if((65<=e.keyCode&&e.keyCode<=90)||(e.keyCode>=37&&e.keyCode<=40)){
e.charCode=e.keyCode;
this.onKeyPress(e);
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
},setDisabled:function(_44){
_c.deprecated("dijit.Editor::setDisabled is deprecated","use dijit.Editor::attr(\"disabled\",boolean) instead",2);
this.set("disabled",_44);
},_setValueAttr:function(_45){
this.setValue(_45);
},_setDisableSpellCheckAttr:function(_46){
if(this.document){
_6.set(this.document.body,"spellcheck",!_46);
}else{
this.onLoadDeferred.then(_e.hitch(this,function(){
_6.set(this.document.body,"spellcheck",!_46);
}));
}
this._set("disableSpellCheck",_46);
},onKeyPress:function(e){
if(e.keyCode===_d.SHIFT||e.keyCode===_d.ALT||e.keyCode===_d.META||e.keyCode===_d.CTRL||(e.keyCode==_d.TAB&&!this.isTabIndent&&!e.ctrlKey&&!e.altKey)){
return true;
}
var c=(e.keyChar&&e.keyChar.toLowerCase())||e.keyCode,_47=this._keyHandlers[c],_48=arguments;
if(_47&&!e.altKey){
_1.some(_47,function(h){
if(!(h.shift^e.shiftKey)&&!(h.ctrl^(e.ctrlKey||e.metaKey))){
if(!h.handler.apply(this,_48)){
e.preventDefault();
}
return true;
}
},this);
}
if(!this._onKeyHitch){
this._onKeyHitch=_e.hitch(this,"onKeyPressed");
}
this.defer("_onKeyHitch",1);
return true;
},addKeyHandler:function(key,_49,_4a,_4b){
if(!_e.isArray(this._keyHandlers[key])){
this._keyHandlers[key]=[];
}
this._keyHandlers[key].push({shift:_4a||false,ctrl:_49||false,handler:_4b});
},onKeyPressed:function(){
this.onDisplayChanged();
},onClick:function(e){
this.onDisplayChanged(e);
},_onIEMouseDown:function(){
if(!this.focused&&!this.disabled){
this.focus();
}
},_onBlur:function(e){
if(has("ie")||has("trident")){
this.defer(function(){
if(!_19.curNode){
this.ownerDocumentBody.focus();
}
});
}
this.inherited(arguments);
var _4c=this.getValue(true);
if(_4c!==this.value){
this.onChange(_4c);
}
this._set("value",_4c);
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
if(this.ownerDocumentBody.focus){
this.ownerDocumentBody.focus();
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
if(has("ie")<9){
this.iframe.fireEvent("onfocus",document.createEventObject());
}else{
this.editNode.focus();
}
},updateInterval:200,_updateTimer:null,onDisplayChanged:function(){
if(this._updateTimer){
this._updateTimer.remove();
}
this._updateTimer=this.defer("onNormalizedDisplayChanged",this.updateInterval);
},onNormalizedDisplayChanged:function(){
delete this._updateTimer;
},onChange:function(){
},_normalizeCommand:function(cmd,_4d){
var _4e=cmd.toLowerCase();
if(_4e==="formatblock"){
if(has("safari")&&_4d===undefined){
_4e="heading";
}
}else{
if(_4e==="hilitecolor"&&!has("mozilla")){
_4e="backcolor";
}
}
return _4e;
},_qcaCache:{},queryCommandAvailable:function(_4f){
var ca=this._qcaCache[_4f];
if(ca!==undefined){
return ca;
}
return (this._qcaCache[_4f]=this._queryCommandAvailable(_4f));
},_queryCommandAvailable:function(_50){
var ie=1;
var _51=1<<1;
var _52=1<<2;
var _53=1<<3;
function _54(_55){
return {ie:Boolean(_55&ie),mozilla:Boolean(_55&_51),webkit:Boolean(_55&_52),opera:Boolean(_55&_53)};
};
var _56=null;
switch(_50.toLowerCase()){
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
_56=_54(_51|ie|_52|_53);
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
_56=_54(_51|ie|_53|_52);
break;
case "blockdirltr":
case "blockdirrtl":
case "dirltr":
case "dirrtl":
case "inlinedirltr":
case "inlinedirrtl":
_56=_54(ie);
break;
case "cut":
case "copy":
case "paste":
_56=_54(ie|_51|_52|_53);
break;
case "inserttable":
_56=_54(_51|ie);
break;
case "insertcell":
case "insertcol":
case "insertrow":
case "deletecells":
case "deletecols":
case "deleterows":
case "mergecells":
case "splitcell":
_56=_54(ie|_51);
break;
default:
return false;
}
return ((has("ie")||has("trident"))&&_56.ie)||(has("mozilla")&&_56.mozilla)||(has("webkit")&&_56.webkit)||(has("opera")&&_56.opera);
},execCommand:function(_57,_58){
var _59;
if(this.focused){
this.focus();
}
_57=this._normalizeCommand(_57,_58);
if(_58!==undefined){
if(_57==="heading"){
throw new Error("unimplemented");
}else{
if(_57==="formatblock"&&(has("ie")||has("trident"))){
_58="<"+_58+">";
}
}
}
var _5a="_"+_57+"Impl";
if(this[_5a]){
_59=this[_5a](_58);
}else{
_58=arguments.length>1?_58:null;
if(_58||_57!=="createlink"){
_59=this.document.execCommand(_57,false,_58);
}
}
this.onDisplayChanged();
return _59;
},queryCommandEnabled:function(_5b){
if(this.disabled||!this._disabledOK){
return false;
}
_5b=this._normalizeCommand(_5b);
var _5c="_"+_5b+"EnabledImpl";
if(this[_5c]){
return this[_5c](_5b);
}else{
return this._browserQueryCommandEnabled(_5b);
}
},queryCommandState:function(_5d){
if(this.disabled||!this._disabledOK){
return false;
}
_5d=this._normalizeCommand(_5d);
try{
return this.document.queryCommandState(_5d);
}
catch(e){
return false;
}
},queryCommandValue:function(_5e){
if(this.disabled||!this._disabledOK){
return false;
}
var r;
_5e=this._normalizeCommand(_5e);
if((has("ie")||has("trident"))&&_5e==="formatblock"){
r=this._native2LocalFormatNames[this.document.queryCommandValue(_5e)];
}else{
if(has("mozilla")&&_5e==="hilitecolor"){
var _5f;
try{
_5f=this.document.queryCommandValue("styleWithCSS");
}
catch(e){
_5f=false;
}
this.document.execCommand("styleWithCSS",false,true);
r=this.document.queryCommandValue(_5e);
this.document.execCommand("styleWithCSS",false,_5f);
}else{
r=this.document.queryCommandValue(_5e);
}
}
return r;
},_sCall:function(_60,_61){
return win.withGlobal(this.window,_60,_16,_61);
},placeCursorAtStart:function(){
this.focus();
var _62=false;
if(has("mozilla")){
var _63=this.editNode.firstChild;
while(_63){
if(_63.nodeType===3){
if(_63.nodeValue.replace(/^\s+|\s+$/g,"").length>0){
_62=true;
this._sCall("selectElement",[_63]);
break;
}
}else{
if(_63.nodeType===1){
_62=true;
var tg=_63.tagName?_63.tagName.toLowerCase():"";
if(/br|input|img|base|meta|area|basefont|hr|link/.test(tg)){
this._sCall("selectElement",[_63]);
}else{
this._sCall("selectElementChildren",[_63]);
}
break;
}
}
_63=_63.nextSibling;
}
}else{
_62=true;
this._sCall("selectElementChildren",[this.editNode]);
}
if(_62){
this._sCall("collapse",[true]);
}
},placeCursorAtEnd:function(){
this.focus();
var _64=false;
if(has("mozilla")){
var _65=this.editNode.lastChild;
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
this._sCall("selectElement",[_65.lastChild||_65]);
break;
}
}
_65=_65.previousSibling;
}
}else{
_64=true;
this._sCall("selectElementChildren",[this.editNode]);
}
if(_64){
this._sCall("collapse",[false]);
}
},getValue:function(_66){
if(this.textarea){
if(this.isClosed||!this.isLoaded){
return this.textarea.value;
}
}
return this.isLoaded?this._postFilterContent(null,_66):this.value;
},_getValueAttr:function(){
return this.getValue(true);
},setValue:function(_67){
if(!this.isLoaded){
this.onLoadDeferred.then(_e.hitch(this,function(){
this.setValue(_67);
}));
return;
}
this._cursorToStart=true;
if(this.textarea&&(this.isClosed||!this.isLoaded)){
this.textarea.value=_67;
}else{
_67=this._preFilterContent(_67);
var _68=this.isClosed?this.domNode:this.editNode;
if(!_67&&has("webkit")){
_67="&#160;";
}
_68.innerHTML=_67;
this._preDomFilterContent(_68);
}
this.onDisplayChanged();
this._set("value",this.getValue(true));
},replaceValue:function(_69){
if(this.isClosed){
this.setValue(_69);
}else{
if(this.window&&this.window.getSelection&&!has("mozilla")){
this.setValue(_69);
}else{
if(this.window&&this.window.getSelection){
_69=this._preFilterContent(_69);
this.execCommand("selectall");
this.execCommand("inserthtml",_69);
this._preDomFilterContent(this.editNode);
}else{
if(this.document&&this.document.selection){
this.setValue(_69);
}
}
}
}
this._set("value",this.getValue(true));
},_preFilterContent:function(_6a){
var ec=_6a;
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
},_postFilterContent:function(dom,_6b){
var ec;
if(!_e.isString(dom)){
dom=dom||this.editNode;
if(this.contentDomPostFilters.length){
if(_6b){
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
var _6c=_5.byId(_1a._scopeName+"._editor.RichText.value");
if(_6c){
if(_6c.value){
_6c.value+=this._SEPARATOR;
}
_6c.value+=this.name+this._NAME_CONTENT_SEP+this.getValue(true);
}
},escapeXml:function(str,_6d){
str=str.replace(/&/gm,"&amp;").replace(/</gm,"&lt;").replace(/>/gm,"&gt;").replace(/"/gm,"&quot;");
if(!_6d){
str=str.replace(/'/gm,"&#39;");
}
return str;
},getNodeHtml:function(_6e){
_c.deprecated("dijit.Editor::getNodeHtml is deprecated","use dijit/_editor/html::getNodeHtml instead",2);
return _18.getNodeHtml(_6e);
},getNodeChildrenHtml:function(dom){
_c.deprecated("dijit.Editor::getNodeChildrenHtml is deprecated","use dijit/_editor/html::getChildrenHtml instead",2);
return _18.getChildrenHtml(dom);
},close:function(_6f){
if(this.isClosed){
return;
}
if(!arguments.length){
_6f=true;
}
if(_6f){
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
this._updateTimer.remove();
}
this.inherited(arguments);
if(_1b._globalSaveHandler){
delete _1b._globalSaveHandler[this.id];
}
},_removeMozBogus:function(_70){
return _70.replace(/\stype="_moz"/gi,"").replace(/\s_moz_dirty=""/gi,"").replace(/_moz_resizing="(true|false)"/gi,"");
},_removeWebkitBogus:function(_71){
_71=_71.replace(/\sclass="webkit-block-placeholder"/gi,"");
_71=_71.replace(/\sclass="apple-style-span"/gi,"");
_71=_71.replace(/<meta charset=\"utf-8\" \/>/gi,"");
return _71;
},_normalizeFontStyle:function(_72){
return _72.replace(/<(\/)?strong([ \>])/gi,"<$1b$2").replace(/<(\/)?em([ \>])/gi,"<$1i$2");
},_preFixUrlAttributes:function(_73){
return _73.replace(/(?:(<a(?=\s).*?\shref=)("|')(.*?)\2)|(?:(<a\s.*?href=)([^"'][^ >]+))/gi,"$1$4$2$3$5$2 _djrealurl=$2$3$5$2").replace(/(?:(<img(?=\s).*?\ssrc=)("|')(.*?)\2)|(?:(<img\s.*?src=)([^"'][^ >]+))/gi,"$1$4$2$3$5$2 _djrealurl=$2$3$5$2");
},_browserQueryCommandEnabled:function(_74){
if(!_74){
return false;
}
var _75=has("ie")<9?this.document.selection.createRange():this.document;
try{
return _75.queryCommandEnabled(_74);
}
catch(e){
return false;
}
},_createlinkEnabledImpl:function(){
var _76=true;
if(has("opera")){
var sel=this.window.getSelection();
if(sel.isCollapsed){
_76=true;
}else{
_76=this.document.queryCommandEnabled("createlink");
}
}else{
_76=this._browserQueryCommandEnabled("createlink");
}
return _76;
},_unlinkEnabledImpl:function(){
var _77=true;
if(has("mozilla")||has("webkit")){
_77=this._sCall("hasAncestorElement",["a"]);
}else{
_77=this._browserQueryCommandEnabled("unlink");
}
return _77;
},_inserttableEnabledImpl:function(){
var _78=true;
if(has("mozilla")||has("webkit")){
_78=true;
}else{
_78=this._browserQueryCommandEnabled("inserttable");
}
return _78;
},_cutEnabledImpl:function(){
var _79=true;
if(has("webkit")){
var sel=this.window.getSelection();
if(sel){
sel=sel.toString();
}
_79=!!sel;
}else{
_79=this._browserQueryCommandEnabled("cut");
}
return _79;
},_copyEnabledImpl:function(){
var _7a=true;
if(has("webkit")){
var sel=this.window.getSelection();
if(sel){
sel=sel.toString();
}
_7a=!!sel;
}else{
_7a=this._browserQueryCommandEnabled("copy");
}
return _7a;
},_pasteEnabledImpl:function(){
var _7b=true;
if(has("webkit")){
return true;
}else{
_7b=this._browserQueryCommandEnabled("paste");
}
return _7b;
},_inserthorizontalruleImpl:function(_7c){
if(has("ie")){
return this._inserthtmlImpl("<hr>");
}
return this.document.execCommand("inserthorizontalrule",false,_7c);
},_unlinkImpl:function(_7d){
if((this.queryCommandEnabled("unlink"))&&(has("mozilla")||has("webkit"))){
var a=this._sCall("getAncestorElement",["a"]);
this._sCall("selectElement",[a]);
return this.document.execCommand("unlink",false,null);
}
return this.document.execCommand("unlink",false,_7d);
},_hilitecolorImpl:function(_7e){
var _7f;
var _80=this._handleTextColorOrProperties("hilitecolor",_7e);
if(!_80){
if(has("mozilla")){
this.document.execCommand("styleWithCSS",false,true);
_7f=this.document.execCommand("hilitecolor",false,_7e);
this.document.execCommand("styleWithCSS",false,false);
}else{
_7f=this.document.execCommand("hilitecolor",false,_7e);
}
}
return _7f;
},_backcolorImpl:function(_81){
if(has("ie")){
_81=_81?_81:null;
}
var _82=this._handleTextColorOrProperties("backcolor",_81);
if(!_82){
_82=this.document.execCommand("backcolor",false,_81);
}
return _82;
},_forecolorImpl:function(_83){
if(has("ie")){
_83=_83?_83:null;
}
var _84=false;
_84=this._handleTextColorOrProperties("forecolor",_83);
if(!_84){
_84=this.document.execCommand("forecolor",false,_83);
}
return _84;
},_inserthtmlImpl:function(_85){
_85=this._preFilterContent(_85);
var rv=true;
if(has("ie")<9){
var _86=this.document.selection.createRange();
if(this.document.selection.type.toUpperCase()==="CONTROL"){
var n=_86.item(0);
while(_86.length){
_86.remove(_86.item(0));
}
n.outerHTML=_85;
}else{
_86.pasteHTML(_85);
}
_86.select();
}else{
if(has("trident")<8){
var _86;
var _87=_17.getSelection(this.window);
if(_87&&_87.rangeCount&&_87.getRangeAt){
_86=_87.getRangeAt(0);
_86.deleteContents();
var div=_8.create("div");
div.innerHTML=_85;
var _88,_89;
var n=this.document.createDocumentFragment();
while((_88=div.firstChild)){
_89=n.appendChild(_88);
}
_86.insertNode(n);
if(_89){
_86=_86.cloneRange();
_86.setStartAfter(_89);
_86.collapse(false);
_87.removeAllRanges();
_87.addRange(_86);
}
}
}else{
if(has("mozilla")&&!_85.length){
this._sCall("remove");
}else{
rv=this.document.execCommand("inserthtml",false,_85);
}
}
}
return rv;
},_boldImpl:function(_8a){
var _8b=false;
if(has("ie")||has("trident")){
this._adaptIESelection();
_8b=this._adaptIEFormatAreaAndExec("bold");
}
if(!_8b){
_8b=this.document.execCommand("bold",false,_8a);
}
return _8b;
},_italicImpl:function(_8c){
var _8d=false;
if(has("ie")||has("trident")){
this._adaptIESelection();
_8d=this._adaptIEFormatAreaAndExec("italic");
}
if(!_8d){
_8d=this.document.execCommand("italic",false,_8c);
}
return _8d;
},_underlineImpl:function(_8e){
var _8f=false;
if(has("ie")||has("trident")){
this._adaptIESelection();
_8f=this._adaptIEFormatAreaAndExec("underline");
}
if(!_8f){
_8f=this.document.execCommand("underline",false,_8e);
}
return _8f;
},_strikethroughImpl:function(_90){
var _91=false;
if(has("ie")||has("trident")){
this._adaptIESelection();
_91=this._adaptIEFormatAreaAndExec("strikethrough");
}
if(!_91){
_91=this.document.execCommand("strikethrough",false,_90);
}
return _91;
},_superscriptImpl:function(_92){
var _93=false;
if(has("ie")||has("trident")){
this._adaptIESelection();
_93=this._adaptIEFormatAreaAndExec("superscript");
}
if(!_93){
_93=this.document.execCommand("superscript",false,_92);
}
return _93;
},_subscriptImpl:function(_94){
var _95=false;
if(has("ie")||has("trident")){
this._adaptIESelection();
_95=this._adaptIEFormatAreaAndExec("subscript");
}
if(!_95){
_95=this.document.execCommand("subscript",false,_94);
}
return _95;
},_fontnameImpl:function(_96){
var _97;
if(has("ie")||has("trident")){
_97=this._handleTextColorOrProperties("fontname",_96);
}
if(!_97){
_97=this.document.execCommand("fontname",false,_96);
}
return _97;
},_fontsizeImpl:function(_98){
var _99;
if(has("ie")||has("trident")){
_99=this._handleTextColorOrProperties("fontsize",_98);
}
if(!_99){
_99=this.document.execCommand("fontsize",false,_98);
}
return _99;
},_insertorderedlistImpl:function(_9a){
var _9b=false;
if(has("ie")||has("trident")){
_9b=this._adaptIEList("insertorderedlist",_9a);
}
if(!_9b){
_9b=this.document.execCommand("insertorderedlist",false,_9a);
}
return _9b;
},_insertunorderedlistImpl:function(_9c){
var _9d=false;
if(has("ie")||has("trident")){
_9d=this._adaptIEList("insertunorderedlist",_9c);
}
if(!_9d){
_9d=this.document.execCommand("insertunorderedlist",false,_9c);
}
return _9d;
},getHeaderHeight:function(){
return this._getNodeChildrenHeight(this.header);
},getFooterHeight:function(){
return this._getNodeChildrenHeight(this.footer);
},_getNodeChildrenHeight:function(_9e){
var h=0;
if(_9e&&_9e.childNodes){
var i;
for(i=0;i<_9e.childNodes.length;i++){
var _9f=_9.position(_9e.childNodes[i]);
h+=_9f.h;
}
}
return h;
},_isNodeEmpty:function(_a0,_a1){
if(_a0.nodeType===1){
if(_a0.childNodes.length>0){
return this._isNodeEmpty(_a0.childNodes[0],_a1);
}
return true;
}else{
if(_a0.nodeType===3){
return (_a0.nodeValue.substring(_a1)==="");
}
}
return false;
},_removeStartingRangeFromRange:function(_a2,_a3){
if(_a2.nextSibling){
_a3.setStart(_a2.nextSibling,0);
}else{
var _a4=_a2.parentNode;
while(_a4&&_a4.nextSibling==null){
_a4=_a4.parentNode;
}
if(_a4){
_a3.setStart(_a4.nextSibling,0);
}
}
return _a3;
},_adaptIESelection:function(){
var _a5=_17.getSelection(this.window);
if(_a5&&_a5.rangeCount&&!_a5.isCollapsed){
var _a6=_a5.getRangeAt(0);
var _a7=_a6.startContainer;
var _a8=_a6.startOffset;
while(_a7.nodeType===3&&_a8>=_a7.length&&_a7.nextSibling){
_a8=_a8-_a7.length;
_a7=_a7.nextSibling;
}
var _a9=null;
while(this._isNodeEmpty(_a7,_a8)&&_a7!==_a9){
_a9=_a7;
_a6=this._removeStartingRangeFromRange(_a7,_a6);
_a7=_a6.startContainer;
_a8=0;
}
_a5.removeAllRanges();
_a5.addRange(_a6);
}
},_adaptIEFormatAreaAndExec:function(_aa){
var _ab=_17.getSelection(this.window);
var doc=this.document;
var rs,ret,_ac,txt,_ad,_ae,_af,_b0;
if(_aa&&_ab&&_ab.isCollapsed){
var _b1=this.queryCommandValue(_aa);
if(_b1){
var _b2=this._tagNamesForCommand(_aa);
_ac=_ab.getRangeAt(0);
var fs=_ac.startContainer;
if(fs.nodeType===3){
var _b3=_ac.endOffset;
if(fs.length<_b3){
ret=this._adjustNodeAndOffset(rs,_b3);
fs=ret.node;
_b3=ret.offset;
}
}
var _b4;
while(fs&&fs!==this.editNode){
var _b5=fs.tagName?fs.tagName.toLowerCase():"";
if(_1.indexOf(_b2,_b5)>-1){
_b4=fs;
break;
}
fs=fs.parentNode;
}
if(_b4){
rs=_ac.startContainer;
var _b6=doc.createElement(_b4.tagName);
_8.place(_b6,_b4,"after");
if(rs&&rs.nodeType===3){
var _b7,_b8;
var _b9=_ac.endOffset;
if(rs.length<_b9){
ret=this._adjustNodeAndOffset(rs,_b9);
rs=ret.node;
_b9=ret.offset;
}
txt=rs.nodeValue;
_ad=doc.createTextNode(txt.substring(0,_b9));
var _ba=txt.substring(_b9,txt.length);
if(_ba){
_ae=doc.createTextNode(_ba);
}
_8.place(_ad,rs,"before");
if(_ae){
_af=doc.createElement("span");
_af.className="ieFormatBreakerSpan";
_8.place(_af,rs,"after");
_8.place(_ae,_af,"after");
_ae=_af;
}
_8.destroy(rs);
var _bb=_ad.parentNode;
var _bc=[];
var _bd;
while(_bb!==_b4){
var tg=_bb.tagName;
_bd={tagName:tg};
_bc.push(_bd);
var _be=doc.createElement(tg);
if(_bb.style){
if(_be.style){
if(_bb.style.cssText){
_be.style.cssText=_bb.style.cssText;
_bd.cssText=_bb.style.cssText;
}
}
}
if(_bb.tagName==="FONT"){
if(_bb.color){
_be.color=_bb.color;
_bd.color=_bb.color;
}
if(_bb.face){
_be.face=_bb.face;
_bd.face=_bb.face;
}
if(_bb.size){
_be.size=_bb.size;
_bd.size=_bb.size;
}
}
if(_bb.className){
_be.className=_bb.className;
_bd.className=_bb.className;
}
if(_ae){
_b7=_ae;
while(_b7){
_b8=_b7.nextSibling;
_be.appendChild(_b7);
_b7=_b8;
}
}
if(_be.tagName==_bb.tagName){
_af=doc.createElement("span");
_af.className="ieFormatBreakerSpan";
_8.place(_af,_bb,"after");
_8.place(_be,_af,"after");
}else{
_8.place(_be,_bb,"after");
}
_ad=_bb;
_ae=_be;
_bb=_bb.parentNode;
}
if(_ae){
_b7=_ae;
if(_b7.nodeType===1||(_b7.nodeType===3&&_b7.nodeValue)){
_b6.innerHTML="";
}
while(_b7){
_b8=_b7.nextSibling;
_b6.appendChild(_b7);
_b7=_b8;
}
}
var _bf;
if(_bc.length){
_bd=_bc.pop();
var _c0=doc.createElement(_bd.tagName);
if(_bd.cssText&&_c0.style){
_c0.style.cssText=_bd.cssText;
}
if(_bd.className){
_c0.className=_bd.className;
}
if(_bd.tagName==="FONT"){
if(_bd.color){
_c0.color=_bd.color;
}
if(_bd.face){
_c0.face=_bd.face;
}
if(_bd.size){
_c0.size=_bd.size;
}
}
_8.place(_c0,_b6,"before");
while(_bc.length){
_bd=_bc.pop();
var _c1=doc.createElement(_bd.tagName);
if(_bd.cssText&&_c1.style){
_c1.style.cssText=_bd.cssText;
}
if(_bd.className){
_c1.className=_bd.className;
}
if(_bd.tagName==="FONT"){
if(_bd.color){
_c1.color=_bd.color;
}
if(_bd.face){
_c1.face=_bd.face;
}
if(_bd.size){
_c1.size=_bd.size;
}
}
_c0.appendChild(_c1);
_c0=_c1;
}
_b0=doc.createTextNode(".");
_af.appendChild(_b0);
_c0.appendChild(_b0);
_bf=_17.create(this.window);
_bf.setStart(_b0,0);
_bf.setEnd(_b0,_b0.length);
_ab.removeAllRanges();
_ab.addRange(_bf);
this._sCall("collapse",[false]);
_b0.parentNode.innerHTML="";
}else{
_af=doc.createElement("span");
_af.className="ieFormatBreakerSpan";
_b0=doc.createTextNode(".");
_af.appendChild(_b0);
_8.place(_af,_b6,"before");
_bf=_17.create(this.window);
_bf.setStart(_b0,0);
_bf.setEnd(_b0,_b0.length);
_ab.removeAllRanges();
_ab.addRange(_bf);
this._sCall("collapse",[false]);
_b0.parentNode.innerHTML="";
}
if(!_b6.firstChild){
_8.destroy(_b6);
}
return true;
}
}
return false;
}else{
_ac=_ab.getRangeAt(0);
rs=_ac.startContainer;
if(rs&&rs.nodeType===3){
var _b3=_ac.startOffset;
if(rs.length<_b3){
ret=this._adjustNodeAndOffset(rs,_b3);
rs=ret.node;
_b3=ret.offset;
}
txt=rs.nodeValue;
_ad=doc.createTextNode(txt.substring(0,_b3));
var _ba=txt.substring(_b3);
if(_ba!==""){
_ae=doc.createTextNode(txt.substring(_b3));
}
_af=doc.createElement("span");
_b0=doc.createTextNode(".");
_af.appendChild(_b0);
if(_ad.length){
_8.place(_ad,rs,"after");
}else{
_ad=rs;
}
_8.place(_af,_ad,"after");
if(_ae){
_8.place(_ae,_af,"after");
}
_8.destroy(rs);
var _bf=_17.create(this.window);
_bf.setStart(_b0,0);
_bf.setEnd(_b0,_b0.length);
_ab.removeAllRanges();
_ab.addRange(_bf);
doc.execCommand(_aa);
_8.place(_af.firstChild,_af,"before");
_8.destroy(_af);
_bf.setStart(_b0,0);
_bf.setEnd(_b0,_b0.length);
_ab.removeAllRanges();
_ab.addRange(_bf);
this._sCall("collapse",[false]);
_b0.parentNode.innerHTML="";
return true;
}
}
}else{
return false;
}
},_adaptIEList:function(_c2){
var _c3=_17.getSelection(this.window);
if(_c3.isCollapsed){
if(_c3.rangeCount&&!this.queryCommandValue(_c2)){
var _c4=_c3.getRangeAt(0);
var sc=_c4.startContainer;
if(sc&&sc.nodeType==3){
if(!_c4.startOffset){
var _c5="ul";
if(_c2==="insertorderedlist"){
_c5="ol";
}
var _c6=this.document.createElement(_c5);
var li=_8.create("li",null,_c6);
_8.place(_c6,sc,"before");
li.appendChild(sc);
_8.create("br",null,_c6,"after");
var _c7=_17.create(this.window);
_c7.setStart(sc,0);
_c7.setEnd(sc,sc.length);
_c3.removeAllRanges();
_c3.addRange(_c7);
this._sCall("collapse",[true]);
return true;
}
}
}
}
return false;
},_handleTextColorOrProperties:function(_c8,_c9){
var _ca=_17.getSelection(this.window);
var doc=this.document;
var rs,ret,_cb,txt,_cc,_cd,_ce,_cf;
_c9=_c9||null;
if(_c8&&_ca&&_ca.isCollapsed){
if(_ca.rangeCount){
_cb=_ca.getRangeAt(0);
rs=_cb.startContainer;
if(rs&&rs.nodeType===3){
var _d0=_cb.startOffset;
if(rs.length<_d0){
ret=this._adjustNodeAndOffset(rs,_d0);
rs=ret.node;
_d0=ret.offset;
}
txt=rs.nodeValue;
_cc=doc.createTextNode(txt.substring(0,_d0));
var _d1=txt.substring(_d0);
if(_d1!==""){
_cd=doc.createTextNode(txt.substring(_d0));
}
_ce=doc.createElement("span");
_cf=doc.createTextNode(".");
_ce.appendChild(_cf);
var _d2=doc.createElement("span");
_ce.appendChild(_d2);
if(_cc.length){
_8.place(_cc,rs,"after");
}else{
_cc=rs;
}
_8.place(_ce,_cc,"after");
if(_cd){
_8.place(_cd,_ce,"after");
}
_8.destroy(rs);
var _d3=_17.create(this.window);
_d3.setStart(_cf,0);
_d3.setEnd(_cf,_cf.length);
_ca.removeAllRanges();
_ca.addRange(_d3);
if(has("webkit")){
var _d4="color";
if(_c8==="hilitecolor"||_c8==="backcolor"){
_d4="backgroundColor";
}
_a.set(_ce,_d4,_c9);
this._sCall("remove",[]);
_8.destroy(_d2);
_ce.innerHTML="&#160;";
this._sCall("selectElement",[_ce]);
this.focus();
}else{
this.execCommand(_c8,_c9);
_8.place(_ce.firstChild,_ce,"before");
_8.destroy(_ce);
_d3.setStart(_cf,0);
_d3.setEnd(_cf,_cf.length);
_ca.removeAllRanges();
_ca.addRange(_d3);
this._sCall("collapse",[false]);
_cf.parentNode.removeChild(_cf);
}
return true;
}
}
}
return false;
},_adjustNodeAndOffset:function(_d5,_d6){
while(_d5.length<_d6&&_d5.nextSibling&&_d5.nextSibling.nodeType===3){
_d6=_d6-_d5.length;
_d5=_d5.nextSibling;
}
return {"node":_d5,"offset":_d6};
},_tagNamesForCommand:function(_d7){
if(_d7==="bold"){
return ["b","strong"];
}else{
if(_d7==="italic"){
return ["i","em"];
}else{
if(_d7==="strikethrough"){
return ["s","strike"];
}else{
if(_d7==="superscript"){
return ["sup"];
}else{
if(_d7==="subscript"){
return ["sub"];
}else{
if(_d7==="underline"){
return ["u"];
}
}
}
}
}
}
return [];
},_stripBreakerNodes:function(_d8){
if(!this.isLoaded){
return;
}
_f(".ieFormatBreakerSpan",_d8).forEach(function(b){
while(b.firstChild){
_8.place(b.firstChild,b,"before");
}
_8.destroy(b);
});
return _d8;
},_stripTrailingEmptyNodes:function(_d9){
function _da(_db){
return (/^(p|div|br)$/i.test(_db.nodeName)&&_db.children.length==0&&/^[\s\xA0]*$/.test(_db.textContent||_db.innerText||""))||(_db.nodeType===3&&/^[\s\xA0]*$/.test(_db.nodeValue));
};
while(_d9.lastChild&&_da(_d9.lastChild)){
_8.destroy(_d9.lastChild);
}
return _d9;
}});
return _1b;
});
