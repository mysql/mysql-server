//>>built
define("dojox/data/WikipediaStore",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/io/script","dojo/io-query","dojox/rpc/Service","dojox/data/ServiceStore"],function(_1,_2,_3,_4,_5,_6,_7){
_1.experimental("dojox.data.WikipediaStore");
return _3("dojox.data.WikipediaStore",_7,{constructor:function(_8){
if(_8&&_8.service){
this.service=_8.service;
}else{
var _9=new _6(require.toUrl("dojox/rpc/SMDLibrary/wikipedia.smd"));
this.service=_9.query;
}
this.idAttribute=this.labelAttribute="title";
},fetch:function(_a){
var rq=_2.mixin({},_a.query);
if(rq&&(!rq.action||rq.action==="parse")){
rq.action="parse";
rq.page=rq.title;
delete rq.title;
}else{
if(rq.action==="query"){
rq.list="search";
rq.srwhat="text";
rq.srsearch=rq.text;
if(_a.start){
rq.sroffset=_a.start-1;
}
if(_a.count){
rq.srlimit=_a.count>=500?500:_a.count;
}
delete rq.text;
}
}
_a.query=rq;
return this.inherited(arguments);
},_processResults:function(_b,_c){
if(_b.parse){
_b.parse.title=_5.queryToObject(_c.ioArgs.url.split("?")[1]).page;
_b=[_b.parse];
}else{
if(_b.query&&_b.query.search){
_b=_b.query.search;
var _d=this;
for(var i in _b){
_b[i]._loadObject=function(_e){
_d.fetch({query:{action:"parse",title:this.title},onItem:_e});
delete this._loadObject;
};
}
}
}
return this.inherited(arguments);
}});
});
