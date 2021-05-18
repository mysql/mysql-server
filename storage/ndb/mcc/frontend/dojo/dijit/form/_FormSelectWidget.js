//>>built
define("dijit/form/_FormSelectWidget",["dojo/_base/array","dojo/_base/Deferred","dojo/aspect","dojo/data/util/sorter","dojo/_base/declare","dojo/dom","dojo/dom-class","dojo/_base/kernel","dojo/_base/lang","dojo/query","dojo/when","dojo/store/util/QueryResults","./_FormValueWidget"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
var _e=_5("dijit.form._FormSelectWidget",_d,{multiple:false,options:null,store:null,_setStoreAttr:function(_f){
if(this._created){
this._deprecatedSetStore(_f);
}
},query:null,_setQueryAttr:function(_10){
if(this._created){
this._deprecatedSetStore(this.store,this.selectedValue,{query:_10});
}
},queryOptions:null,_setQueryOptionsAttr:function(_11){
if(this._created){
this._deprecatedSetStore(this.store,this.selectedValue,{queryOptions:_11});
}
},labelAttr:"",onFetch:null,sortByLabel:true,loadChildrenOnOpen:false,onLoadDeferred:null,getOptions:function(_12){
var _13=this.options||[];
if(_12==null){
return _13;
}
if(_9.isArrayLike(_12)){
return _1.map(_12,"return this.getOptions(item);",this);
}
if(_9.isString(_12)){
_12={value:_12};
}
if(_9.isObject(_12)){
if(!_1.some(_13,function(_14,idx){
for(var a in _12){
if(!(a in _14)||_14[a]!=_12[a]){
return false;
}
}
_12=idx;
return true;
})){
_12=-1;
}
}
if(_12>=0&&_12<_13.length){
return _13[_12];
}
return null;
},addOption:function(_15){
_1.forEach(_9.isArrayLike(_15)?_15:[_15],function(i){
if(i&&_9.isObject(i)){
this.options.push(i);
}
},this);
this._loadChildren();
},removeOption:function(_16){
var _17=this.getOptions(_9.isArrayLike(_16)?_16:[_16]);
_1.forEach(_17,function(_18){
if(_18){
this.options=_1.filter(this.options,function(_19){
return (_19.value!==_18.value||_19.label!==_18.label);
});
this._removeOptionItem(_18);
}
},this);
this._loadChildren();
},updateOption:function(_1a){
_1.forEach(_9.isArrayLike(_1a)?_1a:[_1a],function(i){
var _1b=this.getOptions({value:i.value}),k;
if(_1b){
for(k in i){
_1b[k]=i[k];
}
}
},this);
this._loadChildren();
},setStore:function(_1c,_1d,_1e){
_8.deprecated(this.declaredClass+"::setStore(store, selectedValue, fetchArgs) is deprecated. Use set('query', fetchArgs.query), set('queryOptions', fetchArgs.queryOptions), set('store', store), or set('value', selectedValue) instead.","","2.0");
this._deprecatedSetStore(_1c,_1d,_1e);
},_deprecatedSetStore:function(_1f,_20,_21){
var _22=this.store;
_21=_21||{};
if(_22!==_1f){
var h;
while((h=this._notifyConnections.pop())){
h.remove();
}
if(!_1f.get){
_9.mixin(_1f,{_oldAPI:true,get:function(id){
var _23=new _2();
this.fetchItemByIdentity({identity:id,onItem:function(_24){
_23.resolve(_24);
},onError:function(_25){
_23.reject(_25);
}});
return _23.promise;
},query:function(_26,_27){
var _28=new _2(function(){
if(_29.abort){
_29.abort();
}
});
_28.total=new _2();
var _29=this.fetch(_9.mixin({query:_26,onBegin:function(_2a){
_28.total.resolve(_2a);
},onComplete:function(_2b){
_28.resolve(_2b);
},onError:function(_2c){
_28.reject(_2c);
}},_27));
return new _c(_28);
}});
if(_1f.getFeatures()["dojo.data.api.Notification"]){
this._notifyConnections=[_3.after(_1f,"onNew",_9.hitch(this,"_onNewItem"),true),_3.after(_1f,"onDelete",_9.hitch(this,"_onDeleteItem"),true),_3.after(_1f,"onSet",_9.hitch(this,"_onSetItem"),true)];
}
}
this._set("store",_1f);
}
if(this.options&&this.options.length){
this.removeOption(this.options);
}
if(this._queryRes&&this._queryRes.close){
this._queryRes.close();
}
if(this._observeHandle&&this._observeHandle.remove){
this._observeHandle.remove();
this._observeHandle=null;
}
if(_21.query){
this._set("query",_21.query);
}
if(_21.queryOptions){
this._set("queryOptions",_21.queryOptions);
}
if(_1f&&_1f.query){
this._loadingStore=true;
this.onLoadDeferred=new _2();
this._queryRes=_1f.query(this.query,this.queryOptions);
_b(this._queryRes,_9.hitch(this,function(_2d){
if(this.sortByLabel&&!_21.sort&&_2d.length){
if(_1f.getValue){
_2d.sort(_4.createSortFunction([{attribute:_1f.getLabelAttributes(_2d[0])[0]}],_1f));
}else{
var _2e=this.labelAttr;
_2d.sort(function(a,b){
return a[_2e]>b[_2e]?1:b[_2e]>a[_2e]?-1:0;
});
}
}
if(_21.onFetch){
_2d=_21.onFetch.call(this,_2d,_21);
}
_1.forEach(_2d,function(i){
this._addOptionForItem(i);
},this);
if(this._queryRes.observe){
this._observeHandle=this._queryRes.observe(_9.hitch(this,function(_2f,_30,_31){
if(_30==_31){
this._onSetItem(_2f);
}else{
if(_30!=-1){
this._onDeleteItem(_2f);
}
if(_31!=-1){
this._onNewItem(_2f);
}
}
}),true);
}
this._loadingStore=false;
this.set("value","_pendingValue" in this?this._pendingValue:_20);
delete this._pendingValue;
if(!this.loadChildrenOnOpen){
this._loadChildren();
}else{
this._pseudoLoadChildren(_2d);
}
this.onLoadDeferred.resolve(true);
this.onSetStore();
}),_9.hitch(this,function(err){
console.error("dijit.form.Select: "+err.toString());
this.onLoadDeferred.reject(err);
}));
}
return _22;
},_setValueAttr:function(_32,_33){
if(!this._onChangeActive){
_33=null;
}
if(this._loadingStore){
this._pendingValue=_32;
return;
}
if(_32==null){
return;
}
if(_9.isArrayLike(_32)){
_32=_1.map(_32,function(_34){
return _9.isObject(_34)?_34:{value:_34};
});
}else{
if(_9.isObject(_32)){
_32=[_32];
}else{
_32=[{value:_32}];
}
}
_32=_1.filter(this.getOptions(_32),function(i){
return i&&i.value;
});
var _35=this.getOptions()||[];
if(!this.multiple&&(!_32[0]||!_32[0].value)&&!!_35.length){
_32[0]=_35[0];
}
_1.forEach(_35,function(opt){
opt.selected=_1.some(_32,function(v){
return v.value===opt.value;
});
});
var val=_1.map(_32,function(opt){
return opt.value;
});
if(typeof val=="undefined"||typeof val[0]=="undefined"){
return;
}
var _36=_1.map(_32,function(opt){
return opt.label;
});
this._setDisplay(this.multiple?_36:_36[0]);
this.inherited(arguments,[this.multiple?val:val[0],_33]);
this._updateSelection();
},_getDisplayedValueAttr:function(){
var ret=_1.map([].concat(this.get("selectedOptions")),function(v){
if(v&&"label" in v){
return v.label;
}else{
if(v){
return v.value;
}
}
return null;
},this);
return this.multiple?ret:ret[0];
},_setDisplayedValueAttr:function(_37){
this.set("value",this.getOptions(typeof _37=="string"?{label:_37}:_37));
},_loadChildren:function(){
if(this._loadingStore){
return;
}
_1.forEach(this._getChildren(),function(_38){
_38.destroyRecursive();
});
_1.forEach(this.options,this._addOptionItem,this);
this._updateSelection();
},_updateSelection:function(){
this.focusedChild=null;
this._set("value",this._getValueFromOpts());
var val=[].concat(this.value);
if(val&&val[0]){
var _39=this;
_1.forEach(this._getChildren(),function(_3a){
var _3b=_1.some(val,function(v){
return _3a.option&&(v===_3a.option.value);
});
if(_3b&&!_39.multiple){
_39.focusedChild=_3a;
}
_7.toggle(_3a.domNode,this.baseClass.replace(/\s+|$/g,"SelectedOption "),_3b);
_3a.domNode.setAttribute("aria-selected",_3b?"true":"false");
},this);
}
},_getValueFromOpts:function(){
var _3c=this.getOptions()||[];
if(!this.multiple&&_3c.length){
var opt=_1.filter(_3c,function(i){
return i.selected;
})[0];
if(opt&&opt.value){
return opt.value;
}else{
_3c[0].selected=true;
return _3c[0].value;
}
}else{
if(this.multiple){
return _1.map(_1.filter(_3c,function(i){
return i.selected;
}),function(i){
return i.value;
})||[];
}
}
return "";
},_onNewItem:function(_3d,_3e){
if(!_3e||!_3e.parent){
this._addOptionForItem(_3d);
}
},_onDeleteItem:function(_3f){
var _40=this.store;
this.removeOption({value:_40.getIdentity(_3f)});
},_onSetItem:function(_41){
this.updateOption(this._getOptionObjForItem(_41));
},_getOptionObjForItem:function(_42){
var _43=this.store,_44=(this.labelAttr&&this.labelAttr in _42)?_42[this.labelAttr]:_43.getLabel(_42),_45=(_44?_43.getIdentity(_42):null);
return {value:_45,label:_44,item:_42};
},_addOptionForItem:function(_46){
var _47=this.store;
if(_47.isItemLoaded&&!_47.isItemLoaded(_46)){
_47.loadItem({item:_46,onItem:function(i){
this._addOptionForItem(i);
},scope:this});
return;
}
var _48=this._getOptionObjForItem(_46);
this.addOption(_48);
},constructor:function(_49){
this._oValue=(_49||{}).value||null;
this._notifyConnections=[];
},buildRendering:function(){
this.inherited(arguments);
_6.setSelectable(this.focusNode,false);
},_fillContent:function(){
if(!this.options){
this.options=this.srcNodeRef?_a("> *",this.srcNodeRef).map(function(_4a){
if(_4a.getAttribute("type")==="separator"){
return {value:"",label:"",selected:false,disabled:false};
}
return {value:(_4a.getAttribute("data-"+_8._scopeName+"-value")||_4a.getAttribute("value")),label:String(_4a.innerHTML),selected:_4a.getAttribute("selected")||false,disabled:_4a.getAttribute("disabled")||false};
},this):[];
}
if(!this.value){
this._set("value",this._getValueFromOpts());
}else{
if(this.multiple&&typeof this.value=="string"){
this._set("value",this.value.split(","));
}
}
},postCreate:function(){
this.inherited(arguments);
_3.after(this,"onChange",_9.hitch(this,"_updateSelection"));
var _4b=this.store;
if(_4b&&(_4b.getIdentity||_4b.getFeatures()["dojo.data.api.Identity"])){
this.store=null;
this._deprecatedSetStore(_4b,this._oValue,{query:this.query,queryOptions:this.queryOptions});
}
this._storeInitialized=true;
},startup:function(){
this._loadChildren();
this.inherited(arguments);
},destroy:function(){
var h;
while((h=this._notifyConnections.pop())){
h.remove();
}
if(this._queryRes&&this._queryRes.close){
this._queryRes.close();
}
if(this._observeHandle&&this._observeHandle.remove){
this._observeHandle.remove();
this._observeHandle=null;
}
this.inherited(arguments);
},_addOptionItem:function(){
},_removeOptionItem:function(){
},_setDisplay:function(){
},_getChildren:function(){
return [];
},_getSelectedOptionsAttr:function(){
return this.getOptions({selected:true});
},_pseudoLoadChildren:function(){
},onSetStore:function(){
}});
return _e;
});
