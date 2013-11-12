/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/data/ObjectStore",["../_base/lang","../Evented","../_base/declare","../_base/Deferred","../_base/array","../_base/connect","../regexp"],function(_1,_2,_3,_4,_5,_6,_7){
return _3("dojo.data.ObjectStore",[_2],{objectStore:null,constructor:function(_8){
_1.mixin(this,_8);
},labelProperty:"label",getValue:function(_9,_a,_b){
return typeof _9.get==="function"?_9.get(_a):_a in _9?_9[_a]:_b;
},getValues:function(_c,_d){
var _e=this.getValue(_c,_d);
return _e instanceof Array?_e:_e===undefined?[]:[_e];
},getAttributes:function(_f){
var res=[];
for(var i in _f){
if(_f.hasOwnProperty(i)&&!(i.charAt(0)=="_"&&i.charAt(1)=="_")){
res.push(i);
}
}
return res;
},hasAttribute:function(_10,_11){
return _11 in _10;
},containsValue:function(_12,_13,_14){
return _5.indexOf(this.getValues(_12,_13),_14)>-1;
},isItem:function(_15){
return (typeof _15=="object")&&_15&&!(_15 instanceof Date);
},isItemLoaded:function(_16){
return _16&&typeof _16.load!=="function";
},loadItem:function(_17){
var _18;
if(typeof _17.item.load==="function"){
_4.when(_17.item.load(),function(_19){
_18=_19;
var _1a=_19 instanceof Error?_17.onError:_17.onItem;
if(_1a){
_1a.call(_17.scope,_19);
}
});
}else{
if(_17.onItem){
_17.onItem.call(_17.scope,_17.item);
}
}
return _18;
},close:function(_1b){
return _1b&&_1b.abort&&_1b.abort();
},fetch:function(_1c){
_1c=_1.delegate(_1c,_1c&&_1c.queryOptions);
var _1d=this;
var _1e=_1c.scope||_1d;
var _1f=_1c.query;
if(typeof _1f=="object"){
_1f=_1.delegate(_1f);
for(var i in _1f){
var _20=_1f[i];
if(typeof _20=="string"){
_1f[i]=RegExp("^"+_7.escapeString(_20,"*?").replace(/\*/g,".*").replace(/\?/g,".")+"$",_1c.ignoreCase?"mi":"m");
_1f[i].toString=(function(_21){
return function(){
return _21;
};
})(_20);
}
}
}
var _22=this.objectStore.query(_1f,_1c);
_4.when(_22.total,function(_23){
_4.when(_22,function(_24){
if(_1c.onBegin){
_1c.onBegin.call(_1e,_23||_24.length,_1c);
}
if(_1c.onItem){
for(var i=0;i<_24.length;i++){
_1c.onItem.call(_1e,_24[i],_1c);
}
}
if(_1c.onComplete){
_1c.onComplete.call(_1e,_1c.onItem?null:_24,_1c);
}
return _24;
},_25);
},_25);
function _25(_26){
if(_1c.onError){
_1c.onError.call(_1e,_26,_1c);
}
};
_1c.abort=function(){
if(_22.cancel){
_22.cancel();
}
};
if(_22.observe){
if(this.observing){
this.observing.cancel();
}
this.observing=_22.observe(function(_27,_28,_29){
if(_5.indexOf(_1d._dirtyObjects,_27)==-1){
if(_28==-1){
_1d.onNew(_27);
}else{
if(_29==-1){
_1d.onDelete(_27);
}else{
for(var i in _27){
if(i!=_1d.objectStore.idProperty){
_1d.onSet(_27,i,null,_27[i]);
}
}
}
}
}
},true);
}
this.onFetch(_22);
_1c.store=this;
return _1c;
},getFeatures:function(){
return {"dojo.data.api.Read":!!this.objectStore.get,"dojo.data.api.Identity":true,"dojo.data.api.Write":!!this.objectStore.put,"dojo.data.api.Notification":true};
},getLabel:function(_2a){
if(this.isItem(_2a)){
return this.getValue(_2a,this.labelProperty);
}
return undefined;
},getLabelAttributes:function(_2b){
return [this.labelProperty];
},getIdentity:function(_2c){
return this.objectStore.getIdentity?this.objectStore.getIdentity(_2c):_2c[this.objectStore.idProperty||"id"];
},getIdentityAttributes:function(_2d){
return [this.objectStore.idProperty];
},fetchItemByIdentity:function(_2e){
var _2f;
_4.when(this.objectStore.get(_2e.identity),function(_30){
_2f=_30;
_2e.onItem.call(_2e.scope,_30);
},function(_31){
_2e.onError.call(_2e.scope,_31);
});
return _2f;
},newItem:function(_32,_33){
if(_33){
var _34=this.getValue(_33.parent,_33.attribute,[]);
_34=_34.concat([_32]);
_32.__parent=_34;
this.setValue(_33.parent,_33.attribute,_34);
}
this._dirtyObjects.push({object:_32,save:true});
this.onNew(_32);
return _32;
},deleteItem:function(_35){
this.changing(_35,true);
this.onDelete(_35);
},setValue:function(_36,_37,_38){
var old=_36[_37];
this.changing(_36);
_36[_37]=_38;
this.onSet(_36,_37,old,_38);
},setValues:function(_39,_3a,_3b){
if(!_1.isArray(_3b)){
throw new Error("setValues expects to be passed an Array object as its value");
}
this.setValue(_39,_3a,_3b);
},unsetAttribute:function(_3c,_3d){
this.changing(_3c);
var old=_3c[_3d];
delete _3c[_3d];
this.onSet(_3c,_3d,old,undefined);
},_dirtyObjects:[],changing:function(_3e,_3f){
_3e.__isDirty=true;
for(var i=0;i<this._dirtyObjects.length;i++){
var _40=this._dirtyObjects[i];
if(_3e==_40.object){
if(_3f){
_40.object=false;
if(!this._saveNotNeeded){
_40.save=true;
}
}
return;
}
}
var old=_3e instanceof Array?[]:{};
for(i in _3e){
if(_3e.hasOwnProperty(i)){
old[i]=_3e[i];
}
}
this._dirtyObjects.push({object:!_3f&&_3e,old:old,save:!this._saveNotNeeded});
},save:function(_41){
_41=_41||{};
var _42,_43=[];
var _44=[];
var _45=this;
var _46=this._dirtyObjects;
var _47=_46.length;
try{
_6.connect(_41,"onError",function(){
if(_41.revertOnError!==false){
var _48=_46;
_46=_44;
_45.revert();
_45._dirtyObjects=_48;
}else{
_45._dirtyObjects=_46.concat(_44);
}
});
if(this.objectStore.transaction){
var _49=this.objectStore.transaction();
}
for(var i=0;i<_46.length;i++){
var _4a=_46[i];
var _4b=_4a.object;
var old=_4a.old;
delete _4b.__isDirty;
if(_4b){
_42=this.objectStore.put(_4b,{overwrite:!!old});
}else{
if(typeof old!="undefined"){
_42=this.objectStore.remove(this.getIdentity(old));
}
}
_44.push(_4a);
_46.splice(i--,1);
_4.when(_42,function(_4c){
if(!(--_47)){
if(_41.onComplete){
_41.onComplete.call(_41.scope,_43);
}
}
},function(_4d){
_47=-1;
_41.onError.call(_41.scope,_4d);
});
}
if(_49){
_49.commit();
}
}
catch(e){
_41.onError.call(_41.scope,value);
}
},revert:function(_4e){
var _4f=this._dirtyObjects;
for(var i=_4f.length;i>0;){
i--;
var _50=_4f[i];
var _51=_50.object;
var old=_50.old;
if(_51&&old){
for(var j in old){
if(old.hasOwnProperty(j)&&_51[j]!==old[j]){
this.onSet(_51,j,_51[j],old[j]);
_51[j]=old[j];
}
}
for(j in _51){
if(!old.hasOwnProperty(j)){
this.onSet(_51,j,_51[j]);
delete _51[j];
}
}
}else{
if(!old){
this.onDelete(_51);
}else{
this.onNew(old);
}
}
delete (_51||old).__isDirty;
_4f.splice(i,1);
}
},isDirty:function(_52){
if(!_52){
return !!this._dirtyObjects.length;
}
return _52.__isDirty;
},onSet:function(){
},onNew:function(){
},onDelete:function(){
},onFetch:function(_53){
}});
});
