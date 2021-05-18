//>>built
define("dojox/data/RailsStore",["dojo","dojox","dojox/data/JsonRestStore"],function(_1,_2){
return _1.declare("dojox.data.RailsStore",_2.data.JsonRestStore,{constructor:function(){
},preamble:function(_3){
if(typeof _3.target=="string"&&!_3.service){
var _4=_3.target.replace(/\/$/g,"");
var _5=function(id,_6){
_6=_6||{};
var _7=_4;
var _8;
var _9;
if(_1.isObject(id)){
_9="";
_8="?"+_1.objectToQuery(id);
}else{
if(_6.queryStr&&_6.queryStr.indexOf("?")!=-1){
_9=_6.queryStr.replace(/\?.*/,"");
_8=_6.queryStr.replace(/[^?]*\?/g,"?");
}else{
if(_1.isString(_6.query)&&_6.query.indexOf("?")!=-1){
_9=_6.query.replace(/\?.*/,"");
_8=_6.query.replace(/[^?]*\?/g,"?");
}else{
_9=id?id.toString():"";
_8="";
}
}
}
if(_9.indexOf("=")!=-1){
_8=_9;
_9="";
}
if(_9){
_7=_7+"/"+_9+".json"+_8;
}else{
_7=_7+".json"+_8;
}
var _a=_2.rpc._sync;
_2.rpc._sync=false;
return {url:_7,handleAs:"json",contentType:"application/json",sync:_a,headers:{Accept:"application/json,application/javascript",Range:_6&&(_6.start>=0||_6.count>=0)?"items="+(_6.start||"0")+"-"+((_6.count&&(_6.count+(_6.start||0)-1))||""):undefined}};
};
_3.service=_2.rpc.Rest(this.target,true,null,_5);
}
},fetch:function(_b){
_b=_b||{};
function _c(_d){
function _e(){
if(_b.queryStr==null){
_b.queryStr="";
}
if(_1.isObject(_b.query)){
_b.queryStr="?"+_1.objectToQuery(_b.query);
}else{
if(_1.isString(_b.query)){
_b.queryStr=_b.query;
}
}
};
function _f(){
if(_b.queryStr.indexOf("?")==-1){
return "?";
}else{
return "&";
}
};
if(_b.queryStr==null){
_e();
}
_b.queryStr=_b.queryStr+_f()+_1.objectToQuery(_d);
};
if(_b.start||_b.count){
if((_b.start||0)%_b.count){
throw new Error("The start parameter must be a multiple of the count parameter");
}
_c({page:((_b.start||0)/_b.count)+1,per_page:_b.count});
}
if(_b.sort){
var _10={sortBy:[],sortDir:[]};
_1.forEach(_b.sort,function(_11){
_10.sortBy.push(_11.attribute);
_10.sortDir.push(!!_11.descending?"DESC":"ASC");
});
_c(_10);
delete _b.sort;
}
return this.inherited(arguments);
},_processResults:function(_12,_13){
var _14;
if((typeof this.rootAttribute=="undefined")&&_12[0]){
if(_12[0][this.idAttribute]){
this.rootAttribute=false;
}else{
for(var _15 in _12[0]){
if(_12[0][_15][this.idAttribute]){
this.rootAttribute=_15;
}
}
}
}
if(this.rootAttribute){
_14=_1.map(_12,function(_16){
return _16[this.rootAttribute];
},this);
}else{
_14=_12;
}
var _17=_12.length;
return {totalCount:_13.fullLength||(_13.request.count==_17?(_13.request.start||0)+_17*2:_17),items:_14};
}});
});
