//>>built
define("dijit/layout/ContentPane",["dojo/_base/kernel","dojo/_base/lang","../_Widget","../_Container","./_ContentPaneResizeMixin","dojo/string","dojo/html","dojo/i18n!../nls/loading","dojo/_base/array","dojo/_base/declare","dojo/_base/Deferred","dojo/dom","dojo/dom-attr","dojo/dom-construct","dojo/_base/xhr","dojo/i18n","dojo/when"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11){
return _a("dijit.layout.ContentPane",[_3,_4,_5],{href:"",content:"",extractContent:false,parseOnLoad:true,parserScope:_1._scopeName,preventCache:false,preload:false,refreshOnShow:false,loadingMessage:"<span class='dijitContentPaneLoading'><span class='dijitInline dijitIconLoading'></span>${loadingState}</span>",errorMessage:"<span class='dijitContentPaneError'><span class='dijitInline dijitIconError'></span>${errorState}</span>",isLoaded:false,baseClass:"dijitContentPane",ioArgs:{},onLoadDeferred:null,_setTitleAttr:null,stopParser:true,template:false,create:function(_12,_13){
if((!_12||!_12.template)&&_13&&!("href" in _12)&&!("content" in _12)){
_13=_c.byId(_13);
var df=_13.ownerDocument.createDocumentFragment();
while(_13.firstChild){
df.appendChild(_13.firstChild);
}
_12=_2.delegate(_12,{content:df});
}
this.inherited(arguments,[_12,_13]);
},postMixInProperties:function(){
this.inherited(arguments);
var _14=_10.getLocalization("dijit","loading",this.lang);
this.loadingMessage=_6.substitute(this.loadingMessage,_14);
this.errorMessage=_6.substitute(this.errorMessage,_14);
},buildRendering:function(){
this.inherited(arguments);
if(!this.containerNode){
this.containerNode=this.domNode;
}
this.domNode.removeAttribute("title");
},startup:function(){
this.inherited(arguments);
if(this._contentSetter){
_9.forEach(this._contentSetter.parseResults,function(obj){
if(!obj._started&&!obj._destroyed&&_2.isFunction(obj.startup)){
obj.startup();
obj._started=true;
}
},this);
}
},_startChildren:function(){
_9.forEach(this.getChildren(),function(obj){
if(!obj._started&&!obj._destroyed&&_2.isFunction(obj.startup)){
obj.startup();
obj._started=true;
}
});
if(this._contentSetter){
_9.forEach(this._contentSetter.parseResults,function(obj){
if(!obj._started&&!obj._destroyed&&_2.isFunction(obj.startup)){
obj.startup();
obj._started=true;
}
},this);
}
},setHref:function(_15){
_1.deprecated("dijit.layout.ContentPane.setHref() is deprecated. Use set('href', ...) instead.","","2.0");
return this.set("href",_15);
},_setHrefAttr:function(_16){
this.cancel();
this.onLoadDeferred=new _b(_2.hitch(this,"cancel"));
this.onLoadDeferred.then(_2.hitch(this,"onLoad"));
this._set("href",_16);
if(this.preload||(this._created&&this._isShown())){
this._load();
}else{
this._hrefChanged=true;
}
return this.onLoadDeferred;
},setContent:function(_17){
_1.deprecated("dijit.layout.ContentPane.setContent() is deprecated.  Use set('content', ...) instead.","","2.0");
this.set("content",_17);
},_setContentAttr:function(_18){
this._set("href","");
this.cancel();
this.onLoadDeferred=new _b(_2.hitch(this,"cancel"));
if(this._created){
this.onLoadDeferred.then(_2.hitch(this,"onLoad"));
}
this._setContent(_18||"");
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
},destroy:function(){
this.cancel();
this.inherited(arguments);
},destroyRecursive:function(_19){
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
this.onLoadDeferred=new _b(_2.hitch(this,"cancel"));
this.onLoadDeferred.then(_2.hitch(this,"onLoad"));
this._load();
return this.onLoadDeferred;
},_load:function(){
this._setContent(this.onDownloadStart(),true);
var _1a=this;
var _1b={preventCache:(this.preventCache||this.refreshOnShow),url:this.href,handleAs:"text"};
if(_2.isObject(this.ioArgs)){
_2.mixin(_1b,this.ioArgs);
}
var _1c=(this._xhrDfd=(this.ioMethod||_f.get)(_1b)),_1d;
_1c.then(function(_1e){
_1d=_1e;
try{
_1a._isDownloaded=true;
return _1a._setContent(_1e,false);
}
catch(err){
_1a._onError("Content",err);
}
},function(err){
if(!_1c.canceled){
_1a._onError("Download",err);
}
delete _1a._xhrDfd;
return err;
}).then(function(){
_1a.onDownloadEnd();
delete _1a._xhrDfd;
return _1d;
});
delete this._hrefChanged;
},_onLoadHandler:function(_1f){
this._set("isLoaded",true);
try{
this.onLoadDeferred.resolve(_1f);
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
},destroyDescendants:function(_20){
if(this.isLoaded){
this._onUnloadHandler();
}
var _21=this._contentSetter;
_9.forEach(this.getChildren(),function(_22){
if(_22.destroyRecursive){
_22.destroyRecursive(_20);
}else{
if(_22.destroy){
_22.destroy(_20);
}
}
_22._destroyed=true;
});
if(_21){
_9.forEach(_21.parseResults,function(_23){
if(!_23._destroyed){
if(_23.destroyRecursive){
_23.destroyRecursive(_20);
}else{
if(_23.destroy){
_23.destroy(_20);
}
}
_23._destroyed=true;
}
});
delete _21.parseResults;
}
if(!_20){
_e.empty(this.containerNode);
}
delete this._singleChild;
},_setContent:function(_24,_25){
this.destroyDescendants();
var _26=this._contentSetter;
if(!(_26&&_26 instanceof _7._ContentSetter)){
_26=this._contentSetter=new _7._ContentSetter({node:this.containerNode,_onError:_2.hitch(this,this._onError),onContentError:_2.hitch(this,function(e){
var _27=this.onContentError(e);
try{
this.containerNode.innerHTML=_27;
}
catch(e){
console.error("Fatal "+this.id+" could not change content due to "+e.message,e);
}
})});
}
var _28=_2.mixin({cleanContent:this.cleanContent,extractContent:this.extractContent,parseContent:!_24.domNode&&this.parseOnLoad,parserScope:this.parserScope,startup:false,dir:this.dir,lang:this.lang,textDir:this.textDir},this._contentSetterParams||{});
var p=_26.set((_2.isObject(_24)&&_24.domNode)?_24.domNode:_24,_28);
var _29=this;
return _11(p&&p.then?p:_26.parseDeferred,function(){
delete _29._contentSetterParams;
if(!_25){
if(_29._started){
_29._startChildren();
_29._scheduleLayout();
}
_29._onLoadHandler(_24);
}
});
},_onError:function(_2a,err,_2b){
this.onLoadDeferred.reject(err);
var _2c=this["on"+_2a+"Error"].call(this,err);
if(_2b){
console.error(_2b,err);
}else{
if(_2c){
this._setContent(_2c,true);
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
