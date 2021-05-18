/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/data/ObjectStore",["../_base/lang","../Evented","../_base/declare","../_base/Deferred","../promise/all","../_base/array","../_base/connect","../regexp"],function(_1,_2,_3,_4,_5,_6,_7,_8){
function _9(_a){
return _a=="*"?".*":_a=="?"?".":_a;
};
return _3("dojo.data.ObjectStore",[_2],{objectStore:null,constructor:function(_b){
this._dirtyObjects=[];
if(_b.labelAttribute){
_b.labelProperty=_b.labelAttribute;
}
_1.mixin(this,_b);
},labelProperty:"label",getValue:function(_c,_d,_e){
return typeof _c.get==="function"?_c.get(_d):_d in _c?_c[_d]:_e;
},getValues:function(_f,_10){
var val=this.getValue(_f,_10);
return val instanceof Array?val:val===undefined?[]:[val];
},getAttributes:function(_11){
var res=[];
for(var i in _11){
if(_11.hasOwnProperty(i)&&!(i.charAt(0)=="_"&&i.charAt(1)=="_")){
res.push(i);
}
}
return res;
},hasAttribute:function(_12,_13){
return _13 in _12;
},containsValue:function(_14,_15,_16){
return _6.indexOf(this.getValues(_14,_15),_16)>-1;
},isItem:function(_17){
return (typeof _17=="object")&&_17&&!(_17 instanceof Date);
},isItemLoaded:function(_18){
return _18&&typeof _18.load!=="function";
},loadItem:function(_19){
var _1a;
if(typeof _19.item.load==="function"){
_4.when(_19.item.load(),function(_1b){
_1a=_1b;
var _1c=_1b instanceof Error?_19.onError:_19.onItem;
if(_1c){
_1c.call(_19.scope,_1b);
}
});
}else{
if(_19.onItem){
_19.onItem.call(_19.scope,_19.item);
}
}
return _1a;
},close:function(_1d){
return _1d&&_1d.abort&&_1d.abort();
},fetch:function(_1e){
_1e=_1.delegate(_1e,_1e&&_1e.queryOptions);
var _1f=this;
var _20=_1e.scope||_1f;
var _21=_1e.query;
if(typeof _21=="object"){
_21=_1.delegate(_21);
for(var i in _21){
var _22=_21[i];
if(typeof _22=="string"){
_21[i]=RegExp("^"+_8.escapeString(_22,"*?\\").replace(/\\.|\*|\?/g,_9)+"$",_1e.ignoreCase?"mi":"m");
_21[i].toString=(function(_23){
return function(){
return _23;
};
})(_22);
}
}
}
var _24=this.objectStore.query(_21,_1e);
_4.when(_24.total,function(_25){
_4.when(_24,function(_26){
if(_1e.onBegin){
_1e.onBegin.call(_20,_25||_26.length,_1e);
}
if(_1e.onItem){
for(var i=0;i<_26.length;i++){
_1e.onItem.call(_20,_26[i],_1e);
}
}
if(_1e.onComplete){
_1e.onComplete.call(_20,_1e.onItem?null:_26,_1e);
}
return _26;
},_27);
},_27);
function _27(_28){
if(_1e.onError){
_1e.onError.call(_20,_28,_1e);
}
};
_1e.abort=function(){
if(_24.cancel){
_24.cancel();
}
};
if(_24.observe){
if(this.observing){
this.observing.cancel();
}
this.observing=_24.observe(function(_29,_2a,_2b){
if(_6.indexOf(_1f._dirtyObjects,_29)==-1){
if(_2a==-1){
_1f.onNew(_29);
}else{
if(_2b==-1){
_1f.onDelete(_29);
}else{
for(var i in _29){
if(i!=_1f.objectStore.idProperty){
_1f.onSet(_29,i,null,_29[i]);
}
}
}
}
}
},true);
}
this.onFetch(_24);
_1e.store=this;
return _1e;
},getFeatures:function(){
return {"dojo.data.api.Read":!!this.objectStore.get,"dojo.data.api.Identity":true,"dojo.data.api.Write":!!this.objectStore.put,"dojo.data.api.Notification":true};
},getLabel:function(_2c){
if(this.isItem(_2c)){
return this.getValue(_2c,this.labelProperty);
}
return undefined;
},getLabelAttributes:function(_2d){
return [this.labelProperty];
},getIdentity:function(_2e){
return this.objectStore.getIdentity?this.objectStore.getIdentity(_2e):_2e[this.objectStore.idProperty||"id"];
},getIdentityAttributes:function(_2f){
return [this.objectStore.idProperty];
},fetchItemByIdentity:function(_30){
var _31;
_4.when(this.objectStore.get(_30.identity),function(_32){
_31=_32;
_30.onItem.call(_30.scope,_32);
},function(_33){
_30.onError.call(_30.scope,_33);
});
return _31;
},newItem:function(_34,_35){
if(_35){
var _36=this.getValue(_35.parent,_35.attribute,[]);
_36=_36.concat([_34]);
_34.__parent=_36;
this.setValue(_35.parent,_35.attribute,_36);
}
this._dirtyObjects.push({object:_34,save:true});
this.onNew(_34);
return _34;
},deleteItem:function(_37){
this.changing(_37,true);
this.onDelete(_37);
},setValue:function(_38,_39,_3a){
var old=_38[_39];
this.changing(_38);
_38[_39]=_3a;
this.onSet(_38,_39,old,_3a);
},setValues:function(_3b,_3c,_3d){
if(!_1.isArray(_3d)){
throw new Error("setValues expects to be passed an Array object as its value");
}
this.setValue(_3b,_3c,_3d);
},unsetAttribute:function(_3e,_3f){
this.changing(_3e);
var old=_3e[_3f];
delete _3e[_3f];
this.onSet(_3e,_3f,old,undefined);
},changing:function(_40,_41){
_40.__isDirty=true;
for(var i=0;i<this._dirtyObjects.length;i++){
var _42=this._dirtyObjects[i];
if(_40==_42.object){
if(_41){
_42.object=false;
if(!this._saveNotNeeded){
_42.save=true;
}
}
return;
}
}
var old=_40 instanceof Array?[]:{};
for(i in _40){
if(_40.hasOwnProperty(i)){
old[i]=_40[i];
}
}
this._dirtyObjects.push({object:!_41&&_40,old:old,save:!this._saveNotNeeded});
},save:function(_43){
_43=_43||{};
var _44,_45=[];
var _46=[];
var _47=this;
var _48=this._dirtyObjects;
var _49=_48.length;
try{
_7.connect(_43,"onError",function(){
if(_43.revertOnError!==false){
var _4a=_48;
_48=_46;
_47.revert();
_47._dirtyObjects=_4a;
}else{
_47._dirtyObjects=_48.concat(_46);
}
});
var _4b;
if(this.objectStore.transaction){
_4b=this.objectStore.transaction();
}
for(var i=0;i<_48.length;i++){
var _4c=_48[i];
var _4d=_4c.object;
var old=_4c.old;
delete _4d.__isDirty;
if(_4d){
_44=this.objectStore.put(_4d,{overwrite:!!old});
_45.push(_44);
}else{
if(typeof old!="undefined"){
_44=this.objectStore.remove(this.getIdentity(old));
_45.push(_44);
}
}
_46.push(_4c);
_48.splice(i--,1);
}
_5(_45).then(function(_4e){
if(_43.onComplete){
_43.onComplete.call(_43.scope,_4e);
}
},function(_4f){
if(_43.onError){
_43.onError.call(_43.scope,_4f);
}
});
if(_4b){
_4b.commit();
}
}
catch(e){
_43.onError.call(_43.scope,value);
}
},revert:function(){
var _50=this._dirtyObjects;
for(var i=_50.length;i>0;){
i--;
var _51=_50[i];
var _52=_51.object;
var old=_51.old;
if(_52&&old){
for(var j in old){
if(old.hasOwnProperty(j)&&_52[j]!==old[j]){
this.onSet(_52,j,_52[j],old[j]);
_52[j]=old[j];
}
}
for(j in _52){
if(!old.hasOwnProperty(j)){
this.onSet(_52,j,_52[j]);
delete _52[j];
}
}
}else{
if(!old){
this.onDelete(_52);
}else{
this.onNew(old);
}
}
delete (_52||old).__isDirty;
_50.splice(i,1);
}
},isDirty:function(_53){
if(!_53){
return !!this._dirtyObjects.length;
}
return _53.__isDirty;
},onSet:function(){
},onNew:function(){
},onDelete:function(){
},onFetch:function(_54){
}});
});
