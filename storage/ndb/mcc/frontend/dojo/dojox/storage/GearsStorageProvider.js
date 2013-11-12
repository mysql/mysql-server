//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/gears,dojox/storage/Provider,dojox/storage/manager,dojox/sql"],function(_1,_2,_3){
_2.provide("dojox.storage.GearsStorageProvider");
_2.require("dojo.gears");
_2.require("dojox.storage.Provider");
_2.require("dojox.storage.manager");
_2.require("dojox.sql");
if(_2.gears.available){
(function(){
_2.declare("dojox.storage.GearsStorageProvider",_3.storage.Provider,{constructor:function(){
},TABLE_NAME:"__DOJO_STORAGE",initialized:false,_available:null,_storageReady:false,initialize:function(){
if(_2.config["disableGearsStorage"]==true){
return;
}
this.TABLE_NAME="__DOJO_STORAGE";
this.initialized=true;
_3.storage.manager.loaded();
},isAvailable:function(){
return this._available=_2.gears.available;
},put:function(_4,_5,_6,_7){
this._initStorage();
if(!this.isValidKey(_4)){
throw new Error("Invalid key given: "+_4);
}
_7=_7||this.DEFAULT_NAMESPACE;
if(!this.isValidKey(_7)){
throw new Error("Invalid namespace given: "+_4);
}
if(_2.isString(_5)){
_5="string:"+_5;
}else{
_5=_2.toJson(_5);
}
try{
_3.sql("DELETE FROM "+this.TABLE_NAME+" WHERE namespace = ? AND key = ?",_7,_4);
_3.sql("INSERT INTO "+this.TABLE_NAME+" VALUES (?, ?, ?)",_7,_4,_5);
}
catch(e){
_6(this.FAILED,_4,e.toString(),_7);
return;
}
if(_6){
_6(_3.storage.SUCCESS,_4,null,_7);
}
},get:function(_8,_9){
this._initStorage();
if(!this.isValidKey(_8)){
throw new Error("Invalid key given: "+_8);
}
_9=_9||this.DEFAULT_NAMESPACE;
if(!this.isValidKey(_9)){
throw new Error("Invalid namespace given: "+_8);
}
var _a=_3.sql("SELECT * FROM "+this.TABLE_NAME+" WHERE namespace = ? AND "+" key = ?",_9,_8);
if(!_a.length){
return null;
}else{
_a=_a[0].value;
}
if(_2.isString(_a)&&(/^string:/.test(_a))){
_a=_a.substring("string:".length);
}else{
_a=_2.fromJson(_a);
}
return _a;
},getNamespaces:function(){
this._initStorage();
var _b=[_3.storage.DEFAULT_NAMESPACE];
var rs=_3.sql("SELECT namespace FROM "+this.TABLE_NAME+" DESC GROUP BY namespace");
for(var i=0;i<rs.length;i++){
if(rs[i].namespace!=_3.storage.DEFAULT_NAMESPACE){
_b.push(rs[i].namespace);
}
}
return _b;
},getKeys:function(_c){
this._initStorage();
_c=_c||this.DEFAULT_NAMESPACE;
if(!this.isValidKey(_c)){
throw new Error("Invalid namespace given: "+_c);
}
var rs=_3.sql("SELECT key FROM "+this.TABLE_NAME+" WHERE namespace = ?",_c);
var _d=[];
for(var i=0;i<rs.length;i++){
_d.push(rs[i].key);
}
return _d;
},clear:function(_e){
this._initStorage();
_e=_e||this.DEFAULT_NAMESPACE;
if(!this.isValidKey(_e)){
throw new Error("Invalid namespace given: "+_e);
}
_3.sql("DELETE FROM "+this.TABLE_NAME+" WHERE namespace = ?",_e);
},remove:function(_f,_10){
this._initStorage();
if(!this.isValidKey(_f)){
throw new Error("Invalid key given: "+_f);
}
_10=_10||this.DEFAULT_NAMESPACE;
if(!this.isValidKey(_10)){
throw new Error("Invalid namespace given: "+_f);
}
_3.sql("DELETE FROM "+this.TABLE_NAME+" WHERE namespace = ? AND"+" key = ?",_10,_f);
},putMultiple:function(_11,_12,_13,_14){
this._initStorage();
if(!this.isValidKeyArray(_11)||!_12 instanceof Array||_11.length!=_12.length){
throw new Error("Invalid arguments: keys = ["+_11+"], values = ["+_12+"]");
}
if(_14==null||typeof _14=="undefined"){
_14=_3.storage.DEFAULT_NAMESPACE;
}
if(!this.isValidKey(_14)){
throw new Error("Invalid namespace given: "+_14);
}
this._statusHandler=_13;
try{
_3.sql.open();
_3.sql.db.execute("BEGIN TRANSACTION");
var _15="REPLACE INTO "+this.TABLE_NAME+" VALUES (?, ?, ?)";
for(var i=0;i<_11.length;i++){
var _16=_12[i];
if(_2.isString(_16)){
_16="string:"+_16;
}else{
_16=_2.toJson(_16);
}
_3.sql.db.execute(_15,[_14,_11[i],_16]);
}
_3.sql.db.execute("COMMIT TRANSACTION");
_3.sql.close();
}
catch(e){
if(_13){
_13(this.FAILED,_11,e.toString(),_14);
}
return;
}
if(_13){
_13(_3.storage.SUCCESS,_11,null,_14);
}
},getMultiple:function(_17,_18){
this._initStorage();
if(!this.isValidKeyArray(_17)){
throw new ("Invalid key array given: "+_17);
}
if(_18==null||typeof _18=="undefined"){
_18=_3.storage.DEFAULT_NAMESPACE;
}
if(!this.isValidKey(_18)){
throw new Error("Invalid namespace given: "+_18);
}
var _19="SELECT * FROM "+this.TABLE_NAME+" WHERE namespace = ? AND "+" key = ?";
var _1a=[];
for(var i=0;i<_17.length;i++){
var _1b=_3.sql(_19,_18,_17[i]);
if(!_1b.length){
_1a[i]=null;
}else{
_1b=_1b[0].value;
if(_2.isString(_1b)&&(/^string:/.test(_1b))){
_1a[i]=_1b.substring("string:".length);
}else{
_1a[i]=_2.fromJson(_1b);
}
}
}
return _1a;
},removeMultiple:function(_1c,_1d){
this._initStorage();
if(!this.isValidKeyArray(_1c)){
throw new Error("Invalid arguments: keys = ["+_1c+"]");
}
if(_1d==null||typeof _1d=="undefined"){
_1d=_3.storage.DEFAULT_NAMESPACE;
}
if(!this.isValidKey(_1d)){
throw new Error("Invalid namespace given: "+_1d);
}
_3.sql.open();
_3.sql.db.execute("BEGIN TRANSACTION");
var _1e="DELETE FROM "+this.TABLE_NAME+" WHERE namespace = ? AND key = ?";
for(var i=0;i<_1c.length;i++){
_3.sql.db.execute(_1e,[_1d,_1c[i]]);
}
_3.sql.db.execute("COMMIT TRANSACTION");
_3.sql.close();
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
},_initStorage:function(){
if(this._storageReady){
return;
}
if(!google.gears.factory.hasPermission){
var _1f=null;
var _20=null;
var msg="This site would like to use Google Gears to enable "+"enhanced functionality.";
var _21=google.gears.factory.getPermission(_1f,_20,msg);
if(!_21){
throw new Error("You must give permission to use Gears in order to "+"store data");
}
}
try{
_3.sql("CREATE TABLE IF NOT EXISTS "+this.TABLE_NAME+"( "+" namespace TEXT, "+" key TEXT, "+" value TEXT "+")");
_3.sql("CREATE UNIQUE INDEX IF NOT EXISTS namespace_key_index"+" ON "+this.TABLE_NAME+" (namespace, key)");
}
catch(e){
throw new Error("Unable to create storage tables for Gears in "+"Dojo Storage");
}
this._storageReady=true;
}});
_3.storage.manager.register("dojox.storage.GearsStorageProvider",new _3.storage.GearsStorageProvider());
})();
}
});
