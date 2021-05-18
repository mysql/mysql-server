//>>built
define("dojox/data/JsonRestStore",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojox/rpc/Rest","dojox/rpc/JsonRest","dojox/json/schema","dojox/data/ServiceStore"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_1.getObject("dojox.rpc",true);
var _9=_2("dojox.data.JsonRestStore",_7,{constructor:function(_a){
_3.connect(_4._index,"onUpdate",this,function(_b,_c,_d,_e){
var _f=this.service.servicePath;
if(!_b.__id){
}else{
if(_b.__id.substring(0,_f.length)==_f){
this.onSet(_b,_c,_d,_e);
}
}
});
this.idAttribute=this.idAttribute||"id";
if(typeof _a.target=="string"){
_a.target=_a.target.match(/\/$/)||this.allowNoTrailingSlash?_a.target:(_a.target+"/");
if(!this.service){
this.service=_5.services[_a.target]||_4(_a.target,true);
}
}
_5.registerService(this.service,_a.target,this.schema);
this.schema=this.service._schema=this.schema||this.service._schema||{};
this.service._store=this;
this.service.idAsRef=this.idAsRef;
this.schema._idAttr=this.idAttribute;
var _10=_5.getConstructor(this.service);
var _11=this;
this._constructor=function(_12){
_10.call(this,_12);
_11.onNew(this);
};
this._constructor.prototype=_10.prototype;
this._index=_4._index;
},loadReferencedSchema:true,idAsRef:false,referenceIntegrity:true,target:"",allowNoTrailingSlash:false,newItem:function(_13,_14){
_13=new this._constructor(_13);
if(_14){
var _15=this.getValue(_14.parent,_14.attribute,[]);
_15=_15.concat([_13]);
_13.__parent=_15;
this.setValue(_14.parent,_14.attribute,_15);
}
return _13;
},deleteItem:function(_16){
var _17=[];
var _18=_19._getStoreForItem(_16)||this;
if(this.referenceIntegrity){
_5._saveNotNeeded=true;
var _1a=_4._index;
var _1b=function(_1c){
var _1d;
_17.push(_1c);
_1c.__checked=1;
for(var i in _1c){
if(i.substring(0,2)!="__"){
var _1e=_1c[i];
if(_1e==_16){
if(_1c!=_1a){
if(_1c instanceof Array){
(_1d=_1d||[]).push(i);
}else{
(_19._getStoreForItem(_1c)||_18).unsetAttribute(_1c,i);
}
}
}else{
if((typeof _1e=="object")&&_1e){
if(!_1e.__checked){
_1b(_1e);
}
if(typeof _1e.__checked=="object"&&_1c!=_1a){
(_19._getStoreForItem(_1c)||_18).setValue(_1c,i,_1e.__checked);
}
}
}
}
}
if(_1d){
i=_1d.length;
_1c=_1c.__checked=_1c.concat();
while(i--){
_1c.splice(_1d[i],1);
}
return _1c;
}
return null;
};
_1b(_1a);
_5._saveNotNeeded=false;
var i=0;
while(_17[i]){
delete _17[i++].__checked;
}
}
_5.deleteObject(_16);
_18.onDelete(_16);
},changing:function(_1f,_20){
_5.changing(_1f,_20);
},cancelChanging:function(_21){
if(!_21.__id){
return;
}
dirtyObjects=_22=_5.getDirtyObjects();
for(var i=0;i<dirtyObjects.length;i++){
var _22=dirtyObjects[i];
if(_21==_22.object){
dirtyObjects.splice(i,1);
return;
}
}
},setValue:function(_23,_24,_25){
var old=_23[_24];
var _26=_23.__id?_19._getStoreForItem(_23):this;
if(_6&&_26.schema&&_26.schema.properties){
_6.mustBeValid(_6.checkPropertyChange(_25,_26.schema.properties[_24]));
}
if(_24==_26.idAttribute){
throw new Error("Can not change the identity attribute for an item");
}
_26.changing(_23);
_23[_24]=_25;
if(_25&&!_25.__parent){
_25.__parent=_23;
}
_26.onSet(_23,_24,old,_25);
},setValues:function(_27,_28,_29){
if(!_1.isArray(_29)){
throw new Error("setValues expects to be passed an Array object as its value");
}
this.setValue(_27,_28,_29);
},unsetAttribute:function(_2a,_2b){
this.changing(_2a);
var old=_2a[_2b];
delete _2a[_2b];
this.onSet(_2a,_2b,old,undefined);
},save:function(_2c){
if(!(_2c&&_2c.global)){
(_2c=_2c||{}).service=this.service;
}
if("syncMode" in _2c?_2c.syncMode:this.syncMode){
_8._sync=true;
}
var _2d=_5.commit(_2c);
this.serverVersion=this._updates&&this._updates.length;
return _2d;
},revert:function(_2e){
_5.revert(!(_2e&&_2e.global)&&this.service);
},isDirty:function(_2f){
return _5.isDirty(_2f,this);
},isItem:function(_30,_31){
return _30&&_30.__id&&(_31||this.service==_5.getServiceAndId(_30.__id).service);
},_doQuery:function(_32){
var _33=typeof _32.queryStr=="string"?_32.queryStr:_32.query;
var _34=_5.query(this.service,_33,_32);
var _35=this;
if(this.loadReferencedSchema){
_34.addCallback(function(_36){
var _37=_34.ioArgs&&_34.ioArgs.xhr&&_34.ioArgs.xhr.getResponseHeader("Content-Type");
var _38=_37&&_37.match(/definedby\s*=\s*([^;]*)/);
if(_37&&!_38){
_38=_34.ioArgs.xhr.getResponseHeader("Link");
_38=_38&&_38.match(/<([^>]*)>;\s*rel="?definedby"?/);
}
_38=_38&&_38[1];
if(_38){
var _39=_5.getServiceAndId((_35.target+_38).replace(/^(.*\/)?(\w+:\/\/)|[^\/\.]+\/\.\.\/|^.*\/(\/)/,"$2$3"));
var _3a=_5.byId(_39.service,_39.id);
_3a.addCallbacks(function(_3b){
_1.mixin(_35.schema,_3b);
return _36;
},function(_3c){
console.error(_3c);
return _36;
});
return _3a;
}
return undefined;
});
}
return _34;
},_processResults:function(_3d,_3e){
var _3f=_3d.length;
return {totalCount:_3e.fullLength||(_3e.request.count==_3f?(_3e.request.start||0)+_3f*2:_3f),items:_3d};
},getConstructor:function(){
return this._constructor;
},getIdentity:function(_40){
var id=_40.__clientId||_40.__id;
if(!id){
return id;
}
var _41=this.service.servicePath.replace(/[^\/]*$/,"");
return id.substring(0,_41.length)!=_41?id:id.substring(_41.length);
},fetchItemByIdentity:function(_42){
var id=_42.identity;
var _43=this;
if(id.toString().match(/^(\w*:)?\//)){
var _44=_5.getServiceAndId(id);
_43=_44.service._store;
_42.identity=_44.id;
}
_42._prefix=_43.service.servicePath.replace(/[^\/]*$/,"");
return _43.inherited(arguments);
},onSet:function(){
},onNew:function(){
},onDelete:function(){
},getFeatures:function(){
var _45=this.inherited(arguments);
_45["dojo.data.api.Write"]=true;
_45["dojo.data.api.Notification"]=true;
return _45;
},getParent:function(_46){
return _46&&_46.__parent;
}});
_9.getStore=function(_47,_48){
if(typeof _47.target=="string"){
_47.target=_47.target.match(/\/$/)||_47.allowNoTrailingSlash?_47.target:(_47.target+"/");
var _49=(_5.services[_47.target]||{})._store;
if(_49){
return _49;
}
}
return new (_48||_9)(_47);
};
var _19=_1.getObject("dojox.data",true);
_19._getStoreForItem=function(_4a){
if(_4a.__id){
var _4b=_5.getServiceAndId(_4a.__id);
if(_4b&&_4b.service._store){
return _4b.service._store;
}else{
var _4c=_4a.__id.toString().match(/.*\//)[0];
return new _9({target:_4c});
}
}
return null;
};
var _4d=_1.getObject("dojox.json.ref",true);
_4d._useRefs=true;
return _9;
});
