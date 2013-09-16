//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/storage/manager,dojox/storage/Provider"],function(_1,_2,_3){
_2.provide("dojox.storage.AirDBStorageProvider");
_2.require("dojox.storage.manager");
_2.require("dojox.storage.Provider");
if(_2.isAIR){
(function(){
if(!_4){
var _4={};
}
_4.File=window.runtime.flash.filesystem.File;
_4.SQLConnection=window.runtime.flash.data.SQLConnection;
_4.SQLStatement=window.runtime.flash.data.SQLStatement;
_2.declare("dojox.storage.AirDBStorageProvider",[_3.storage.Provider],{DATABASE_FILE:"dojo.db",TABLE_NAME:"__DOJO_STORAGE",initialized:false,_db:null,initialize:function(){
this.initialized=false;
try{
this._db=new _4.SQLConnection();
this._db.open(_4.File.applicationStorageDirectory.resolvePath(this.DATABASE_FILE));
this._sql("CREATE TABLE IF NOT EXISTS "+this.TABLE_NAME+"(namespace TEXT, key TEXT, value TEXT)");
this._sql("CREATE UNIQUE INDEX IF NOT EXISTS namespace_key_index ON "+this.TABLE_NAME+" (namespace, key)");
this.initialized=true;
}
catch(e){
}
_3.storage.manager.loaded();
},_sql:function(_5,_6){
var _7=new _4.SQLStatement();
_7.sqlConnection=this._db;
_7.text=_5;
if(_6){
for(var _8 in _6){
_7.parameters[_8]=_6[_8];
}
}
_7.execute();
return _7.getResult();
},_beginTransaction:function(){
this._db.begin();
},_commitTransaction:function(){
this._db.commit();
},isAvailable:function(){
return true;
},put:function(_9,_a,_b,_c){
if(this.isValidKey(_9)==false){
throw new Error("Invalid key given: "+_9);
}
_c=_c||this.DEFAULT_NAMESPACE;
if(this.isValidKey(_c)==false){
throw new Error("Invalid namespace given: "+_c);
}
try{
this._sql("DELETE FROM "+this.TABLE_NAME+" WHERE namespace = :namespace AND key = :key",{":namespace":_c,":key":_9});
this._sql("INSERT INTO "+this.TABLE_NAME+" VALUES (:namespace, :key, :value)",{":namespace":_c,":key":_9,":value":_a});
}
catch(e){
_b(this.FAILED,_9,e.toString());
return;
}
if(_b){
_b(this.SUCCESS,_9,null,_c);
}
},get:function(_d,_e){
if(this.isValidKey(_d)==false){
throw new Error("Invalid key given: "+_d);
}
_e=_e||this.DEFAULT_NAMESPACE;
var _f=this._sql("SELECT * FROM "+this.TABLE_NAME+" WHERE namespace = :namespace AND key = :key",{":namespace":_e,":key":_d});
if(_f.data&&_f.data.length){
return _f.data[0].value;
}
return null;
},getNamespaces:function(){
var _10=[this.DEFAULT_NAMESPACE];
var rs=this._sql("SELECT namespace FROM "+this.TABLE_NAME+" DESC GROUP BY namespace");
if(rs.data){
for(var i=0;i<rs.data.length;i++){
if(rs.data[i].namespace!=this.DEFAULT_NAMESPACE){
_10.push(rs.data[i].namespace);
}
}
}
return _10;
},getKeys:function(_11){
_11=_11||this.DEFAULT_NAMESPACE;
if(this.isValidKey(_11)==false){
throw new Error("Invalid namespace given: "+_11);
}
var _12=[];
var rs=this._sql("SELECT key FROM "+this.TABLE_NAME+" WHERE namespace = :namespace",{":namespace":_11});
if(rs.data){
for(var i=0;i<rs.data.length;i++){
_12.push(rs.data[i].key);
}
}
return _12;
},clear:function(_13){
if(this.isValidKey(_13)==false){
throw new Error("Invalid namespace given: "+_13);
}
this._sql("DELETE FROM "+this.TABLE_NAME+" WHERE namespace = :namespace",{":namespace":_13});
},remove:function(key,_14){
_14=_14||this.DEFAULT_NAMESPACE;
this._sql("DELETE FROM "+this.TABLE_NAME+" WHERE namespace = :namespace AND key = :key",{":namespace":_14,":key":key});
},putMultiple:function(_15,_16,_17,_18){
if(this.isValidKeyArray(_15)===false||!_16 instanceof Array||_15.length!=_16.length){
throw new Error("Invalid arguments: keys = ["+_15+"], values = ["+_16+"]");
}
if(_18==null||typeof _18=="undefined"){
_18=this.DEFAULT_NAMESPACE;
}
if(this.isValidKey(_18)==false){
throw new Error("Invalid namespace given: "+_18);
}
this._statusHandler=_17;
try{
this._beginTransaction();
for(var i=0;i<_15.length;i++){
this._sql("DELETE FROM "+this.TABLE_NAME+" WHERE namespace = :namespace AND key = :key",{":namespace":_18,":key":_15[i]});
this._sql("INSERT INTO "+this.TABLE_NAME+" VALUES (:namespace, :key, :value)",{":namespace":_18,":key":_15[i],":value":_16[i]});
}
this._commitTransaction();
}
catch(e){
if(_17){
_17(this.FAILED,_15,e.toString(),_18);
}
return;
}
if(_17){
_17(this.SUCCESS,_15,null);
}
},getMultiple:function(_19,_1a){
if(this.isValidKeyArray(_19)===false){
throw new Error("Invalid key array given: "+_19);
}
if(_1a==null||typeof _1a=="undefined"){
_1a=this.DEFAULT_NAMESPACE;
}
if(this.isValidKey(_1a)==false){
throw new Error("Invalid namespace given: "+_1a);
}
var _1b=[];
for(var i=0;i<_19.length;i++){
var _1c=this._sql("SELECT * FROM "+this.TABLE_NAME+" WHERE namespace = :namespace AND key = :key",{":namespace":_1a,":key":_19[i]});
_1b[i]=_1c.data&&_1c.data.length?_1c.data[0].value:null;
}
return _1b;
},removeMultiple:function(_1d,_1e){
_1e=_1e||this.DEFAULT_NAMESPACE;
this._beginTransaction();
for(var i=0;i<_1d.length;i++){
this._sql("DELETE FROM "+this.TABLE_NAME+" WHERE namespace = namespace = :namespace AND key = :key",{":namespace":_1e,":key":_1d[i]});
}
this._commitTransaction();
},isPermanent:function(){
return true;
},getMaximumSize:function(){
return this.SIZE_NO_LIMIT;
},hasSettingsUI:function(){
return false;
},showSettingsUI:function(){
throw new Error(this.declaredClass+" does not support a storage settings user-interface");
},hideSettingsUI:function(){
throw new Error(this.declaredClass+" does not support a storage settings user-interface");
}});
_3.storage.manager.register("dojox.storage.AirDBStorageProvider",new _3.storage.AirDBStorageProvider());
_3.storage.manager.initialize();
})();
}
});
