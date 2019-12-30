//>>built
define("dojox/grid/EnhancedGrid",["dojo/_base/kernel","../main","dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/sniff","dojo/dom","dojo/dom-geometry","./DataGrid","./DataSelection","./enhanced/_PluginManager","./enhanced/plugins/_SelectionPreserver","dojo/i18n!./enhanced/nls/EnhancedGrid"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
_1.experimental("dojox.grid.EnhancedGrid");
var _e=_3("dojox.grid.EnhancedGrid",_9,{plugins:null,pluginMgr:null,_pluginMgrClass:_b,postMixInProperties:function(){
this._nls=_d;
this.inherited(arguments);
},postCreate:function(){
this.pluginMgr=new this._pluginMgrClass(this);
this.pluginMgr.preInit();
this.inherited(arguments);
this.pluginMgr.postInit();
},plugin:function(_f){
return this.pluginMgr.getPlugin(_f);
},startup:function(){
this.inherited(arguments);
this.pluginMgr.startup();
},createSelection:function(){
this.selection=new _2.grid.enhanced.DataSelection(this);
},canSort:function(_10,_11){
return true;
},doKeyEvent:function(e){
try{
var _12=this.focus.focusView;
_12.content.decorateEvent(e);
if(!e.cell){
_12.header.decorateEvent(e);
}
}
catch(e){
}
this.inherited(arguments);
},doApplyCellEdit:function(_13,_14,_15){
if(!_15){
this.invalidated[_14]=true;
return;
}
this.inherited(arguments);
},mixin:function(_16,_17){
var _18={};
for(var p in _17){
if(p=="_inherited"||p=="declaredClass"||p=="constructor"||_17["privates"]&&_17["privates"][p]){
continue;
}
_18[p]=_17[p];
}
_4.mixin(_16,_18);
},_copyAttr:function(idx,_19){
if(!_19){
return;
}
return this.inherited(arguments);
},_getHeaderHeight:function(){
this.inherited(arguments);
return _8.getMarginBox(this.viewsHeaderNode).h;
},_fetch:function(_1a,_1b){
if(this.items){
return this.inherited(arguments);
}
_1a=_1a||0;
if(this.store&&!this._pending_requests[_1a]){
if(!this._isLoaded&&!this._isLoading){
this._isLoading=true;
this.showMessage(this.loadingMessage);
}
this._pending_requests[_1a]=true;
try{
var req={start:_1a,count:this.rowsPerPage,query:this.query,sort:this.getSortProps(),queryOptions:this.queryOptions,isRender:_1b,onBegin:_4.hitch(this,"_onFetchBegin"),onComplete:_4.hitch(this,"_onFetchComplete"),onError:_4.hitch(this,"_onFetchError")};
this._storeLayerFetch(req);
}
catch(e){
this._onFetchError(e,{start:_1a,count:this.rowsPerPage});
}
}
return 0;
},_storeLayerFetch:function(req){
this.store.fetch(req);
},getCellByField:function(_1c){
return _5.filter(this.layout.cells,function(_1d){
return _1d.field==_1c;
})[0];
},onMouseUp:function(e){
},createView:function(){
var _1e=this.inherited(arguments);
if(_6("mozilla")){
var _1f=function(_20,_21){
for(var n=_20;n&&_21(n);n=n.parentNode){
}
return n;
};
var _22=function(_23){
var _24=_23.toUpperCase();
return function(_25){
return _25.tagName!=_24;
};
};
var _26=_1e.header.getCellX;
_1e.header.getCellX=function(e){
var x=_26.call(_1e.header,e);
var n=_1f(e.target,_22("th"));
if(n&&n!==e.target&&_7.isDescendant(e.target,n)){
x+=n.firstChild.offsetLeft;
}
return x;
};
}
return _1e;
},destroy:function(){
delete this._nls;
this.pluginMgr.destroy();
this.inherited(arguments);
}});
_3("dojox.grid.enhanced.DataSelection",_a,{constructor:function(_27){
if(_27.keepSelection){
if(this.preserver){
this.preserver.destroy();
}
this.preserver=new _c(this);
}
},_range:function(_28,_29){
this.grid._selectingRange=true;
this.inherited(arguments);
this.grid._selectingRange=false;
this.onChanged();
},deselectAll:function(_2a){
this.grid._selectingRange=true;
this.inherited(arguments);
this.grid._selectingRange=false;
this.onChanged();
}});
_e.markupFactory=function(_2b,_2c,_2d,_2e){
return _2.grid._Grid.markupFactory(_2b,_2c,_2d,_4.partial(_9.cell_markupFactory,_2e));
};
_e.registerPlugin=function(_2f,_30){
_b.registerPlugin(_2f,_30);
};
return _e;
});
