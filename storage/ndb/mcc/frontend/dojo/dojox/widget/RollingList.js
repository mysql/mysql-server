//>>built
define(["dijit","dojo","dojox","dojo/i18n!dijit/nls/common","dojo/require!dojo/window,dijit/layout/ContentPane,dijit/_Templated,dijit/_Contained,dijit/layout/_LayoutWidget,dijit/Menu,dijit/form/Button,dijit/focus,dijit/_base/focus,dojox/html/metrics,dojo/i18n"],function(_1,_2,_3){
_2.provide("dojox.widget.RollingList");
_2.experimental("dojox.widget.RollingList");
_2.require("dojo.window");
_2.require("dijit.layout.ContentPane");
_2.require("dijit._Templated");
_2.require("dijit._Contained");
_2.require("dijit.layout._LayoutWidget");
_2.require("dijit.Menu");
_2.require("dijit.form.Button");
_2.require("dijit.focus");
_2.require("dijit._base.focus");
_2.require("dojox.html.metrics");
_2.require("dojo.i18n");
_2.requireLocalization("dijit","common");
_2.declare("dojox.widget._RollingListPane",[_1.layout.ContentPane,_1._Templated,_1._Contained],{templateString:"<div class=\"dojoxRollingListPane\"><table><tbody><tr><td dojoAttachPoint=\"containerNode\"></td></tr></tbody></div>",parentWidget:null,parentPane:null,store:null,items:null,query:null,queryOptions:null,_focusByNode:true,minWidth:0,_setContentAndScroll:function(_4,_5){
this._setContent(_4,_5);
this.parentWidget.scrollIntoView(this);
},_updateNodeWidth:function(n,_6){
n.style.width="";
var _7=_2.marginBox(n).w;
if(_7<_6){
_2.marginBox(n,{w:_6});
}
},_onMinWidthChange:function(v){
this._updateNodeWidth(this.domNode,v);
},_setMinWidthAttr:function(v){
if(v!==this.minWidth){
this.minWidth=v;
this._onMinWidthChange(v);
}
},startup:function(){
if(this._started){
return;
}
if(this.store&&this.store.getFeatures()["dojo.data.api.Notification"]){
window.setTimeout(_2.hitch(this,function(){
this.connect(this.store,"onSet","_onSetItem");
this.connect(this.store,"onNew","_onNewItem");
this.connect(this.store,"onDelete","_onDeleteItem");
}),1);
}
this.connect(this.focusNode||this.domNode,"onkeypress","_focusKey");
this.parentWidget._updateClass(this.domNode,"Pane");
this.inherited(arguments);
this._onMinWidthChange(this.minWidth);
},_focusKey:function(e){
if(e.charOrCode==_2.keys.BACKSPACE){
_2.stopEvent(e);
return;
}else{
if(e.charOrCode==_2.keys.LEFT_ARROW&&this.parentPane){
this.parentPane.focus();
this.parentWidget.scrollIntoView(this.parentPane);
}else{
if(e.charOrCode==_2.keys.ENTER){
this.parentWidget._onExecute();
}
}
}
},focus:function(_8){
if(this.parentWidget._focusedPane!=this){
this.parentWidget._focusedPane=this;
this.parentWidget.scrollIntoView(this);
if(this._focusByNode&&(!this.parentWidget._savedFocus||_8)){
try{
(this.focusNode||this.domNode).focus();
}
catch(e){
}
}
}
},_onShow:function(){
if((this.store||this.items)&&((this.refreshOnShow&&this.domNode)||(!this.isLoaded&&this.domNode))){
this.refresh();
}
},_load:function(){
this.isLoaded=false;
if(this.items){
this._setContentAndScroll(this.onLoadStart(),true);
window.setTimeout(_2.hitch(this,"_doQuery"),1);
}else{
this._doQuery();
}
},_doLoadItems:function(_9,_a){
var _b=0,_c=this.store;
_2.forEach(_9,function(_d){
if(!_c.isItemLoaded(_d)){
_b++;
}
});
if(_b===0){
_a();
}else{
var _e=function(_f){
_b--;
if((_b)===0){
_a();
}
};
_2.forEach(_9,function(_10){
if(!_c.isItemLoaded(_10)){
_c.loadItem({item:_10,onItem:_e});
}
});
}
},_doQuery:function(){
if(!this.domNode){
return;
}
var _11=this.parentWidget.preloadItems;
_11=(_11===true||(this.items&&this.items.length<=Number(_11)));
if(this.items&&_11){
this._doLoadItems(this.items,_2.hitch(this,"onItems"));
}else{
if(this.items){
this.onItems();
}else{
this._setContentAndScroll(this.onFetchStart(),true);
this.store.fetch({query:this.query,onComplete:function(_12){
this.items=_12;
this.onItems();
},onError:function(e){
this._onError("Fetch",e);
},scope:this});
}
}
},_hasItem:function(_13){
var _14=this.items||[];
for(var i=0,_15;(_15=_14[i]);i++){
if(this.parentWidget._itemsMatch(_15,_13)){
return true;
}
}
return false;
},_onSetItem:function(_16,_17,_18,_19){
if(this._hasItem(_16)){
this.refresh();
}
},_onNewItem:function(_1a,_1b){
var sel;
if((!_1b&&!this.parentPane)||(_1b&&this.parentPane&&this.parentPane._hasItem(_1b.item)&&(sel=this.parentPane._getSelected())&&this.parentWidget._itemsMatch(sel.item,_1b.item))){
this.items.push(_1a);
this.refresh();
}else{
if(_1b&&this.parentPane&&this._hasItem(_1b.item)){
this.refresh();
}
}
},_onDeleteItem:function(_1c){
if(this._hasItem(_1c)){
this.items=_2.filter(this.items,function(i){
return (i!=_1c);
});
this.refresh();
}
},onFetchStart:function(){
return this.loadingMessage;
},onFetchError:function(_1d){
return this.errorMessage;
},onLoadStart:function(){
return this.loadingMessage;
},onLoadError:function(_1e){
return this.errorMessage;
},onItems:function(){
if(!this.onLoadDeferred){
this.cancel();
this.onLoadDeferred=new _2.Deferred(_2.hitch(this,"cancel"));
}
this._onLoadHandler();
}});
_2.declare("dojox.widget._RollingListGroupPane",[_3.widget._RollingListPane],{templateString:"<div><div dojoAttachPoint=\"containerNode\"></div>"+"<div dojoAttachPoint=\"menuContainer\">"+"<div dojoAttachPoint=\"menuNode\"></div>"+"</div></div>",_menu:null,_setContent:function(_1f){
if(!this._menu){
this.inherited(arguments);
}
},_onMinWidthChange:function(v){
if(!this._menu){
return;
}
var _20=_2.marginBox(this.domNode).w;
var _21=_2.marginBox(this._menu.domNode).w;
this._updateNodeWidth(this._menu.domNode,v-(_20-_21));
},onItems:function(){
var _22,_23=false;
if(this._menu){
_22=this._getSelected();
this._menu.destroyRecursive();
}
this._menu=this._getMenu();
var _24,_25;
if(this.items.length){
_2.forEach(this.items,function(_26){
_24=this.parentWidget._getMenuItemForItem(_26,this);
if(_24){
if(_22&&this.parentWidget._itemsMatch(_24.item,_22.item)){
_25=_24;
}
this._menu.addChild(_24);
}
},this);
}else{
_24=this.parentWidget._getMenuItemForItem(null,this);
if(_24){
this._menu.addChild(_24);
}
}
if(_25){
this._setSelected(_25);
if((_22&&!_22.children&&_25.children)||(_22&&_22.children&&!_25.children)){
var _27=this.parentWidget._getPaneForItem(_25.item,this,_25.children);
if(_27){
this.parentWidget.addChild(_27,this.getIndexInParent()+1);
}else{
this.parentWidget._removeAfter(this);
this.parentWidget._onItemClick(null,this,_25.item,_25.children);
}
}
}else{
if(_22){
this.parentWidget._removeAfter(this);
}
}
this.containerNode.innerHTML="";
this.containerNode.appendChild(this._menu.domNode);
this.parentWidget.scrollIntoView(this);
this._checkScrollConnection(true);
this.inherited(arguments);
this._onMinWidthChange(this.minWidth);
},_checkScrollConnection:function(_28){
var _29=this.store;
if(this._scrollConn){
this.disconnect(this._scrollConn);
}
delete this._scrollConn;
if(!_2.every(this.items,function(i){
return _29.isItemLoaded(i);
})){
if(_28){
this._loadVisibleItems();
}
this._scrollConn=this.connect(this.domNode,"onscroll","_onScrollPane");
}
},startup:function(){
this.inherited(arguments);
this.parentWidget._updateClass(this.domNode,"GroupPane");
},focus:function(_2a){
if(this._menu){
if(this._pendingFocus){
this.disconnect(this._pendingFocus);
}
delete this._pendingFocus;
var _2b=this._menu.focusedChild;
if(!_2b){
var _2c=_2.query(".dojoxRollingListItemSelected",this.domNode)[0];
if(_2c){
_2b=_1.byNode(_2c);
}
}
if(!_2b){
_2b=this._menu.getChildren()[0]||this._menu;
}
this._focusByNode=false;
if(_2b.focusNode){
if(!this.parentWidget._savedFocus||_2a){
try{
_2b.focusNode.focus();
}
catch(e){
}
}
window.setTimeout(function(){
try{
_2.window.scrollIntoView(_2b.focusNode);
}
catch(e){
}
},1);
}else{
if(_2b.focus){
if(!this.parentWidget._savedFocus||_2a){
_2b.focus();
}
}else{
this._focusByNode=true;
}
}
this.inherited(arguments);
}else{
if(!this._pendingFocus){
this._pendingFocus=this.connect(this,"onItems","focus");
}
}
},_getMenu:function(){
var _2d=this;
var _2e=new _1.Menu({parentMenu:this.parentPane?this.parentPane._menu:null,onCancel:function(_2f){
if(_2d.parentPane){
_2d.parentPane.focus(true);
}
},_moveToPopup:function(evt){
if(this.focusedChild&&!this.focusedChild.disabled){
this.focusedChild._onClick(evt);
}
}},this.menuNode);
this.connect(_2e,"onItemClick",function(_30,evt){
if(_30.disabled){
return;
}
evt.alreadySelected=_2.hasClass(_30.domNode,"dojoxRollingListItemSelected");
if(evt.alreadySelected&&((evt.type=="keypress"&&evt.charOrCode!=_2.keys.ENTER)||(evt.type=="internal"))){
var p=this.parentWidget.getChildren()[this.getIndexInParent()+1];
if(p){
p.focus(true);
this.parentWidget.scrollIntoView(p);
}
}else{
this._setSelected(_30,_2e);
this.parentWidget._onItemClick(evt,this,_30.item,_30.children);
if(evt.type=="keypress"&&evt.charOrCode==_2.keys.ENTER){
this.parentWidget._onExecute();
}
}
});
if(!_2e._started){
_2e.startup();
}
return _2e;
},_onScrollPane:function(){
if(this._visibleLoadPending){
window.clearTimeout(this._visibleLoadPending);
}
this._visibleLoadPending=window.setTimeout(_2.hitch(this,"_loadVisibleItems"),500);
},_loadVisibleItems:function(){
delete this._visibleLoadPending;
var _31=this._menu;
if(!_31){
return;
}
var _32=_31.getChildren();
if(!_32||!_32.length){
return;
}
var _33=function(n,m,pb){
var s=_2.getComputedStyle(n);
var r=0;
if(m){
r+=_2._getMarginExtents(n,s).t;
}
if(pb){
r+=_2._getPadBorderExtents(n,s).t;
}
return r;
};
var _34=_33(this.domNode,false,true)+_33(this.containerNode,true,true)+_33(_31.domNode,true,true)+_33(_32[0].domNode,true,false);
var h=_2.contentBox(this.domNode).h;
var _35=this.domNode.scrollTop-_34-(h/2);
var _36=_35+(3*h/2);
var _37=_2.filter(_32,function(c){
var cnt=c.domNode.offsetTop;
var s=c.store;
var i=c.item;
return (cnt>=_35&&cnt<=_36&&!s.isItemLoaded(i));
});
var _38=_2.map(_37,function(c){
return c.item;
});
var _39=_2.hitch(this,function(){
var _3a=this._getSelected();
var _3b;
_2.forEach(_38,function(_3c,idx){
var _3d=this.parentWidget._getMenuItemForItem(_3c,this);
var _3e=_37[idx];
var _3f=_3e.getIndexInParent();
_31.removeChild(_3e);
if(_3d){
if(_3a&&this.parentWidget._itemsMatch(_3d.item,_3a.item)){
_3b=_3d;
}
_31.addChild(_3d,_3f);
if(_31.focusedChild==_3e){
_31.focusChild(_3d);
}
}
_3e.destroy();
},this);
this._checkScrollConnection(false);
});
this._doLoadItems(_38,_39);
},_getSelected:function(_40){
if(!_40){
_40=this._menu;
}
if(_40){
var _41=this._menu.getChildren();
for(var i=0,_42;(_42=_41[i]);i++){
if(_2.hasClass(_42.domNode,"dojoxRollingListItemSelected")){
return _42;
}
}
}
return null;
},_setSelected:function(_43,_44){
if(!_44){
_44=this._menu;
}
if(_44){
_2.forEach(_44.getChildren(),function(i){
this.parentWidget._updateClass(i.domNode,"Item",{"Selected":(_43&&(i==_43&&!i.disabled))});
},this);
}
}});
_2.declare("dojox.widget.RollingList",[_1._Widget,_1._Templated,_1._Container],{templateString:_2.cache("dojox.widget","RollingList/RollingList.html","<div class=\"dojoxRollingList ${className}\"\n\t><div class=\"dojoxRollingListContainer\" dojoAttachPoint=\"containerNode\" dojoAttachEvent=\"onkeypress:_onKey\"\n\t></div\n\t><div class=\"dojoxRollingListButtons\" dojoAttachPoint=\"buttonsNode\"\n        ><button dojoType=\"dijit.form.Button\" dojoAttachPoint=\"okButton\"\n\t\t\t\tdojoAttachEvent=\"onClick:_onExecute\">${okButtonLabel}</button\n        ><button dojoType=\"dijit.form.Button\" dojoAttachPoint=\"cancelButton\"\n\t\t\t\tdojoAttachEvent=\"onClick:_onCancel\">${cancelButtonLabel}</button\n\t></div\n></div>\n"),widgetsInTemplate:true,className:"",store:null,query:null,queryOptions:null,childrenAttrs:["children"],parentAttr:"",value:null,executeOnDblClick:true,preloadItems:false,showButtons:false,okButtonLabel:"",cancelButtonLabel:"",minPaneWidth:0,postMixInProperties:function(){
this.inherited(arguments);
var loc=_2.i18n.getLocalization("dijit","common");
this.okButtonLabel=this.okButtonLabel||loc.buttonOk;
this.cancelButtonLabel=this.cancelButtonLabel||loc.buttonCancel;
},_setShowButtonsAttr:function(_45){
var _46=false;
if((this.showButtons!=_45&&this._started)||(this.showButtons==_45&&!this.started)){
_46=true;
}
_2.toggleClass(this.domNode,"dojoxRollingListButtonsHidden",!_45);
this.showButtons=_45;
if(_46){
if(this._started){
this.layout();
}else{
window.setTimeout(_2.hitch(this,"layout"),0);
}
}
},_itemsMatch:function(_47,_48){
if(!_47&&!_48){
return true;
}else{
if(!_47||!_48){
return false;
}
}
return (_47==_48||(this._isIdentity&&this.store.getIdentity(_47)==this.store.getIdentity(_48)));
},_removeAfter:function(idx){
if(typeof idx!="number"){
idx=this.getIndexOfChild(idx);
}
if(idx>=0){
_2.forEach(this.getChildren(),function(c,i){
if(i>idx){
this.removeChild(c);
c.destroyRecursive();
}
},this);
}
var _49=this.getChildren(),_4a=_49[_49.length-1];
var _4b=null;
while(_4a&&!_4b){
var val=_4a._getSelected?_4a._getSelected():null;
if(val){
_4b=val.item;
}
_4a=_4a.parentPane;
}
if(!this._setInProgress){
this._setValue(_4b);
}
},addChild:function(_4c,_4d){
if(_4d>0){
this._removeAfter(_4d-1);
}
this.inherited(arguments);
if(!_4c._started){
_4c.startup();
}
_4c.attr("minWidth",this.minPaneWidth);
this.layout();
if(!this._savedFocus){
_4c.focus();
}
},_setMinPaneWidthAttr:function(_4e){
if(_4e!==this.minPaneWidth){
this.minPaneWidth=_4e;
_2.forEach(this.getChildren(),function(c){
c.attr("minWidth",_4e);
});
}
},_updateClass:function(_4f,_50,_51){
if(!this._declaredClasses){
this._declaredClasses=("dojoxRollingList "+this.className).split(" ");
}
_2.forEach(this._declaredClasses,function(c){
if(c){
_2.addClass(_4f,c+_50);
for(var k in _51||{}){
_2.toggleClass(_4f,c+_50+k,_51[k]);
}
_2.toggleClass(_4f,c+_50+"FocusSelected",(_2.hasClass(_4f,c+_50+"Focus")&&_2.hasClass(_4f,c+_50+"Selected")));
_2.toggleClass(_4f,c+_50+"HoverSelected",(_2.hasClass(_4f,c+_50+"Hover")&&_2.hasClass(_4f,c+_50+"Selected")));
}
});
},scrollIntoView:function(_52){
if(this._scrollingTimeout){
window.clearTimeout(this._scrollingTimeout);
}
delete this._scrollingTimeout;
this._scrollingTimeout=window.setTimeout(_2.hitch(this,function(){
if(_52.domNode){
_2.window.scrollIntoView(_52.domNode);
}
delete this._scrollingTimeout;
return;
}),1);
},resize:function(_53){
_1.layout._LayoutWidget.prototype.resize.call(this,_53);
},layout:function(){
var _54=this.getChildren();
if(this._contentBox){
var bn=this.buttonsNode;
var _55=this._contentBox.h-_2.marginBox(bn).h-_3.html.metrics.getScrollbar().h;
_2.forEach(_54,function(c){
_2.marginBox(c.domNode,{h:_55});
});
}
if(this._focusedPane){
var foc=this._focusedPane;
delete this._focusedPane;
if(!this._savedFocus){
foc.focus();
}
}else{
if(_54&&_54.length){
if(!this._savedFocus){
_54[0].focus();
}
}
}
},_onChange:function(_56){
this.onChange(_56);
},_setValue:function(_57){
delete this._setInProgress;
if(!this._itemsMatch(this.value,_57)){
this.value=_57;
this._onChange(_57);
}
},_setValueAttr:function(_58){
if(this._itemsMatch(this.value,_58)&&!_58){
return;
}
if(this._setInProgress&&this._setInProgress===_58){
return;
}
this._setInProgress=_58;
if(!_58||!this.store.isItem(_58)){
var _59=this.getChildren()[0];
_59._setSelected(null);
this._onItemClick(null,_59,null,null);
return;
}
var _5a=_2.hitch(this,function(_5b,_5c){
var _5d=this.store,id;
if(this.parentAttr&&_5d.getFeatures()["dojo.data.api.Identity"]&&((id=this.store.getValue(_5b,this.parentAttr))||id==="")){
var cb=function(i){
if(_5d.getIdentity(i)==_5d.getIdentity(_5b)){
_5c(null);
}else{
_5c([i]);
}
};
if(id===""){
_5c(null);
}else{
if(typeof id=="string"){
_5d.fetchItemByIdentity({identity:id,onItem:cb});
}else{
if(_5d.isItem(id)){
cb(id);
}
}
}
}else{
var _5e=this.childrenAttrs.length;
var _5f=[];
_2.forEach(this.childrenAttrs,function(_60){
var q={};
q[_60]=_5b;
_5d.fetch({query:q,scope:this,onComplete:function(_61){
if(this._setInProgress!==_58){
return;
}
_5f=_5f.concat(_61);
_5e--;
if(_5e===0){
_5c(_5f);
}
}});
},this);
}
});
var _62=_2.hitch(this,function(_63,idx){
var set=_63[idx];
var _64=this.getChildren()[idx];
var _65;
if(set&&_64){
var fx=_2.hitch(this,function(){
if(_65){
this.disconnect(_65);
}
delete _65;
if(this._setInProgress!==_58){
return;
}
var _66=_2.filter(_64._menu.getChildren(),function(i){
return this._itemsMatch(i.item,set);
},this)[0];
if(_66){
idx++;
_64._menu.onItemClick(_66,{type:"internal",stopPropagation:function(){
},preventDefault:function(){
}});
if(_63[idx]){
_62(_63,idx);
}else{
this._setValue(set);
this.onItemClick(set,_64,this.getChildItems(set));
}
}
});
if(!_64.isLoaded){
_65=this.connect(_64,"onLoad",fx);
}else{
fx();
}
}else{
if(idx===0){
this.set("value",null);
}
}
});
var _67=[];
var _68=_2.hitch(this,function(_69){
if(_69&&_69.length){
_67.push(_69[0]);
_5a(_69[0],_68);
}else{
if(!_69){
_67.pop();
}
_67.reverse();
_62(_67,0);
}
});
var ns=this.domNode.style;
if(ns.display=="none"||ns.visibility=="hidden"){
this._setValue(_58);
}else{
if(!this._itemsMatch(_58,this._visibleItem)){
_68([_58]);
}
}
},_onItemClick:function(evt,_6a,_6b,_6c){
if(evt){
var _6d=this._getPaneForItem(_6b,_6a,_6c);
var _6e=(evt.type=="click"&&evt.alreadySelected);
if(_6e&&_6d){
this._removeAfter(_6a.getIndexInParent()+1);
var _6f=_6a.getNextSibling();
if(_6f&&_6f._setSelected){
_6f._setSelected(null);
}
this.scrollIntoView(_6f);
}else{
if(_6d){
this.addChild(_6d,_6a.getIndexInParent()+1);
if(this._savedFocus){
_6d.focus(true);
}
}else{
this._removeAfter(_6a);
this.scrollIntoView(_6a);
}
}
}else{
if(_6a){
this._removeAfter(_6a);
this.scrollIntoView(_6a);
}
}
if(!evt||evt.type!="internal"){
this._setValue(_6b);
this.onItemClick(_6b,_6a,_6c);
}
this._visibleItem=_6b;
},_getPaneForItem:function(_70,_71,_72){
var ret=this.getPaneForItem(_70,_71,_72);
ret.store=this.store;
ret.parentWidget=this;
ret.parentPane=_71||null;
if(!_70){
ret.query=this.query;
ret.queryOptions=this.queryOptions;
}else{
if(_72){
ret.items=_72;
}else{
ret.items=[_70];
}
}
return ret;
},_getMenuItemForItem:function(_73,_74){
var _75=this.store;
if(!_73||!_75||!_75.isItem(_73)){
var i=new _1.MenuItem({label:"---",disabled:true,iconClass:"dojoxEmpty",focus:function(){
}});
this._updateClass(i.domNode,"Item");
return i;
}else{
var _76=_75.isItemLoaded(_73);
var _77=_76?this.getChildItems(_73):undefined;
var _78;
if(_77){
_78=this.getMenuItemForItem(_73,_74,_77);
_78.children=_77;
this._updateClass(_78.domNode,"Item",{"Expanding":true});
if(!_78._started){
var c=_78.connect(_78,"startup",function(){
this.disconnect(c);
_2.style(this.arrowWrapper,"display","");
});
}else{
_2.style(_78.arrowWrapper,"display","");
}
}else{
_78=this.getMenuItemForItem(_73,_74,null);
if(_76){
this._updateClass(_78.domNode,"Item",{"Single":true});
}else{
this._updateClass(_78.domNode,"Item",{"Unloaded":true});
_78.attr("disabled",true);
}
}
_78.store=this.store;
_78.item=_73;
if(!_78.label){
_78.attr("label",this.store.getLabel(_73).replace(/</,"&lt;"));
}
if(_78.focusNode){
var _79=this;
_78.focus=function(){
if(!this.disabled){
try{
this.focusNode.focus();
}
catch(e){
}
}
};
_78.connect(_78.focusNode,"onmouseenter",function(){
if(!this.disabled){
_79._updateClass(this.domNode,"Item",{"Hover":true});
}
});
_78.connect(_78.focusNode,"onmouseleave",function(){
if(!this.disabled){
_79._updateClass(this.domNode,"Item",{"Hover":false});
}
});
_78.connect(_78.focusNode,"blur",function(){
_79._updateClass(this.domNode,"Item",{"Focus":false,"Hover":false});
});
_78.connect(_78.focusNode,"focus",function(){
_79._updateClass(this.domNode,"Item",{"Focus":true});
_79._focusedPane=_74;
});
if(this.executeOnDblClick){
_78.connect(_78.focusNode,"ondblclick",function(){
_79._onExecute();
});
}
}
return _78;
}
},_setStore:function(_7a){
if(_7a===this.store&&this._started){
return;
}
this.store=_7a;
this._isIdentity=_7a.getFeatures()["dojo.data.api.Identity"];
var _7b=this._getPaneForItem();
this.addChild(_7b,0);
},_onKey:function(e){
if(e.charOrCode==_2.keys.BACKSPACE){
_2.stopEvent(e);
return;
}else{
if(e.charOrCode==_2.keys.ESCAPE&&this._savedFocus){
try{
_1.focus(this._savedFocus);
}
catch(e){
}
_2.stopEvent(e);
return;
}else{
if(e.charOrCode==_2.keys.LEFT_ARROW||e.charOrCode==_2.keys.RIGHT_ARROW){
_2.stopEvent(e);
return;
}
}
}
},_resetValue:function(){
this.set("value",this._lastExecutedValue);
},_onCancel:function(){
this._resetValue();
this.onCancel();
},_onExecute:function(){
this._lastExecutedValue=this.get("value");
this.onExecute();
},focus:function(){
var _7c=this._savedFocus;
this._savedFocus=_1.getFocus(this);
if(!this._savedFocus.node){
delete this._savedFocus;
}
if(!this._focusedPane){
var _7d=this.getChildren()[0];
if(_7d&&!_7c){
_7d.focus(true);
}
}else{
this._savedFocus=_1.getFocus(this);
var foc=this._focusedPane;
delete this._focusedPane;
if(!_7c){
foc.focus(true);
}
}
},handleKey:function(e){
if(e.charOrCode==_2.keys.DOWN_ARROW){
delete this._savedFocus;
this.focus();
return false;
}else{
if(e.charOrCode==_2.keys.ESCAPE){
this._onCancel();
return false;
}
}
return true;
},_updateChildClasses:function(){
var _7e=this.getChildren();
var _7f=_7e.length;
_2.forEach(_7e,function(c,idx){
_2.toggleClass(c.domNode,"dojoxRollingListPaneCurrentChild",(idx==(_7f-1)));
_2.toggleClass(c.domNode,"dojoxRollingListPaneCurrentSelected",(idx==(_7f-2)));
});
},startup:function(){
if(this._started){
return;
}
if(!this.getParent||!this.getParent()){
this.resize();
this.connect(_2.global,"onresize","resize");
}
this.connect(this,"addChild","_updateChildClasses");
this.connect(this,"removeChild","_updateChildClasses");
this._setStore(this.store);
this.set("showButtons",this.showButtons);
this.inherited(arguments);
this._lastExecutedValue=this.get("value");
},getChildItems:function(_80){
var _81,_82=this.store;
_2.forEach(this.childrenAttrs,function(_83){
var _84=_82.getValues(_80,_83);
if(_84&&_84.length){
_81=(_81||[]).concat(_84);
}
});
return _81;
},getMenuItemForItem:function(_85,_86,_87){
return new _1.MenuItem({});
},getPaneForItem:function(_88,_89,_8a){
if(!_88||_8a){
return new _3.widget._RollingListGroupPane({});
}else{
return null;
}
},onItemClick:function(_8b,_8c,_8d){
},onExecute:function(){
},onCancel:function(){
},onChange:function(_8e){
}});
});
