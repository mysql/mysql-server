//>>built
define("dojox/store/db/SQL",["dojo/_base/declare","dojo/Deferred","dojo/when","dojo/store/util/QueryResults","dojo/_base/lang","dojo/promise/all"],function(_1,_2,_3,_4,_5,_6){
var _7=/(.*)\*$/;
function _8(_9){
return _9&&_5.mixin(_9,JSON.parse(_9.__extra));
};
return _1([],{constructor:function(_a){
var _b=_a.dbConfig;
this.database=openDatabase(_a.dbName||"dojo-db","1.0","dojo-db",4*1024*1024);
var _c=this.indexPrefix=_a.indexPrefix||"idx_";
var _d=_a.table||_a.storeName;
this.table=(_a.table||_a.storeName).replace(/[^\w]/g,"_");
var _e=[];
this.indices=_b.stores[_d];
this.repeatingIndices={};
for(var _f in this.indices){
if(this.indices[_f].multiEntry){
this.repeatingIndices[_f]=true;
}
}
if(!_b.available){
for(var _d in _b.stores){
var _10=_b.stores[_d];
var _11=_d.replace(/[^\w]/g,"_");
var _12=_10[this.idProperty];
var _13=["__extra",this.idProperty+" "+((_12&&_12.autoIncrement)?"INTEGER PRIMARY KEY AUTOINCREMENT":"PRIMARY KEY")];
var _14=[this.idProperty];
for(var _f in _10){
if(_f!=this.idProperty){
_13.push(_f);
}
}
_e.push(this.executeSql("CREATE TABLE IF NOT EXISTS "+_11+" ("+_13.join(",")+")"));
for(var _f in _10){
if(_f!=this.idProperty){
if(_10[_f].multiEntry){
_14.push(_f);
var _15=_11+"_repeating_"+_f;
_e.push(this.executeSql("CREATE TABLE IF NOT EXISTS "+_15+" (id,value)"));
_e.push(this.executeSql("CREATE INDEX IF NOT EXISTS idx_"+_15+"_id ON "+_15+"(id)"));
_e.push(this.executeSql("CREATE INDEX IF NOT EXISTS idx_"+_15+"_value ON "+_15+"(value)"));
}else{
_e.push(this.executeSql("ALTER TABLE "+_11+" ADD "+_f).then(null,function(){
}));
if(_10[_f].indexed!==false){
_e.push(this.executeSql("CREATE INDEX IF NOT EXISTS "+_c+_11+"_"+_f+" ON "+_11+"("+_f+")"));
}
}
}
}
}
_b.available=_6(_e);
}
this.available=_b.available;
},idProperty:"id",selectColumns:["*"],get:function(id){
return _3(this.executeSql("SELECT "+this.selectColumns.join(",")+" FROM "+this.table+" WHERE "+this.idProperty+"=?",[id]),function(_16){
return _16.rows.length>0?_8(_16.rows.item(0)):undefined;
});
},getIdentity:function(_17){
return _17[this.idProperty];
},remove:function(id){
return this.executeSql("DELETE FROM "+this.table+" WHERE "+this.idProperty+"=?",[id]);
},identifyGeneratedKey:true,add:function(_18,_19){
var _1a=[],_1b=[],_1c=[];
var _1d={};
var _1e=[];
var _1f=this;
for(var i in _18){
if(_18.hasOwnProperty(i)){
if(i in this.indices||i==this.idProperty){
if(this.repeatingIndices[i]){
_1e.push(function(id){
var _20=_18[i];
return _6(_20.map(function(_21){
return _1f.executeSql("INSERT INTO "+_1f.table+"_repeating_"+i+" (value, id) VALUES (?, ?)",[_21,id]);
}));
});
}else{
_1c.push(i);
_1b.push("?");
_1a.push(_18[i]);
}
}else{
_1d[i]=_18[i];
}
}
}
_1c.push("__extra");
_1b.push("?");
_1a.push(JSON.stringify(_1d));
var _22=this.idProperty;
if(this.identifyGeneratedKey){
_1a.idColumn=_22;
}
var sql="INSERT INTO "+this.table+" ("+_1c.join(",")+") VALUES ("+_1b.join(",")+")";
return _3(this.executeSql(sql,_1a),function(_23){
var id=_23.insertId;
_18[_22]=id;
return _6(_1e.map(function(_24){
return _24(id);
})).then(function(){
return id;
});
});
},put:function(_25,_26){
_26=_26||{};
var id=_26.id||_25[this.idProperty];
var _27=_26.overwrite;
if(_27===undefined){
var _28=this;
return this.get(id).then(function(_29){
if((_26.overwrite=!!_29)){
_26.overwrite=true;
return _28.put(_25,_26);
}else{
return _28.add(_25,_26);
}
});
}
if(!_27){
return _28.add(_25,_26);
}
var sql="UPDATE "+this.table+" SET ";
var _2a=[];
var _2b=[];
var _2c={};
var _2d=[];
for(var i in _25){
if(_25.hasOwnProperty(i)){
if(i in this.indices||i==this.idProperty){
if(this.repeatingIndices[i]){
this.executeSql("DELETE FROM "+this.table+"_repeating_"+i+" WHERE id=?",[id]);
var _2e=_25[i];
for(var j=0;j<_2e.length;j++){
this.executeSql("INSERT INTO "+this.table+"_repeating_"+i+" (value, id) VALUES (?, ?)",[_2e[j],id]);
}
}else{
_2b.push(i+"=?");
_2a.push(_25[i]);
}
}else{
_2c[i]=_25[i];
}
}
}
_2b.push("__extra=?");
_2a.push(JSON.stringify(_2c));
sql+=_2b.join(",")+" WHERE "+this.idProperty+"=?";
_2a.push(_25[this.idProperty]);
return _3(this.executeSql(sql,_2a),function(_2f){
return id;
});
},query:function(_30,_31){
_31=_31||{};
var _32="FROM "+this.table;
var _33;
var _34;
var _35=this;
var _36=this.table;
var _37=[];
if(_30.forEach){
_33=_30.map(_38).join(") OR (");
if(_33){
_33="("+_33+")";
}
}else{
_33=_38(_30);
}
if(_33){
_33=" WHERE "+_33;
}
function _38(_39){
var _3a=[];
for(var i in _39){
var _3b=_39[i];
function _3c(_3d){
var _3e=_3d&&_3d.match&&_3d.match(_7);
if(_3e){
_37.push(_3e[1]+"%");
return " LIKE ?";
}
_37.push(_3d);
return "=?";
};
if(_3b){
if(_3b.contains){
var _3f=_35.table+"_repeating_"+i;
_3a.push(_3b.contains.map(function(_40){
return _35.idProperty+" IN (SELECT id FROM "+_3f+" WHERE "+"value"+_3c(_40)+")";
}).join(" AND "));
continue;
}else{
if(typeof _3b=="object"&&("from" in _3b||"to" in _3b)){
var _41=_3b.excludeFrom?">":">=";
var _42=_3b.excludeTo?"<":"<=";
if("from" in _3b){
_37.push(_3b.from);
if("to" in _3b){
_37.push(_3b.to);
_3a.push("("+_36+"."+i+_41+"? AND "+_36+"."+i+_42+"?)");
}else{
_3a.push(_36+"."+i+_41+"?");
}
}else{
_37.push(_3b.to);
_3a.push(_36+"."+i+_42+"?");
}
continue;
}
}
}
_3a.push(_36+"."+i+_3c(_3b));
}
return _3a.join(" AND ");
};
if(_31.sort){
_33+=" ORDER BY "+_31.sort.map(function(_43){
return _36+"."+_43.attribute+" "+(_43.descending?"desc":"asc");
});
}
var _44=_33;
if(_31.count){
_44+=" LIMIT "+_31.count;
}
if(_31.start){
_44+=" OFFSET "+_31.start;
}
var _45=_5.delegate(this.executeSql("SELECT * "+_32+_44,_37).then(function(_46){
var _47=[];
for(var i=0;i<_46.rows.length;i++){
_47.push(_8(_46.rows.item(i)));
}
return _47;
}));
var _35=this;
_45.total={then:function(_48,_49){
return _35.executeSql("SELECT COUNT(*) "+_32+_33,_37).then(function(_4a){
return _4a.rows.item(0)["COUNT(*)"];
}).then(_48,_49);
}};
return new _4(_45);
},executeSql:function(sql,_4b){
var _4c=new _2();
var _4d,_4e;
this.database.transaction(function(_4f){
_4f.executeSql(sql,_4b,function(_50,_51){
_4c.resolve(_4d=_51);
},function(_52,e){
_4c.reject(_4e=e);
});
});
if(_4d){
return _4d;
}
if(_4e){
throw _4e;
}
return _4c.promise;
}});
});
