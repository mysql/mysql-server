//>>built
define("dojox/rpc/JsonRest",["dojo","dojox","dojox/json/ref","dojox/rpc/Rest"],function(_1,_2){
var _3=[];
var _4=_2.rpc.Rest;
var jr;
function _5(_6,_7,_8,_9){
var _a=_7.ioArgs&&_7.ioArgs.xhr&&_7.ioArgs.xhr.getResponseHeader("Last-Modified");
if(_a&&_4._timeStamps){
_4._timeStamps[_9]=_a;
}
var _b=_6._schema&&_6._schema.hrefProperty;
if(_b){
_2.json.ref.refAttribute=_b;
}
_8=_8&&_2.json.ref.resolveJson(_8,{defaultId:_9,index:_4._index,timeStamps:_a&&_4._timeStamps,time:_a,idPrefix:_6.servicePath.replace(/[^\/]*$/,""),idAttribute:jr.getIdAttribute(_6),schemas:jr.schemas,loader:jr._loader,idAsRef:_6.idAsRef,assignAbsoluteIds:true});
_2.json.ref.refAttribute="$ref";
return _8;
};
jr=_2.rpc.JsonRest={serviceClass:_2.rpc.Rest,conflictDateHeader:"If-Unmodified-Since",commit:function(_c){
_c=_c||{};
var _d=[];
var _e={};
var _f=[];
for(var i=0;i<_3.length;i++){
var _10=_3[i];
var _11=_10.object;
var old=_10.old;
var _12=false;
if(!(_c.service&&(_11||old)&&(_11||old).__id.indexOf(_c.service.servicePath))&&_10.save){
delete _11.__isDirty;
if(_11){
if(old){
var _13;
if((_13=_11.__id.match(/(.*)#.*/))){
_11=_4._index[_13[1]];
}
if(!(_11.__id in _e)){
_e[_11.__id]=_11;
if(_c.incrementalUpdates&&!_13){
var _14=(typeof _c.incrementalUpdates=="function"?_c.incrementalUpdates:function(){
_14={};
for(var j in _11){
if(_11.hasOwnProperty(j)){
if(_11[j]!==old[j]){
_14[j]=_11[j];
}
}else{
if(old.hasOwnProperty(j)){
return null;
}
}
}
return _14;
})(_11,old);
}
if(_14){
_d.push({method:"post",target:_11,content:_14});
}else{
_d.push({method:"put",target:_11,content:_11});
}
}
}else{
var _15=jr.getServiceAndId(_11.__id).service;
var _16=jr.getIdAttribute(_15);
if((_16 in _11)&&!_c.alwaysPostNewItems){
_d.push({method:"put",target:_11,content:_11});
}else{
_d.push({method:"post",target:{__id:_15.servicePath},content:_11});
}
}
}else{
if(old){
_d.push({method:"delete",target:old});
}
}
_f.push(_10);
_3.splice(i--,1);
}
}
_1.connect(_c,"onError",function(){
if(_c.revertOnError!==false){
var _17=_3;
_3=_f;
var _18=0;
jr.revert();
_3=_17;
}else{
_1.forEach(_f,function(obj){
jr.changing(obj.object,!obj.object);
});
}
});
jr.sendToServer(_d,_c);
return _d;
},sendToServer:function(_19,_1a){
var _1b;
var _1c=_1.xhr;
var _1d=_19.length;
var i,_1e;
var _1f;
var _20=this.conflictDateHeader;
_1.xhr=function(_21,_22){
_22.headers=_22.headers||{};
_22.headers["Transaction"]=_19.length-1==i?"commit":"open";
if(_20&&_1f){
_22.headers[_20]=_1f;
}
if(_1e){
_22.headers["Content-ID"]="<"+_1e+">";
}
return _1c.apply(_1,arguments);
};
for(i=0;i<_19.length;i++){
var _23=_19[i];
_2.rpc.JsonRest._contentId=_23.content&&_23.content.__id;
var _24=_23.method=="post";
_1f=_23.method=="put"&&_4._timeStamps[_23.content.__id];
if(_1f){
_4._timeStamps[_23.content.__id]=(new Date())+"";
}
_1e=_24&&_2.rpc.JsonRest._contentId;
var _25=jr.getServiceAndId(_23.target.__id);
var _26=_25.service;
var dfd=_23.deferred=_26[_23.method](_25.id.replace(/#/,""),_2.json.ref.toJson(_23.content,false,_26.servicePath,true));
(function(_27,dfd,_28){
dfd.addCallback(function(_29){
try{
var _2a=dfd.ioArgs.xhr&&dfd.ioArgs.xhr.getResponseHeader("Location");
if(_2a){
var _2b=_2a.match(/(^\w+:\/\/)/)&&_2a.indexOf(_28.servicePath);
_2a=_2b>0?_2a.substring(_2b):(_28.servicePath+_2a).replace(/^(.*\/)?(\w+:\/\/)|[^\/\.]+\/\.\.\/|^.*\/(\/)/,"$2$3");
_27.__id=_2a;
_4._index[_2a]=_27;
}
_29=_5(_28,dfd,_29,_27&&_27.__id);
}
catch(e){
}
if(!(--_1d)){
if(_1a.onComplete){
_1a.onComplete.call(_1a.scope,_19);
}
}
return _29;
});
})(_23.content,dfd,_26);
dfd.addErrback(function(_2c){
_1d=-1;
_1a.onError.call(_1a.scope,_2c);
});
}
_1.xhr=_1c;
},getDirtyObjects:function(){
return _3;
},revert:function(_2d){
for(var i=_3.length;i>0;){
i--;
var _2e=_3[i];
var _2f=_2e.object;
var old=_2e.old;
var _30=_2.data._getStoreForItem(_2f||old);
if(!(_2d&&(_2f||old)&&(_2f||old).__id.indexOf(_2d.servicePath))){
if(_2f&&old){
for(var j in old){
if(old.hasOwnProperty(j)&&_2f[j]!==old[j]){
if(_30){
_30.onSet(_2f,j,_2f[j],old[j]);
}
_2f[j]=old[j];
}
}
for(j in _2f){
if(!old.hasOwnProperty(j)){
if(_30){
_30.onSet(_2f,j,_2f[j]);
}
delete _2f[j];
}
}
}else{
if(!old){
if(_30){
_30.onDelete(_2f);
}
}else{
if(_30){
_30.onNew(old);
}
}
}
delete (_2f||old).__isDirty;
_3.splice(i,1);
}
}
},changing:function(_31,_32){
if(!_31.__id){
return;
}
_31.__isDirty=true;
for(var i=0;i<_3.length;i++){
var _33=_3[i];
if(_31==_33.object){
if(_32){
_33.object=false;
if(!this._saveNotNeeded){
_33.save=true;
}
}
return;
}
}
var old=_31 instanceof Array?[]:{};
for(i in _31){
if(_31.hasOwnProperty(i)){
old[i]=_31[i];
}
}
_3.push({object:!_32&&_31,old:old,save:!this._saveNotNeeded});
},deleteObject:function(_34){
this.changing(_34,true);
},getConstructor:function(_35,_36){
if(typeof _35=="string"){
var _37=_35;
_35=new _2.rpc.Rest(_35,true);
this.registerService(_35,_37,_36);
}
if(_35._constructor){
return _35._constructor;
}
_35._constructor=function(_38){
var _39=this;
var _3a=arguments;
var _3b;
var _3c;
function _3d(_3e){
if(_3e){
_3d(_3e["extends"]);
_3b=_3e.properties;
for(var i in _3b){
var _3f=_3b[i];
if(_3f&&(typeof _3f=="object")&&("default" in _3f)){
_39[i]=_3f["default"];
}
}
}
if(_3e&&_3e.prototype&&_3e.prototype.initialize){
_3c=true;
_3e.prototype.initialize.apply(_39,_3a);
}
};
_3d(_35._schema);
if(!_3c&&_38&&typeof _38=="object"){
_1.mixin(_39,_38);
}
var _40=jr.getIdAttribute(_35);
_4._index[this.__id=this.__clientId=_35.servicePath+(this[_40]||Math.random().toString(16).substring(2,14)+"@"+((_2.rpc.Client&&_2.rpc.Client.clientId)||"client"))]=this;
if(_2.json.schema&&_3b){
_2.json.schema.mustBeValid(_2.json.schema.validate(this,_35._schema));
}
_3.push({object:this,save:true});
};
return _1.mixin(_35._constructor,_35._schema,{load:_35});
},fetch:function(_41){
var _42=jr.getServiceAndId(_41);
return this.byId(_42.service,_42.id);
},getIdAttribute:function(_43){
var _44=_43._schema;
var _45;
if(_44){
if(!(_45=_44._idAttr)){
for(var i in _44.properties){
if(_44.properties[i].identity||(_44.properties[i].link=="self")){
_44._idAttr=_45=i;
}
}
}
}
return _45||"id";
},getServiceAndId:function(_46){
var _47="";
for(var _48 in jr.services){
if((_46.substring(0,_48.length)==_48)&&(_48.length>=_47.length)){
_47=_48;
}
}
if(_47){
return {service:jr.services[_47],id:_46.substring(_47.length)};
}
var _49=_46.match(/^(.*\/)([^\/]*)$/);
return {service:new jr.serviceClass(_49[1],true),id:_49[2]};
},services:{},schemas:{},registerService:function(_4a,_4b,_4c){
_4b=_4a.servicePath=_4b||_4a.servicePath;
_4a._schema=jr.schemas[_4b]=_4c||_4a._schema||{};
jr.services[_4b]=_4a;
},byId:function(_4d,id){
var _4e,_4f=_4._index[(_4d.servicePath||"")+id];
if(_4f&&!_4f._loadObject){
_4e=new _1.Deferred();
_4e.callback(_4f);
return _4e;
}
return this.query(_4d,id);
},query:function(_50,id,_51){
var _52=_50(id,_51);
_52.addCallback(function(_53){
if(_53.nodeType&&_53.cloneNode){
return _53;
}
return _5(_50,_52,_53,typeof id!="string"||(_51&&(_51.start||_51.count))?undefined:id);
});
return _52;
},_loader:function(_54){
var _55=jr.getServiceAndId(this.__id);
var _56=this;
jr.query(_55.service,_55.id).addBoth(function(_57){
if(_57==_56){
delete _57.$ref;
delete _57._loadObject;
}else{
_56._loadObject=function(_58){
_58(_57);
};
}
_54(_57);
});
},isDirty:function(_59,_5a){
if(!_59){
if(_5a){
return _1.some(_3,function(_5b){
return _2.data._getStoreForItem(_5b.object||_5b.old)==_5a;
});
}
return !!_3.length;
}
return _59.__isDirty;
}};
return _2.rpc.JsonRest;
});
