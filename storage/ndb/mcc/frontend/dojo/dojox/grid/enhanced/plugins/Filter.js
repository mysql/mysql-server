//>>built
define("dojox/grid/enhanced/plugins/Filter",["dojo/_base/declare","dojo/_base/lang","../_Plugin","./Dialog","./filter/FilterLayer","./filter/FilterBar","./filter/FilterDefDialog","./filter/FilterStatusTip","./filter/ClearFilterConfirm","../../EnhancedGrid","dojo/i18n!../nls/Filter"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
var _c=_1("dojox.grid.enhanced.plugins.Filter",_3,{name:"filter",constructor:function(_d,_e){
this.grid=_d;
this.nls=_b;
_e=this.args=_2.isObject(_e)?_e:{};
if(typeof _e.ruleCount!="number"||_e.ruleCount<0){
_e.ruleCount=3;
}
var rc=this.ruleCountToConfirmClearFilter=_e.ruleCountToConfirmClearFilter;
if(rc===undefined){
this.ruleCountToConfirmClearFilter=2;
}
this._wrapStore();
var _f={"plugin":this};
this.clearFilterDialog=new _4({refNode:this.grid.domNode,title:this.nls["clearFilterDialogTitle"],content:new _9(_f)});
this.filterDefDialog=new _7(_f);
this.filterBar=new _6(_f);
this.filterStatusTip=new _8(_f);
_d.onFilterDefined=function(){
};
this.connect(_d.layer("filter"),"onFilterDefined",function(_10){
_d.onFilterDefined(_d.getFilter(),_d.getFilterRelation());
});
},destroy:function(){
this.inherited(arguments);
try{
this.grid.unwrap("filter");
this.filterBar.destroyRecursive();
this.filterBar=null;
this.clearFilterDialog.destroyRecursive();
this.clearFilterDialog=null;
this.filterStatusTip.destroy();
this.filterStatusTip=null;
this.filterDefDialog.destroy();
this.filterDefDialog=null;
this.grid=null;
this.args=null;
}
catch(e){
console.warn("Filter.destroy() error:",e);
}
},_wrapStore:function(){
var g=this.grid;
var _11=this.args;
var _12=_11.isServerSide?new _5.ServerSideFilterLayer(_11):new _5.ClientSideFilterLayer({cacheSize:_11.filterCacheSize,fetchAll:_11.fetchAllOnFirstFilter,getter:this._clientFilterGetter});
_5.wrap(g,"_storeLayerFetch",_12);
this.connect(g,"_onDelete",_2.hitch(_12,"invalidate"));
},onSetStore:function(_13){
this.filterDefDialog.clearFilter(true);
},_clientFilterGetter:function(_14,_15,_16){
return _15.get(_16,_14);
}});
_a.registerPlugin(_c);
return _c;
});
