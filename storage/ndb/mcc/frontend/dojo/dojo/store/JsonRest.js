/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/store/JsonRest",["../_base/xhr","../_base/lang","../json","../_base/declare","./util/QueryResults"],function(_1,_2,_3,_4,_5){
var _6=null;
return _4("dojo.store.JsonRest",_6,{constructor:function(_7){
this.headers={};
_4.safeMixin(this,_7);
},headers:{},target:"",idProperty:"id",ascendingPrefix:"+",descendingPrefix:"-",_getTarget:function(id){
var _8=this.target;
if(typeof id!="undefined"){
if((_8.charAt(_8.length-1)=="/")||(_8.charAt(_8.length-1)=="=")){
_8+=id;
}else{
_8+="/"+id;
}
}
return _8;
},get:function(id,_9){
_9=_9||{};
var _a=_2.mixin({Accept:this.accepts},this.headers,_9.headers||_9);
return _1("GET",{url:this._getTarget(id),handleAs:"json",headers:_a,timeout:_9&&_9.timeout});
},accepts:"application/javascript, application/json",getIdentity:function(_b){
return _b[this.idProperty];
},put:function(_c,_d){
_d=_d||{};
var id=("id" in _d)?_d.id:this.getIdentity(_c);
var _e=typeof id!="undefined";
return _1(_e&&!_d.incremental?"PUT":"POST",{url:this._getTarget(id),postData:_3.stringify(_c),handleAs:"json",headers:_2.mixin({"Content-Type":"application/json",Accept:this.accepts,"If-Match":_d.overwrite===true?"*":null,"If-None-Match":_d.overwrite===false?"*":null},this.headers,_d.headers),timeout:_d&&_d.timeout});
},add:function(_f,_10){
_10=_10||{};
_10.overwrite=false;
return this.put(_f,_10);
},remove:function(id,_11){
_11=_11||{};
return _1("DELETE",{url:this._getTarget(id),headers:_2.mixin({},this.headers,_11.headers),timeout:_11&&_11.timeout});
},query:function(_12,_13){
_13=_13||{};
var _14=_2.mixin({Accept:this.accepts},this.headers,_13.headers);
var _15=this.target.indexOf("?")>-1;
_12=_12||"";
if(_12&&typeof _12=="object"){
_12=_1.objectToQuery(_12);
_12=_12?(_15?"&":"?")+_12:"";
}
if(_13.start>=0||_13.count>=0){
_14["X-Range"]="items="+(_13.start||"0")+"-"+(("count" in _13&&_13.count!=Infinity)?(_13.count+(_13.start||0)-1):"");
if(this.rangeParam){
_12+=(_12||_15?"&":"?")+this.rangeParam+"="+_14["X-Range"];
_15=true;
}else{
_14.Range=_14["X-Range"];
}
}
if(_13&&_13.sort){
var _16=this.sortParam;
_12+=(_12||_15?"&":"?")+(_16?_16+"=":"sort(");
for(var i=0;i<_13.sort.length;i++){
var _17=_13.sort[i];
_12+=(i>0?",":"")+(_17.descending?this.descendingPrefix:this.ascendingPrefix)+encodeURIComponent(_17.attribute);
}
if(!_16){
_12+=")";
}
}
var _18=_1("GET",{url:this.target+(_12||""),handleAs:"json",headers:_14,timeout:_13&&_13.timeout});
_18.total=_18.then(function(){
var _19=_18.ioArgs.xhr.getResponseHeader("Content-Range");
if(!_19){
_19=_18.ioArgs.xhr.getResponseHeader("X-Content-Range");
}
return _19&&(_19=_19.match(/\/(.*)/))&&+_19[1];
});
return _5(_18);
}});
});
