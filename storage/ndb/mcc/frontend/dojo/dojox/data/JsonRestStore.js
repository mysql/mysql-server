//>>built
define("dojox/data/JsonRestStore",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojox/rpc/Rest","dojox/rpc/JsonRest","dojox/json/schema","dojox/data/ServiceStore"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_2("dojox.data.JsonRestStore",_7,{constructor:function(_9){
_3.connect(_4._index,"onUpdate",this,function(_a,_b,_c,_d){
var _e=this.service.servicePath;
if(!_a.__id){
}else{
if(_a.__id.substring(0,_e.length)==_e){
this.onSet(_a,_b,_c,_d);
}
}
});
this.idAttribute=this.idAttribute||"id";
if(typeof _9.target=="string"){
_9.target=_9.target.match(/\/$/)||this.allowNoTrailingSlash?_9.target:(_9.target+"/");
if(!this.service){
this.service=_5.services[_9.target]||_4(_9.target,true);
}
}
_5.registerService(this.service,_9.target,this.schema);
this.schema=this.service._schema=this.schema||this.service._schema||{};
this.service._store=this;
this.service.idAsRef=this.idAsRef;
this.schema._idAttr=this.idAttribute;
var _f=_5.getConstructor(this.service);
var _10=this;
this._constructor=function(_11){
_f.call(this,_11);
_10.onNew(this);
};
this._constructor.prototype=_f.prototype;
this._index=_4._index;
},loadReferencedSchema:true,idAsRef:false,referenceIntegrity:true,target:"",allowNoTrailingSlash:false,newItem:function(_12,_13){
_12=new this._constructor(_12);
if(_13){
var _14=this.getValue(_13.parent,_13.attribute,[]);
_14=_14.concat([_12]);
_12.__parent=_14;
this.setValue(_13.parent,_13.attribute,_14);
}
return _12;
},deleteItem:function(_15){
var _16=[];
var _17=_18._getStoreForItem(_15)||this;
if(this.referenceIntegrity){
_5._saveNotNeeded=true;
var _19=_4._index;
var _1a=function(_1b){
var _1c;
_16.push(_1b);
_1b.__checked=1;
for(var i in _1b){
if(i.substring(0,2)!="__"){
var _1d=_1b[i];
if(_1d==_15){
if(_1b!=_19){
if(_1b instanceof Array){
(_1c=_1c||[]).push(i);
}else{
(_18._getStoreForItem(_1b)||_17).unsetAttribute(_1b,i);
}
}
}else{
if((typeof _1d=="object")&&_1d){
if(!_1d.__checked){
_1a(_1d);
}
if(typeof _1d.__checked=="object"&&_1b!=_19){
(_18._getStoreForItem(_1b)||_17).setValue(_1b,i,_1d.__checked);
}
}
}
}
}
if(_1c){
i=_1c.length;
_1b=_1b.__checked=_1b.concat();
while(i--){
_1b.splice(_1c[i],1);
}
return _1b;
}
return null;
};
_1a(_19);
_5._saveNotNeeded=false;
var i=0;
while(_16[i]){
delete _16[i++].__checked;
}
}
_5.deleteObject(_15);
_17.onDelete(_15);
},changing:function(_1e,_1f){
_5.changing(_1e,_1f);
},cancelChanging:function(_20){
if(!_20.__id){
return;
}
dirtyObjects=_21=_5.getDirtyObjects();
for(var i=0;i<dirtyObjects.length;i++){
var _21=dirtyObjects[i];
if(_20==_21.object){
dirtyObjects.splice(i,1);
return;
}
}
},setValue:function(_22,_23,_24){
var old=_22[_23];
var _25=_22.__id?_18._getStoreForItem(_22):this;
if(_6&&_25.schema&&_25.schema.properties){
_6.mustBeValid(_6.checkPropertyChange(_24,_25.schema.properties[_23]));
}
if(_23==_25.idAttribute){
throw new Error("Can not change the identity attribute for an item");
}
_25.changing(_22);
_22[_23]=_24;
if(_24&&!_24.__parent){
_24.__parent=_22;
}
_25.onSet(_22,_23,old,_24);
},setValues:function(_26,_27,_28){
if(!_1.isArray(_28)){
throw new Error("setValues expects to be passed an Array object as its value");
}
this.setValue(_26,_27,_28);
},unsetAttribute:function(_29,_2a){
this.changing(_29);
var old=_29[_2a];
delete _29[_2a];
this.onSet(_29,_2a,old,undefined);
},save:function(_2b){
if(!(_2b&&_2b.global)){
(_2b=_2b||{}).service=this.service;
}
if("syncMode" in _2b?_2b.syncMode:this.syncMode){
rpcConfig._sync=true;
}
var _2c=_5.commit(_2b);
this.serverVersion=this._updates&&this._updates.length;
return _2c;
},revert:function(_2d){
_5.revert(_2d&&_2d.global&&this.service);
},isDirty:function(_2e){
return _5.isDirty(_2e,this);
},isItem:function(_2f,_30){
return _2f&&_2f.__id&&(_30||this.service==_5.getServiceAndId(_2f.__id).service);
},_doQuery:function(_31){
var _32=typeof _31.queryStr=="string"?_31.queryStr:_31.query;
var _33=_5.query(this.service,_32,_31);
var _34=this;
if(this.loadReferencedSchema){
_33.addCallback(function(_35){
var _36=_33.ioArgs&&_33.ioArgs.xhr&&_33.ioArgs.xhr.getResponseHeader("Content-Type");
var _37=_36&&_36.match(/definedby\s*=\s*([^;]*)/);
if(_36&&!_37){
_37=_33.ioArgs.xhr.getResponseHeader("Link");
_37=_37&&_37.match(/<([^>]*)>;\s*rel="?definedby"?/);
}
_37=_37&&_37[1];
if(_37){
var _38=_5.getServiceAndId((_34.target+_37).replace(/^(.*\/)?(\w+:\/\/)|[^\/\.]+\/\.\.\/|^.*\/(\/)/,"$2$3"));
var _39=_5.byId(_38.service,_38.id);
_39.addCallbacks(function(_3a){
_1.mixin(_34.schema,_3a);
return _35;
},function(_3b){
console.error(_3b);
return _35;
});
return _39;
}
return undefined;
});
}
return _33;
},_processResults:function(_3c,_3d){
var _3e=_3c.length;
return {totalCount:_3d.fullLength||(_3d.request.count==_3e?(_3d.request.start||0)+_3e*2:_3e),items:_3c};
},getConstructor:function(){
return this._constructor;
},getIdentity:function(_3f){
var id=_3f.__clientId||_3f.__id;
if(!id){
return id;
}
var _40=this.service.servicePath.replace(/[^\/]*$/,"");
return id.substring(0,_40.length)!=_40?id:id.substring(_40.length);
},fetchItemByIdentity:function(_41){
var id=_41.identity;
var _42=this;
if(id.toString().match(/^(\w*:)?\//)){
var _43=_5.getServiceAndId(id);
_42=_43.service._store;
_41.identity=_43.id;
}
_41._prefix=_42.service.servicePath.replace(/[^\/]*$/,"");
return _42.inherited(arguments);
},onSet:function(){
},onNew:function(){
},onDelete:function(){
},getFeatures:function(){
var _44=this.inherited(arguments);
_44["dojo.data.api.Write"]=true;
_44["dojo.data.api.Notification"]=true;
return _44;
},getParent:function(_45){
return _45&&_45.__parent;
}});
_8.getStore=function(_46,_47){
if(typeof _46.target=="string"){
_46.target=_46.target.match(/\/$/)||_46.allowNoTrailingSlash?_46.target:(_46.target+"/");
var _48=(_5.services[_46.target]||{})._store;
if(_48){
return _48;
}
}
return new (_47||_8)(_46);
};
var _18=_1.getObject("dojox.data",true);
_18._getStoreForItem=function(_49){
if(_49.__id){
var _4a=_5.getServiceAndId(_49.__id);
if(_4a&&_4a.service._store){
return _4a.service._store;
}else{
var _4b=_49.__id.toString().match(/.*\//)[0];
return new _8({target:_4b});
}
}
return null;
};
var _4c=_1.getObject("dojox.json.ref",true);
_4c._useRefs=true;
return _8;
});
