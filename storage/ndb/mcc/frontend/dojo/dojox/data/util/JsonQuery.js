//>>built
define("dojox/data/util/JsonQuery",["dojo","dojox"],function(_1,_2){
_1.declare("dojox.data.util.JsonQuery",null,{useFullIdInQueries:false,_toJsonQuery:function(_3,_4){
var _5=true;
var _6=this;
function _7(_8,_9){
var _a=_9.__id;
if(_a){
var _b={};
_b[_6.idAttribute]=_6.useFullIdInQueries?_9.__id:_9[_6.idAttribute];
_9=_b;
}
for(var i in _9){
var _c=_9[i];
var _d=_8+(/^[a-zA-Z_][\w_]*$/.test(i)?"."+i:"["+_1._escapeString(i)+"]");
if(_c&&typeof _c=="object"){
_7(_d,_c);
}else{
if(_c!="*"){
_e+=(_5?"":"&")+_d+((!_a&&typeof _c=="string"&&_3.queryOptions&&_3.queryOptions.ignoreCase)?"~":"=")+(_6.simplifiedQuery?encodeURIComponent(_c):_1.toJson(_c));
_5=false;
}
}
}
};
if(_3.query&&typeof _3.query=="object"){
var _e="[?(";
_7("@",_3.query);
if(!_5){
_e+=")]";
}else{
_e="";
}
_3.queryStr=_e.replace(/\\"|"/g,function(t){
return t=="\""?"'":t;
});
}else{
if(!_3.query||_3.query=="*"){
_3.query="";
}
}
var _f=_3.sort;
if(_f){
_3.queryStr=_3.queryStr||(typeof _3.query=="string"?_3.query:"");
_5=true;
for(i=0;i<_f.length;i++){
_3.queryStr+=(_5?"[":",")+(_f[i].descending?"\\":"/")+"@["+_1._escapeString(_f[i].attribute)+"]";
_5=false;
}
_3.queryStr+="]";
}
if(_4&&(_3.start||_3.count)){
_3.queryStr=(_3.queryStr||(typeof _3.query=="string"?_3.query:""))+"["+(_3.start||"")+":"+(_3.count?(_3.start||0)+_3.count:"")+"]";
}
if(typeof _3.queryStr=="string"){
_3.queryStr=_3.queryStr.replace(/\\"|"/g,function(t){
return t=="\""?"'":t;
});
return _3.queryStr;
}
return _3.query;
},jsonQueryPagination:true,fetch:function(_10){
this._toJsonQuery(_10,this.jsonQueryPagination);
return this.inherited(arguments);
},isUpdateable:function(){
return true;
},matchesQuery:function(_11,_12){
_12._jsonQuery=_12._jsonQuery||_2.json.query(this._toJsonQuery(_12));
return _12._jsonQuery([_11]).length;
},clientSideFetch:function(_13,_14){
_13._jsonQuery=_13._jsonQuery||_2.json.query(this._toJsonQuery(_13));
return this.clientSidePaging(_13,_13._jsonQuery(_14));
},querySuperSet:function(_15,_16){
if(!_15.query){
return _16.query;
}
return this.inherited(arguments);
}});
return _2.data.util.JsonQuery;
});
