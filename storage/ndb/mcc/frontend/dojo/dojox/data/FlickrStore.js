//>>built
define("dojox/data/FlickrStore",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/data/util/simpleFetch","dojo/io/script","dojo/_base/connect","dojo/date/stamp","dojo/AdapterRegistry"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_2("dojox.data.FlickrStore",null,{constructor:function(_a){
if(_a&&_a.label){
this.label=_a.label;
}
if(_a&&"urlPreventCache" in _a){
this.urlPreventCache=_a.urlPreventCache?true:false;
}
},_storeRef:"_S",label:"title",urlPreventCache:true,_assertIsItem:function(_b){
if(!this.isItem(_b)){
throw new Error("dojox.data.FlickrStore: a function was passed an item argument that was not an item");
}
},_assertIsAttribute:function(_c){
if(typeof _c!=="string"){
throw new Error("dojox.data.FlickrStore: a function was passed an attribute argument that was not an attribute name string");
}
},getFeatures:function(){
return {"dojo.data.api.Read":true};
},getValue:function(_d,_e,_f){
var _10=this.getValues(_d,_e);
if(_10&&_10.length>0){
return _10[0];
}
return _f;
},getAttributes:function(_11){
return ["title","description","author","datePublished","dateTaken","imageUrl","imageUrlSmall","imageUrlMedium","tags","link"];
},hasAttribute:function(_12,_13){
var v=this.getValue(_12,_13);
if(v||v===""||v===false){
return true;
}
return false;
},isItemLoaded:function(_14){
return this.isItem(_14);
},loadItem:function(_15){
},getLabel:function(_16){
return this.getValue(_16,this.label);
},getLabelAttributes:function(_17){
return [this.label];
},containsValue:function(_18,_19,_1a){
var _1b=this.getValues(_18,_19);
for(var i=0;i<_1b.length;i++){
if(_1b[i]===_1a){
return true;
}
}
return false;
},getValues:function(_1c,_1d){
this._assertIsItem(_1c);
this._assertIsAttribute(_1d);
var u=_1.hitch(this,"_unescapeHtml");
var s=_1.hitch(_7,"fromISOString");
switch(_1d){
case "title":
return [u(_1c.title)];
case "author":
return [u(_1c.author)];
case "datePublished":
return [s(_1c.published)];
case "dateTaken":
return [s(_1c.date_taken)];
case "imageUrlSmall":
return [_1c.media.m.replace(/_m\./,"_s.")];
case "imageUrl":
return [_1c.media.m.replace(/_m\./,".")];
case "imageUrlMedium":
return [_1c.media.m];
case "link":
return [_1c.link];
case "tags":
return _1c.tags.split(" ");
case "description":
return [u(_1c.description)];
default:
return [];
}
},isItem:function(_1e){
if(_1e&&_1e[this._storeRef]===this){
return true;
}
return false;
},close:function(_1f){
},_fetchItems:function(_20,_21,_22){
var rq=_20.query=_20.query||{};
var _23={format:"json",tagmode:"any"};
_3.forEach(["tags","tagmode","lang","id","ids"],function(i){
if(rq[i]){
_23[i]=rq[i];
}
});
_23.id=rq.id||rq.userid||rq.groupid;
if(rq.userids){
_23.ids=rq.userids;
}
var _24=null;
var _25={url:dojox.data.FlickrStore.urlRegistry.match(_20),preventCache:this.urlPreventCache,content:_23};
var _26=_1.hitch(this,function(_27){
if(!!_24){
_6.disconnect(_24);
}
_21(this._processFlickrData(_27),_20);
});
_24=_6.connect("jsonFlickrFeed",_26);
var _28=_5.get(_25);
_28.addErrback(function(_29){
_6.disconnect(_24);
_22(_29,_20);
});
},_processFlickrData:function(_2a){
var _2b=[];
if(_2a.items){
_2b=_2a.items;
for(var i=0;i<_2a.items.length;i++){
var _2c=_2a.items[i];
_2c[this._storeRef]=this;
}
}
return _2b;
},_unescapeHtml:function(str){
return str.replace(/&amp;/gm,"&").replace(/&lt;/gm,"<").replace(/&gt;/gm,">").replace(/&quot;/gm,"\"").replace(/&#39;/gm,"'");
}});
_1.extend(_9,_4);
var _2d="http://api.flickr.com/services/feeds/";
var reg=_9.urlRegistry=new _8(true);
reg.register("group pool",function(_2e){
return !!_2e.query["groupid"];
},_2d+"groups_pool.gne");
reg.register("default",function(_2f){
return true;
},_2d+"photos_public.gne");
if(!_30){
var _30=function(_31){
};
}
return _9;
});
