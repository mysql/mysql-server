//>>built
define("dijit/form/_FormSelectWidget",["dojo/_base/array","dojo/aspect","dojo/data/util/sorter","dojo/_base/declare","dojo/dom","dojo/dom-class","dojo/_base/kernel","dojo/_base/lang","dojo/query","./_FormValueWidget"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _4("dijit.form._FormSelectWidget",_a,{multiple:false,options:null,store:null,query:null,queryOptions:null,onFetch:null,sortByLabel:true,loadChildrenOnOpen:false,getOptions:function(_b){
var _c=_b,_d=this.options||[],l=_d.length;
if(_c===undefined){
return _d;
}
if(_8.isArray(_c)){
return _1.map(_c,"return this.getOptions(item);",this);
}
if(_8.isObject(_b)){
if(!_1.some(this.options,function(o,_e){
if(o===_c||(o.value&&o.value===_c.value)){
_c=_e;
return true;
}
return false;
})){
_c=-1;
}
}
if(typeof _c=="string"){
for(var i=0;i<l;i++){
if(_d[i].value===_c){
_c=i;
break;
}
}
}
if(typeof _c=="number"&&_c>=0&&_c<l){
return this.options[_c];
}
return null;
},addOption:function(_f){
if(!_8.isArray(_f)){
_f=[_f];
}
_1.forEach(_f,function(i){
if(i&&_8.isObject(i)){
this.options.push(i);
}
},this);
this._loadChildren();
},removeOption:function(_10){
if(!_8.isArray(_10)){
_10=[_10];
}
var _11=this.getOptions(_10);
_1.forEach(_11,function(i){
if(i){
this.options=_1.filter(this.options,function(_12){
return (_12.value!==i.value||_12.label!==i.label);
});
this._removeOptionItem(i);
}
},this);
this._loadChildren();
},updateOption:function(_13){
if(!_8.isArray(_13)){
_13=[_13];
}
_1.forEach(_13,function(i){
var _14=this.getOptions(i),k;
if(_14){
for(k in i){
_14[k]=i[k];
}
}
},this);
this._loadChildren();
},setStore:function(_15,_16,_17){
var _18=this.store;
_17=_17||{};
if(_18!==_15){
var h;
while(h=this._notifyConnections.pop()){
h.remove();
}
if(_15&&_15.getFeatures()["dojo.data.api.Notification"]){
this._notifyConnections=[_2.after(_15,"onNew",_8.hitch(this,"_onNewItem"),true),_2.after(_15,"onDelete",_8.hitch(this,"_onDeleteItem"),true),_2.after(_15,"onSet",_8.hitch(this,"_onSetItem"),true)];
}
this._set("store",_15);
}
this._onChangeActive=false;
if(this.options&&this.options.length){
this.removeOption(this.options);
}
if(_15){
this._loadingStore=true;
_15.fetch(_8.delegate(_17,{onComplete:function(_19,_1a){
if(this.sortByLabel&&!_17.sort&&_19.length){
_19.sort(_3.createSortFunction([{attribute:_15.getLabelAttributes(_19[0])[0]}],_15));
}
if(_17.onFetch){
_19=_17.onFetch.call(this,_19,_1a);
}
_1.forEach(_19,function(i){
this._addOptionForItem(i);
},this);
this._loadingStore=false;
this.set("value","_pendingValue" in this?this._pendingValue:_16);
delete this._pendingValue;
if(!this.loadChildrenOnOpen){
this._loadChildren();
}else{
this._pseudoLoadChildren(_19);
}
this._fetchedWith=_1a;
this._lastValueReported=this.multiple?[]:null;
this._onChangeActive=true;
this.onSetStore();
this._handleOnChange(this.value);
},scope:this}));
}else{
delete this._fetchedWith;
}
return _18;
},_setValueAttr:function(_1b,_1c){
if(this._loadingStore){
this._pendingValue=_1b;
return;
}
var _1d=this.getOptions()||[];
if(!_8.isArray(_1b)){
_1b=[_1b];
}
_1.forEach(_1b,function(i,idx){
if(!_8.isObject(i)){
i=i+"";
}
if(typeof i==="string"){
_1b[idx]=_1.filter(_1d,function(_1e){
return _1e.value===i;
})[0]||{value:"",label:""};
}
},this);
_1b=_1.filter(_1b,function(i){
return i&&i.value;
});
if(!this.multiple&&(!_1b[0]||!_1b[0].value)&&_1d.length){
_1b[0]=_1d[0];
}
_1.forEach(_1d,function(i){
i.selected=_1.some(_1b,function(v){
return v.value===i.value;
});
});
var val=_1.map(_1b,function(i){
return i.value;
}),_1f=_1.map(_1b,function(i){
return i.label;
});
this._set("value",this.multiple?val:val[0]);
this._setDisplay(this.multiple?_1f:_1f[0]);
this._updateSelection();
this._handleOnChange(this.value,_1c);
},_getDisplayedValueAttr:function(){
var val=this.get("value");
if(!_8.isArray(val)){
val=[val];
}
var ret=_1.map(this.getOptions(val),function(v){
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
},_loadChildren:function(){
if(this._loadingStore){
return;
}
_1.forEach(this._getChildren(),function(_20){
_20.destroyRecursive();
});
_1.forEach(this.options,this._addOptionItem,this);
this._updateSelection();
},_updateSelection:function(){
this._set("value",this._getValueFromOpts());
var val=this.value;
if(!_8.isArray(val)){
val=[val];
}
if(val&&val[0]){
_1.forEach(this._getChildren(),function(_21){
var _22=_1.some(val,function(v){
return _21.option&&(v===_21.option.value);
});
_6.toggle(_21.domNode,this.baseClass+"SelectedOption",_22);
_21.domNode.setAttribute("aria-selected",_22);
},this);
}
},_getValueFromOpts:function(){
var _23=this.getOptions()||[];
if(!this.multiple&&_23.length){
var opt=_1.filter(_23,function(i){
return i.selected;
})[0];
if(opt&&opt.value){
return opt.value;
}else{
_23[0].selected=true;
return _23[0].value;
}
}else{
if(this.multiple){
return _1.map(_1.filter(_23,function(i){
return i.selected;
}),function(i){
return i.value;
})||[];
}
}
return "";
},_onNewItem:function(_24,_25){
if(!_25||!_25.parent){
this._addOptionForItem(_24);
}
},_onDeleteItem:function(_26){
var _27=this.store;
this.removeOption(_27.getIdentity(_26));
},_onSetItem:function(_28){
this.updateOption(this._getOptionObjForItem(_28));
},_getOptionObjForItem:function(_29){
var _2a=this.store,_2b=_2a.getLabel(_29),_2c=(_2b?_2a.getIdentity(_29):null);
return {value:_2c,label:_2b,item:_29};
},_addOptionForItem:function(_2d){
var _2e=this.store;
if(!_2e.isItemLoaded(_2d)){
_2e.loadItem({item:_2d,onItem:function(i){
this._addOptionForItem(i);
},scope:this});
return;
}
var _2f=this._getOptionObjForItem(_2d);
this.addOption(_2f);
},constructor:function(_30){
this._oValue=(_30||{}).value||null;
this._notifyConnections=[];
},buildRendering:function(){
this.inherited(arguments);
_5.setSelectable(this.focusNode,false);
},_fillContent:function(){
var _31=this.options;
if(!_31){
_31=this.options=this.srcNodeRef?_9("> *",this.srcNodeRef).map(function(_32){
if(_32.getAttribute("type")==="separator"){
return {value:"",label:"",selected:false,disabled:false};
}
return {value:(_32.getAttribute("data-"+_7._scopeName+"-value")||_32.getAttribute("value")),label:String(_32.innerHTML),selected:_32.getAttribute("selected")||false,disabled:_32.getAttribute("disabled")||false};
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
this.connect(this,"onChange","_updateSelection");
this.connect(this,"startup","_loadChildren");
this._setValueAttr(this.value,null);
},startup:function(){
this.inherited(arguments);
var _33=this.store,_34={};
_1.forEach(["query","queryOptions","onFetch"],function(i){
if(this[i]){
_34[i]=this[i];
}
delete this[i];
},this);
if(_33&&_33.getFeatures()["dojo.data.api.Identity"]){
this.store=null;
this.setStore(_33,this._oValue,_34);
}
},destroy:function(){
var h;
while(h=this._notifyConnections.pop()){
h.remove();
}
this.inherited(arguments);
},_addOptionItem:function(){
},_removeOptionItem:function(){
},_setDisplay:function(){
},_getChildren:function(){
return [];
},_getSelectedOptionsAttr:function(){
return this.getOptions(this.get("value"));
},_pseudoLoadChildren:function(){
},onSetStore:function(){
}});
});
