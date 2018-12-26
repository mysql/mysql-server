//>>built
define("dijit/layout/ContentPane",["dojo/_base/kernel","dojo/_base/lang","../_Widget","./_ContentPaneResizeMixin","dojo/string","dojo/html","dojo/i18n!../nls/loading","dojo/_base/array","dojo/_base/declare","dojo/_base/Deferred","dojo/dom","dojo/dom-attr","dojo/_base/window","dojo/_base/xhr","dojo/i18n"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
return _9("dijit.layout.ContentPane",[_3,_4],{href:"",content:"",extractContent:false,parseOnLoad:true,parserScope:_1._scopeName,preventCache:false,preload:false,refreshOnShow:false,loadingMessage:"<span class='dijitContentPaneLoading'><span class='dijitInline dijitIconLoading'></span>${loadingState}</span>",errorMessage:"<span class='dijitContentPaneError'><span class='dijitInline dijitIconError'></span>${errorState}</span>",isLoaded:false,baseClass:"dijitContentPane",ioArgs:{},onLoadDeferred:null,_setTitleAttr:null,stopParser:true,template:false,create:function(_10,_11){
if((!_10||!_10.template)&&_11&&!("href" in _10)&&!("content" in _10)){
var df=_d.doc.createDocumentFragment();
_11=_b.byId(_11);
while(_11.firstChild){
df.appendChild(_11.firstChild);
}
_10=_2.delegate(_10,{content:df});
}
this.inherited(arguments,[_10,_11]);
},postMixInProperties:function(){
this.inherited(arguments);
var _12=_f.getLocalization("dijit","loading",this.lang);
this.loadingMessage=_5.substitute(this.loadingMessage,_12);
this.errorMessage=_5.substitute(this.errorMessage,_12);
},buildRendering:function(){
this.inherited(arguments);
if(!this.containerNode){
this.containerNode=this.domNode;
}
this.domNode.title="";
if(!_c.get(this.domNode,"role")){
this.domNode.setAttribute("role","group");
}
},startup:function(){
this.inherited(arguments);
if(this._contentSetter){
_8.forEach(this._contentSetter.parseResults,function(obj){
if(!obj._started&&!obj._destroyed&&_2.isFunction(obj.startup)){
obj.startup();
obj._started=true;
}
},this);
}
},setHref:function(_13){
_1.deprecated("dijit.layout.ContentPane.setHref() is deprecated. Use set('href', ...) instead.","","2.0");
return this.set("href",_13);
},_setHrefAttr:function(_14){
this.cancel();
this.onLoadDeferred=new _a(_2.hitch(this,"cancel"));
this.onLoadDeferred.addCallback(_2.hitch(this,"onLoad"));
this._set("href",_14);
if(this.preload||(this._created&&this._isShown())){
this._load();
}else{
this._hrefChanged=true;
}
return this.onLoadDeferred;
},setContent:function(_15){
_1.deprecated("dijit.layout.ContentPane.setContent() is deprecated.  Use set('content', ...) instead.","","2.0");
this.set("content",_15);
},_setContentAttr:function(_16){
this._set("href","");
this.cancel();
this.onLoadDeferred=new _a(_2.hitch(this,"cancel"));
if(this._created){
this.onLoadDeferred.addCallback(_2.hitch(this,"onLoad"));
}
this._setContent(_16||"");
this._isDownloaded=false;
return this.onLoadDeferred;
},_getContentAttr:function(){
return this.containerNode.innerHTML;
},cancel:function(){
if(this._xhrDfd&&(this._xhrDfd.fired==-1)){
this._xhrDfd.cancel();
}
delete this._xhrDfd;
this.onLoadDeferred=null;
},uninitialize:function(){
if(this._beingDestroyed){
this.cancel();
}
this.inherited(arguments);
},destroyRecursive:function(_17){
if(this._beingDestroyed){
return;
}
this.inherited(arguments);
},_onShow:function(){
this.inherited(arguments);
if(this.href){
if(!this._xhrDfd&&(!this.isLoaded||this._hrefChanged||this.refreshOnShow)){
return this.refresh();
}
}
},refresh:function(){
this.cancel();
this.onLoadDeferred=new _a(_2.hitch(this,"cancel"));
this.onLoadDeferred.addCallback(_2.hitch(this,"onLoad"));
this._load();
return this.onLoadDeferred;
},_load:function(){
this._setContent(this.onDownloadStart(),true);
var _18=this;
var _19={preventCache:(this.preventCache||this.refreshOnShow),url:this.href,handleAs:"text"};
if(_2.isObject(this.ioArgs)){
_2.mixin(_19,this.ioArgs);
}
var _1a=(this._xhrDfd=(this.ioMethod||_e.get)(_19));
_1a.addCallback(function(_1b){
try{
_18._isDownloaded=true;
_18._setContent(_1b,false);
_18.onDownloadEnd();
}
catch(err){
_18._onError("Content",err);
}
delete _18._xhrDfd;
return _1b;
});
_1a.addErrback(function(err){
if(!_1a.canceled){
_18._onError("Download",err);
}
delete _18._xhrDfd;
return err;
});
delete this._hrefChanged;
},_onLoadHandler:function(_1c){
this._set("isLoaded",true);
try{
this.onLoadDeferred.callback(_1c);
}
catch(e){
console.error("Error "+this.widgetId+" running custom onLoad code: "+e.message);
}
},_onUnloadHandler:function(){
this._set("isLoaded",false);
try{
this.onUnload();
}
catch(e){
console.error("Error "+this.widgetId+" running custom onUnload code: "+e.message);
}
},destroyDescendants:function(_1d){
if(this.isLoaded){
this._onUnloadHandler();
}
var _1e=this._contentSetter;
_8.forEach(this.getChildren(),function(_1f){
if(_1f.destroyRecursive){
_1f.destroyRecursive(_1d);
}
});
if(_1e){
_8.forEach(_1e.parseResults,function(_20){
if(_20.destroyRecursive&&_20.domNode&&_20.domNode.parentNode==_d.body()){
_20.destroyRecursive(_1d);
}
});
delete _1e.parseResults;
}
if(!_1d){
_6._emptyNode(this.containerNode);
}
delete this._singleChild;
},_setContent:function(_21,_22){
this.destroyDescendants();
var _23=this._contentSetter;
if(!(_23&&_23 instanceof _6._ContentSetter)){
_23=this._contentSetter=new _6._ContentSetter({node:this.containerNode,_onError:_2.hitch(this,this._onError),onContentError:_2.hitch(this,function(e){
var _24=this.onContentError(e);
try{
this.containerNode.innerHTML=_24;
}
catch(e){
console.error("Fatal "+this.id+" could not change content due to "+e.message,e);
}
})});
}
var _25=_2.mixin({cleanContent:this.cleanContent,extractContent:this.extractContent,parseContent:!_21.domNode&&this.parseOnLoad,parserScope:this.parserScope,startup:false,dir:this.dir,lang:this.lang,textDir:this.textDir},this._contentSetterParams||{});
_23.set((_2.isObject(_21)&&_21.domNode)?_21.domNode:_21,_25);
delete this._contentSetterParams;
if(this.doLayout){
this._checkIfSingleChild();
}
if(!_22){
if(this._started){
delete this._started;
this.startup();
this._scheduleLayout();
}
this._onLoadHandler(_21);
}
},_onError:function(_26,err,_27){
this.onLoadDeferred.errback(err);
var _28=this["on"+_26+"Error"].call(this,err);
if(_27){
console.error(_27,err);
}else{
if(_28){
this._setContent(_28,true);
}
}
},onLoad:function(){
},onUnload:function(){
},onDownloadStart:function(){
return this.loadingMessage;
},onContentError:function(){
},onDownloadError:function(){
return this.errorMessage;
},onDownloadEnd:function(){
}});
});
