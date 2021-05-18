//>>built
define("dojox/store/OData",["dojo/io-query","dojo/request","dojo/_base/lang","dojo/json","dojo/_base/declare","dojo/store/util/QueryResults"],function(_1,_2,_3,_4,_5,_6){
var _7=null;
return _5(_7,{headers:{"MaxDataServiceVersion":"2.0"},target:"",idProperty:"id",accepts:"application/json;odata=verbose",childAttr:"children",constructor:function(_8){
_5.safeMixin(this,_8);
},get:function(id,_9){
_9=_9||{};
var _a=_3.mixin({Accept:this.accepts},this.headers,_9.headers||_9);
return _2(this.target+"("+id+")",{handleAs:"json",headers:_a}).then(function(_b){
return _b.d;
});
},getIdentity:function(_c){
return _c[this.idProperty];
},put:function(_d,_e){
_e=_e||{};
var id=this.getIdentity(_d)||_e[this.idProperty];
var _f=id?(this.target+"("+id+")"):this.target;
var _10=_3.mixin({"Content-Type":"application/json;odata=verbose",Accept:this.accepts},this.headers,_e.headers);
if(id){
_10["X-HTTP-Method"]=_e.overwrite?"PUT":"MERGE";
_10["IF-MATCH"]=_e.overwrite?"*":(_e.etag||"*");
}
return _2.post(_f,{handleAs:"json",data:_4.stringify(_d),headers:_10});
},add:function(_11,_12){
_12=_12||{};
_12.overwrite=false;
return this.put(_11,_12);
},remove:function(id,_13){
_13=_13||{};
return _2.post(this.target+"("+id+")",{headers:_3.mixin({"IF-MATCH":"*","X-HTTP-Method":"DELETE"},this.headers,_13.headers)});
},getFormDigest:function(){
var i=this.target.indexOf("_vti_bin");
var url=this.target.slice(0,i)+"_api/contextinfo";
return _2.post(url).then(function(xml){
return xml.substring(xml.indexOf("<d:FormDigestValue>")+19,xml.indexOf("</d:FormDigestValue>"));
});
},getChildren:function(_14,_15){
var id=this.getIdentity(object)||_15[this.idProperty];
return this.query({"$filter":this.idProperty+" eq "+id,"$expand":this.childAttr},_15);
},query:function(_16,_17){
_17=_17||{};
var _18=_3.mixin({Accept:this.accepts},this.headers,_17.headers);
if(_17&&_17.sort){
_16["$orderby"]="";
var _19,i,len;
for(i=0,len=_17.sort.length;i<len;i++){
_19=_17.sort[i];
_16["$orderby"]+=(i>0?",":"")+encodeURIComponent(_19.attribute)+(_19.descending?" desc":" asc");
}
}
if(_17.start>=0||_17.count>=0){
_16["$skip"]=_17.start||0;
_16["$inlinecount"]="allpages";
if("count" in _17&&_17.count!=Infinity){
_16["$top"]=(_17.count);
}
}
_16=this.buildQueryString(_16);
var xhr=_2(this.target+(_16||""),{handleAs:"json",headers:_18});
var _1a=xhr.then(function(_1b){
return _1b.d.results;
});
_1a=_6(_1a);
_1a.total=xhr.then(function(_1c){
return _1c.d.__count;
});
return _1a;
},buildQueryString:function(_1d){
var _1e="";
for(var key in _1d){
if(_1d.hasOwnProperty(key)&&key.indexOf("$")==-1){
var _1f=_1d[key]+"";
var i=_1f.indexOf("*");
if(i!=-1){
_1f=_1f.slice(i!=0?0:1,_1f.length-(i!=0?1:0));
if(_1f.length>0){
_1e+=(_1e.length==0)?"":"and ";
_1e+=(i==0?"endswith":"startswith")+"("+key+",'"+_1f+"')";
}
}
}
}
if(_1e.length>0){
_1d["$filter"]=(_1d["$filter"]&&_1d["$filter"].length>0)?(_1d["$filter"]+" and "+_1e):_1e;
}
var _20=this.target.indexOf("?")>-1;
if(_1d&&typeof _1d=="object"){
_1d=_1.objectToQuery(_1d);
_1d=_1d?(_20?"&":"?")+_1d:"";
}
return _1d;
}});
});
