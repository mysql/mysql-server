//>>built
define("dojox/data/CouchDBRestStore",["dojo","dojox","dojox/data/JsonRestStore"],function(_1,_2){
var _3=_1.declare("dojox.data.CouchDBRestStore",_2.data.JsonRestStore,{save:function(_4){
var _5=this.inherited(arguments);
var _6=this.service.servicePath;
for(var i=0;i<_5.length;i++){
(function(_7,_8){
_8.addCallback(function(_9){
if(_9){
_7.__id=_6+_9.id;
_7._rev=_9.rev;
}
return _9;
});
})(_5[i].content,_5[i].deferred);
}
},fetch:function(_a){
_a.query=_a.query||"_all_docs?";
if(_a.start){
_a.query=(_a.query?(_a.query+"&"):"")+"skip="+_a.start;
delete _a.start;
}
if(_a.count){
_a.query=(_a.query?(_a.query+"&"):"")+"limit="+_a.count;
delete _a.count;
}
return this.inherited(arguments);
},_processResults:function(_b){
var _c=_b.rows;
if(_c){
var _d=this.service.servicePath;
var _e=this;
for(var i=0;i<_c.length;i++){
var _f=_c[i].value;
_f.__id=_d+_c[i].id;
_f._id=_c[i].id;
_f._loadObject=_2.rpc.JsonRest._loader;
_c[i]=_f;
}
return {totalCount:_b.total_rows,items:_b.rows};
}else{
return {items:_b};
}
}});
_3.getStores=function(_10){
var dfd=_1.xhrGet({url:_10+"_all_dbs",handleAs:"json",sync:true});
var _11={};
dfd.addBoth(function(dbs){
for(var i=0;i<dbs.length;i++){
_11[dbs[i]]=new _2.data.CouchDBRestStore({target:_10+dbs[i],idAttribute:"_id"});
}
return _11;
});
return _11;
};
return _3;
});
