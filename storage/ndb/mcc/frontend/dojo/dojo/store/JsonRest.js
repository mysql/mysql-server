/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/store/JsonRest",["../_base/xhr","../json","../_base/declare","./util/QueryResults"],function(_1,_2,_3,_4){
return _3("dojo.store.JsonRest",null,{constructor:function(_5){
_3.safeMixin(this,_5);
},target:"",idProperty:"id",get:function(id,_6){
var _7=_6||{};
_7.Accept=this.accepts;
return _1("GET",{url:this.target+id,handleAs:"json",headers:_7});
},accepts:"application/javascript, application/json",getIdentity:function(_8){
return _8[this.idProperty];
},put:function(_9,_a){
_a=_a||{};
var id=("id" in _a)?_a.id:this.getIdentity(_9);
var _b=typeof id!="undefined";
return _1(_b&&!_a.incremental?"PUT":"POST",{url:_b?this.target+id:this.target,postData:_2.stringify(_9),handleAs:"json",headers:{"Content-Type":"application/json",Accept:this.accepts,"If-Match":_a.overwrite===true?"*":null,"If-None-Match":_a.overwrite===false?"*":null}});
},add:function(_c,_d){
_d=_d||{};
_d.overwrite=false;
return this.put(_c,_d);
},remove:function(id){
return _1("DELETE",{url:this.target+id});
},query:function(_e,_f){
var _10={Accept:this.accepts};
_f=_f||{};
if(_f.start>=0||_f.count>=0){
_10.Range="items="+(_f.start||"0")+"-"+(("count" in _f&&_f.count!=Infinity)?(_f.count+(_f.start||0)-1):"");
}
if(_e&&typeof _e=="object"){
_e=_1.objectToQuery(_e);
_e=_e?"?"+_e:"";
}
if(_f&&_f.sort){
var _11=this.sortParam;
_e+=(_e?"&":"?")+(_11?_11+"=":"sort(");
for(var i=0;i<_f.sort.length;i++){
var _12=_f.sort[i];
_e+=(i>0?",":"")+(_12.descending?"-":"+")+encodeURIComponent(_12.attribute);
}
if(!_11){
_e+=")";
}
}
var _13=_1("GET",{url:this.target+(_e||""),handleAs:"json",headers:_10});
_13.total=_13.then(function(){
var _14=_13.ioArgs.xhr.getResponseHeader("Content-Range");
return _14&&(_14=_14.match(/\/(.*)/))&&+_14[1];
});
return _4(_13);
}});
});
