//>>built
define("dojox/data/CouchDBRestStore",["dojo","dojox","dojox/data/JsonRestStore"],function(_1,_2){
_1.declare("dojox.data.CouchDBRestStore",_2.data.JsonRestStore,{save:function(_3){
var _4=this.inherited(arguments);
var _5=this.service.servicePath;
for(var i=0;i<_4.length;i++){
(function(_6,_7){
_7.addCallback(function(_8){
if(_8){
_6.__id=_5+_8.id;
_6._rev=_8.rev;
}
return _8;
});
})(_4[i].content,_4[i].deferred);
}
},fetch:function(_9){
_9.query=_9.query||"_all_docs?";
if(_9.start){
_9.query=(_9.query?(_9.query+"&"):"")+"skip="+_9.start;
delete _9.start;
}
if(_9.count){
_9.query=(_9.query?(_9.query+"&"):"")+"limit="+_9.count;
delete _9.count;
}
return this.inherited(arguments);
},_processResults:function(_a){
var _b=_a.rows;
if(_b){
var _c=this.service.servicePath;
var _d=this;
for(var i=0;i<_b.length;i++){
var _e=_b[i].value;
_e.__id=_c+_b[i].id;
_e._id=_b[i].id;
_e._loadObject=_2.rpc.JsonRest._loader;
_b[i]=_e;
}
return {totalCount:_a.total_rows,items:_a.rows};
}else{
return {items:_a};
}
}});
_2.data.CouchDBRestStore.getStores=function(_f){
var dfd=_1.xhrGet({url:_f+"_all_dbs",handleAs:"json",sync:true});
var _10={};
dfd.addBoth(function(dbs){
for(var i=0;i<dbs.length;i++){
_10[dbs[i]]=new _2.data.CouchDBRestStore({target:_f+dbs[i],idAttribute:"_id"});
}
return _10;
});
return _10;
};
return _2.data.CouchDBRestStore;
});
