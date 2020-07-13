//>>built
define("dijit/layout/ContentPane",["dojo/_base/kernel","dojo/_base/lang","../_Widget","../_Container","./_ContentPaneResizeMixin","dojo/string","dojo/html","dojo/_base/array","dojo/_base/declare","dojo/_base/Deferred","dojo/dom","dojo/dom-attr","dojo/dom-construct","dojo/_base/xhr","dojo/i18n","dojo/when","dojo/i18n!../nls/loading"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
return _9("dijit.layout.ContentPane",[_3,_4,_5],{href:"",content:"",extractContent:false,parseOnLoad:true,parserScope:_1._scopeName,preventCache:false,preload:false,refreshOnShow:false,loadingMessage:"<span class='dijitContentPaneLoading'><span class='dijitInline dijitIconLoading'></span>${loadingState}</span>",errorMessage:"<span class='dijitContentPaneError'><span class='dijitInline dijitIconError'></span>${errorState}</span>",isLoaded:false,baseClass:"dijitContentPane",ioArgs:{},onLoadDeferred:null,_setTitleAttr:null,stopParser:true,template:false,markupFactory:function(_11,_12,_13){
var _14=new _13(_11,_12);
return !_14.href&&_14._contentSetter&&_14._contentSetter.parseDeferred&&!_14._contentSetter.parseDeferred.isFulfilled()?_14._contentSetter.parseDeferred.then(function(){
return _14;
}):_14;
},create:function(_15,_16){
if((!_15||!_15.template)&&_16&&!("href" in _15)&&!("content" in _15)){
_16=_b.byId(_16);
var df=_16.ownerDocument.createDocumentFragment();
while(_16.firstChild){
df.appendChild(_16.firstChild);
}
_15=_2.delegate(_15,{content:df});
}
this.inherited(arguments,[_15,_16]);
},postMixInProperties:function(){
this.inherited(arguments);
var _17=_f.getLocalization("dijit","loading",this.lang);
this.loadingMessage=_6.substitute(this.loadingMessage,_17);
this.errorMessage=_6.substitute(this.errorMessage,_17);
},buildRendering:function(){
this.inherited(arguments);
if(!this.containerNode){
this.containerNode=this.domNode;
}
this.domNode.removeAttribute("title");
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
},_startChildren:function(){
_8.forEach(this.getChildren(),function(obj){
if(!obj._started&&!obj._destroyed&&_2.isFunction(obj.startup)){
obj.startup();
obj._started=true;
}
});
if(this._contentSetter){
_8.forEach(this._contentSetter.parseResults,function(obj){
if(!obj._started&&!obj._destroyed&&_2.isFunction(obj.startup)){
obj.startup();
obj._started=true;
}
},this);
}
},setHref:function(_18){
_1.deprecated("dijit.layout.ContentPane.setHref() is deprecated. Use set('href', ...) instead.","","2.0");
return this.set("href",_18);
},_setHrefAttr:function(_19){
this.cancel();
this.onLoadDeferred=new _a(_2.hitch(this,"cancel"));
this.onLoadDeferred.then(_2.hitch(this,"onLoad"));
this._set("href",_19);
if(this.preload||(this._created&&this._isShown())){
this._load();
}else{
this._hrefChanged=true;
}
return this.onLoadDeferred;
},setContent:function(_1a){
_1.deprecated("dijit.layout.ContentPane.setContent() is deprecated.  Use set('content', ...) instead.","","2.0");
this.set("content",_1a);
},_setContentAttr:function(_1b){
this._set("href","");
this.cancel();
this.onLoadDeferred=new _a(_2.hitch(this,"cancel"));
if(this._created){
this.onLoadDeferred.then(_2.hitch(this,"onLoad"));
}
this._setContent(_1b||"");
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
},destroyRecursive:function(_1c){
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
this.onLoadDeferred.then(_2.hitch(this,"onLoad"));
this._load();
return this.onLoadDeferred;
},_load:function(){
this._setContent(this.onDownloadStart(),true);
var _1d=this;
var _1e={preventCache:(this.preventCache||this.refreshOnShow),url:this.href,handleAs:"text"};
if(_2.isObject(this.ioArgs)){
_2.mixin(_1e,this.ioArgs);
}
var _1f=(this._xhrDfd=(this.ioMethod||_e.get)(_1e)),_20;
_1f.then(function(_21){
_20=_21;
try{
_1d._isDownloaded=true;
return _1d._setContent(_21,false);
}
catch(err){
_1d._onError("Content",err);
}
},function(err){
if(!_1f.canceled){
_1d._onError("Download",err);
}
delete _1d._xhrDfd;
return err;
}).then(function(){
_1d.onDownloadEnd();
delete _1d._xhrDfd;
return _20;
});
delete this._hrefChanged;
},_onLoadHandler:function(_22){
this._set("isLoaded",true);
try{
this.onLoadDeferred.resolve(_22);
}
catch(e){
console.error("Error "+(this.widgetId||this.id)+" running custom onLoad code: "+e.message);
}
},_onUnloadHandler:function(){
this._set("isLoaded",false);
try{
this.onUnload();
}
catch(e){
console.error("Error "+this.widgetId+" running custom onUnload code: "+e.message);
}
},destroyDescendants:function(_23){
if(this.isLoaded){
this._onUnloadHandler();
}
var _24=this._contentSetter;
_8.forEach(this.getChildren(),function(_25){
if(_25.destroyRecursive){
_25.destroyRecursive(_23);
}else{
if(_25.destroy){
_25.destroy(_23);
}
}
_25._destroyed=true;
});
if(_24){
_8.forEach(_24.parseResults,function(_26){
if(!_26._destroyed){
if(_26.destroyRecursive){
_26.destroyRecursive(_23);
}else{
if(_26.destroy){
_26.destroy(_23);
}
}
_26._destroyed=true;
}
});
delete _24.parseResults;
}
if(!_23){
_d.empty(this.containerNode);
}
delete this._singleChild;
},_setContent:function(_27,_28){
_27=this.preprocessContent(_27);
this.destroyDescendants();
var _29=this._contentSetter;
if(!(_29&&_29 instanceof _7._ContentSetter)){
_29=this._contentSetter=new _7._ContentSetter({node:this.containerNode,_onError:_2.hitch(this,this._onError),onContentError:_2.hitch(this,function(e){
var _2a=this.onContentError(e);
try{
this.containerNode.innerHTML=_2a;
}
catch(e){
console.error("Fatal "+this.id+" could not change content due to "+e.message,e);
}
})});
}
var _2b=_2.mixin({cleanContent:this.cleanContent,extractContent:this.extractContent,parseContent:!_27.domNode&&this.parseOnLoad,parserScope:this.parserScope,startup:false,dir:this.dir,lang:this.lang,textDir:this.textDir},this._contentSetterParams||{});
var p=_29.set((_2.isObject(_27)&&_27.domNode)?_27.domNode:_27,_2b);
var _2c=this;
return _10(p&&p.then?p:_29.parseDeferred,function(){
delete _2c._contentSetterParams;
if(!_28){
if(_2c._started){
_2c._startChildren();
_2c._scheduleLayout();
}
_2c._onLoadHandler(_27);
}
});
},preprocessContent:function(_2d){
return _2d;
},_onError:function(_2e,err,_2f){
this.onLoadDeferred.reject(err);
var _30=this["on"+_2e+"Error"].call(this,err);
if(_2f){
console.error(_2f,err);
}else{
if(_30){
this._setContent(_30,true);
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
