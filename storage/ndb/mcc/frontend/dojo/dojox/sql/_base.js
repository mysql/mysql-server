//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/sql/_crypto"],function(_1,_2,_3){
_2.provide("dojox.sql._base");
_2.require("dojox.sql._crypto");
_2.mixin(_3.sql,{dbName:null,debug:(_2.exists("dojox.sql.debug")?_3.sql.debug:false),open:function(_4){
if(this._dbOpen&&(!_4||_4==this.dbName)){
return;
}
if(!this.dbName){
this.dbName="dot_store_"+window.location.href.replace(/[^0-9A-Za-z_]/g,"_");
if(this.dbName.length>63){
this.dbName=this.dbName.substring(0,63);
}
}
if(!_4){
_4=this.dbName;
}
try{
this._initDb();
this.db.open(_4);
this._dbOpen=true;
}
catch(exp){
throw exp.message||exp;
}
},close:function(_5){
if(_2.isIE){
return;
}
if(!this._dbOpen&&(!_5||_5==this.dbName)){
return;
}
if(!_5){
_5=this.dbName;
}
try{
this.db.close(_5);
this._dbOpen=false;
}
catch(exp){
throw exp.message||exp;
}
},_exec:function(_6){
try{
this._initDb();
if(!this._dbOpen){
this.open();
this._autoClose=true;
}
var _7=null;
var _8=null;
var _9=null;
var _a=_2._toArray(_6);
_7=_a.splice(0,1)[0];
if(this._needsEncrypt(_7)||this._needsDecrypt(_7)){
_8=_a.splice(_a.length-1,1)[0];
_9=_a.splice(_a.length-1,1)[0];
}
if(this.debug){
this._printDebugSQL(_7,_a);
}
var _b;
if(this._needsEncrypt(_7)){
_b=new _3.sql._SQLCrypto("encrypt",_7,_9,_a,_8);
return null;
}else{
if(this._needsDecrypt(_7)){
_b=new _3.sql._SQLCrypto("decrypt",_7,_9,_a,_8);
return null;
}
}
var rs=this.db.execute(_7,_a);
rs=this._normalizeResults(rs);
if(this._autoClose){
this.close();
}
return rs;
}
catch(exp){
exp=exp.message||exp;
if(this._autoClose){
try{
this.close();
}
catch(e){
}
}
throw exp;
}
return null;
},_initDb:function(){
if(!this.db){
try{
this.db=google.gears.factory.create("beta.database","1.0");
}
catch(exp){
_2.setObject("google.gears.denied",true);
if(_3.off){
_3.off.onFrameworkEvent("coreOperationFailed");
}
throw "Google Gears must be allowed to run";
}
}
},_printDebugSQL:function(_c,_d){
var _e="dojox.sql(\""+_c+"\"";
for(var i=0;i<_d.length;i++){
if(typeof _d[i]=="string"){
_e+=", \""+_d[i]+"\"";
}else{
_e+=", "+_d[i];
}
}
_e+=")";
},_normalizeResults:function(rs){
var _f=[];
if(!rs){
return [];
}
while(rs.isValidRow()){
var row={};
for(var i=0;i<rs.fieldCount();i++){
var _10=rs.fieldName(i);
var _11=rs.field(i);
row[_10]=_11;
}
_f.push(row);
rs.next();
}
rs.close();
return _f;
},_needsEncrypt:function(sql){
return /encrypt\([^\)]*\)/i.test(sql);
},_needsDecrypt:function(sql){
return /decrypt\([^\)]*\)/i.test(sql);
}});
_2.declare("dojox.sql._SQLCrypto",null,{constructor:function(_12,sql,_13,_14,_15){
if(_12=="encrypt"){
this._execEncryptSQL(sql,_13,_14,_15);
}else{
this._execDecryptSQL(sql,_13,_14,_15);
}
},_execEncryptSQL:function(sql,_16,_17,_18){
var _19=this._stripCryptoSQL(sql);
var _1a=this._flagEncryptedArgs(sql,_17);
var _1b=this;
this._encrypt(_19,_16,_17,_1a,function(_1c){
var _1d=false;
var _1e=[];
var exp=null;
try{
_1e=_3.sql.db.execute(_19,_1c);
}
catch(execError){
_1d=true;
exp=execError.message||execError;
}
if(exp!=null){
if(_3.sql._autoClose){
try{
_3.sql.close();
}
catch(e){
}
}
_18(null,true,exp.toString());
return;
}
_1e=_3.sql._normalizeResults(_1e);
if(_3.sql._autoClose){
_3.sql.close();
}
if(_3.sql._needsDecrypt(sql)){
var _1f=_1b._determineDecryptedColumns(sql);
_1b._decrypt(_1e,_1f,_16,function(_20){
_18(_20,false,null);
});
}else{
_18(_1e,false,null);
}
});
},_execDecryptSQL:function(sql,_21,_22,_23){
var _24=this._stripCryptoSQL(sql);
var _25=this._determineDecryptedColumns(sql);
var _26=false;
var _27=[];
var exp=null;
try{
_27=_3.sql.db.execute(_24,_22);
}
catch(execError){
_26=true;
exp=execError.message||execError;
}
if(exp!=null){
if(_3.sql._autoClose){
try{
_3.sql.close();
}
catch(e){
}
}
_23(_27,true,exp.toString());
return;
}
_27=_3.sql._normalizeResults(_27);
if(_3.sql._autoClose){
_3.sql.close();
}
this._decrypt(_27,_25,_21,function(_28){
_23(_28,false,null);
});
},_encrypt:function(sql,_29,_2a,_2b,_2c){
this._totalCrypto=0;
this._finishedCrypto=0;
this._finishedSpawningCrypto=false;
this._finalArgs=_2a;
for(var i=0;i<_2a.length;i++){
if(_2b[i]){
var _2d=_2a[i];
var _2e=i;
this._totalCrypto++;
_3.sql._crypto.encrypt(_2d,_29,_2.hitch(this,function(_2f){
this._finalArgs[_2e]=_2f;
this._finishedCrypto++;
if(this._finishedCrypto>=this._totalCrypto&&this._finishedSpawningCrypto){
_2c(this._finalArgs);
}
}));
}
}
this._finishedSpawningCrypto=true;
},_decrypt:function(_30,_31,_32,_33){
this._totalCrypto=0;
this._finishedCrypto=0;
this._finishedSpawningCrypto=false;
this._finalResultSet=_30;
for(var i=0;i<_30.length;i++){
var row=_30[i];
for(var _34 in row){
if(_31=="*"||_31[_34]){
this._totalCrypto++;
var _35=row[_34];
this._decryptSingleColumn(_34,_35,_32,i,function(_36){
_33(_36);
});
}
}
}
this._finishedSpawningCrypto=true;
},_stripCryptoSQL:function(sql){
sql=sql.replace(/DECRYPT\(\*\)/ig,"*");
var _37=sql.match(/ENCRYPT\([^\)]*\)/ig);
if(_37!=null){
for(var i=0;i<_37.length;i++){
var _38=_37[i];
var _39=_38.match(/ENCRYPT\(([^\)]*)\)/i)[1];
sql=sql.replace(_38,_39);
}
}
_37=sql.match(/DECRYPT\([^\)]*\)/ig);
if(_37!=null){
for(i=0;i<_37.length;i++){
var _3a=_37[i];
var _3b=_3a.match(/DECRYPT\(([^\)]*)\)/i)[1];
sql=sql.replace(_3a,_3b);
}
}
return sql;
},_flagEncryptedArgs:function(sql,_3c){
var _3d=new RegExp(/([\"][^\"]*\?[^\"]*[\"])|([\'][^\']*\?[^\']*[\'])|(\?)/ig);
var _3e;
var _3f=0;
var _40=[];
while((_3e=_3d.exec(sql))!=null){
var _41=RegExp.lastMatch+"";
if(/^[\"\']/.test(_41)){
continue;
}
var _42=false;
if(/ENCRYPT\([^\)]*$/i.test(RegExp.leftContext)){
_42=true;
}
_40[_3f]=_42;
_3f++;
}
return _40;
},_determineDecryptedColumns:function(sql){
var _43={};
if(/DECRYPT\(\*\)/i.test(sql)){
_43="*";
}else{
var _44=/DECRYPT\((?:\s*\w*\s*\,?)*\)/ig;
var _45=_44.exec(sql);
while(_45){
var _46=new String(RegExp.lastMatch);
var _47=_46.replace(/DECRYPT\(/i,"");
_47=_47.replace(/\)/,"");
_47=_47.split(/\s*,\s*/);
_2.forEach(_47,function(_48){
if(/\s*\w* AS (\w*)/i.test(_48)){
_48=_48.match(/\s*\w* AS (\w*)/i)[1];
}
_43[_48]=true;
});
_45=_44.exec(sql);
}
}
return _43;
},_decryptSingleColumn:function(_49,_4a,_4b,_4c,_4d){
_3.sql._crypto.decrypt(_4a,_4b,_2.hitch(this,function(_4e){
this._finalResultSet[_4c][_49]=_4e;
this._finishedCrypto++;
if(this._finishedCrypto>=this._totalCrypto&&this._finishedSpawningCrypto){
_4d(this._finalResultSet);
}
}));
}});
(function(){
var _4f=_3.sql;
_3.sql=new Function("return dojox.sql._exec(arguments);");
_2.mixin(_3.sql,_4f);
})();
});
