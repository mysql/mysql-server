//>>built
define("dojox/data/FlickrRestStore",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/io/script","dojox/data/FlickrStore","dojo/_base/connect"],function(_1,_2,_3,_4,_5,_6){
var _7=_2("dojox.data.FlickrRestStore",_5,{constructor:function(_8){
if(_8){
if(_8.label){
this.label=_8.label;
}
if(_8.apikey){
this._apikey=_8.apikey;
}
}
this._cache=[];
this._prevRequests={};
this._handlers={};
this._prevRequestRanges=[];
this._maxPhotosPerUser={};
this._id=_7.prototype._id++;
},_id:0,_requestCount:0,_flickrRestUrl:"http://www.flickr.com/services/rest/",_apikey:null,_storeRef:"_S",_cache:null,_prevRequests:null,_handlers:null,_sortAttributes:{"date-posted":true,"date-taken":true,"interestingness":true},_fetchItems:function(_9,_a,_b){
var _c={};
if(!_9.query){
_9.query=_c={};
}else{
_1.mixin(_c,_9.query);
}
var _d=[];
var _e=[];
var _f={format:"json",method:"flickr.photos.search",api_key:this._apikey,extras:"owner_name,date_upload,date_taken"};
var _10=false;
if(_c.userid){
_10=true;
_f.user_id=_9.query.userid;
_d.push("userid"+_9.query.userid);
}
if(_c.groupid){
_10=true;
_f.group_id=_c.groupid;
_d.push("groupid"+_c.groupid);
}
if(_c.apikey){
_10=true;
_f.api_key=_9.query.apikey;
_e.push("api"+_9.query.apikey);
}else{
if(_f.api_key){
_10=true;
_9.query.apikey=_f.api_key;
_e.push("api"+_f.api_key);
}else{
throw Error("dojox.data.FlickrRestStore: An API key must be specified.");
}
}
_9._curCount=_9.count;
if(_c.page){
_f.page=_9.query.page;
_e.push("page"+_f.page);
}else{
if(("start" in _9)&&_9.start!==null){
if(!_9.count){
_9.count=20;
}
var _11=_9.start%_9.count;
var _12=_9.start,_13=_9.count;
if(_11!==0){
if(_12<_13/2){
_13=_12+_13;
_12=0;
}else{
var _14=20,div=2;
for(var i=_14;i>0;i--){
if(_12%i===0&&(_12/i)>=_13){
div=i;
break;
}
}
_13=_12/div;
}
_9._realStart=_9.start;
_9._realCount=_9.count;
_9._curStart=_12;
_9._curCount=_13;
}else{
_9._realStart=_9._realCount=null;
_9._curStart=_9.start;
_9._curCount=_9.count;
}
_f.page=(_12/_13)+1;
_e.push("page"+_f.page);
}
}
if(_9._curCount){
_f.per_page=_9._curCount;
_e.push("count"+_9._curCount);
}
if(_c.lang){
_f.lang=_9.query.lang;
_d.push("lang"+_9.lang);
}
if(_c.setid){
_f.method="flickr.photosets.getPhotos";
_f.photoset_id=_9.query.setid;
_d.push("set"+_9.query.setid);
}
if(_c.tags){
if(_c.tags instanceof Array){
_f.tags=_c.tags.join(",");
}else{
_f.tags=_c.tags;
}
_d.push("tags"+_f.tags);
if(_c["tag_mode"]&&(_c.tag_mode.toLowerCase()==="any"||_c.tag_mode.toLowerCase()==="all")){
_f.tag_mode=_c.tag_mode;
}
}
if(_c.text){
_f.text=_c.text;
_d.push("text:"+_c.text);
}
if(_c.sort&&_c.sort.length>0){
if(!_c.sort[0].attribute){
_c.sort[0].attribute="date-posted";
}
if(this._sortAttributes[_c.sort[0].attribute]){
if(_c.sort[0].descending){
_f.sort=_c.sort[0].attribute+"-desc";
}else{
_f.sort=_c.sort[0].attribute+"-asc";
}
}
}else{
_f.sort="date-posted-asc";
}
_d.push("sort:"+_f.sort);
_d=_d.join(".");
_e=_e.length>0?"."+_e.join("."):"";
var _15=_d+_e;
_9={query:_c,count:_9._curCount,start:_9._curStart,_realCount:_9._realCount,_realStart:_9._realStart,onBegin:_9.onBegin,onComplete:_9.onComplete,onItem:_9.onItem};
var _16={request:_9,fetchHandler:_a,errorHandler:_b};
if(this._handlers[_15]){
this._handlers[_15].push(_16);
return;
}
this._handlers[_15]=[_16];
var _17=null;
var _18={url:this._flickrRestUrl,preventCache:this.urlPreventCache,content:_f,callbackParamName:"jsoncallback"};
var _19=_1.hitch(this,function(_1a,_1b,_1c){
var _1d=_1c.request.onBegin;
_1c.request.onBegin=null;
var _1e;
var req=_1c.request;
if(("_realStart" in req)&&req._realStart!=null){
req.start=req._realStart;
req.count=req._realCount;
req._realStart=req._realCount=null;
}
if(_1d){
var _1f=null;
if(_1b){
_1f=(_1b.photoset?_1b.photoset:_1b.photos);
}
if(_1f&&("perpage" in _1f)&&("pages" in _1f)){
if(_1f.perpage*_1f.pages<=_1c.request.start+_1c.request.count){
_1e=_1c.request.start+_1f.photo.length;
}else{
_1e=_1f.perpage*_1f.pages;
}
this._maxPhotosPerUser[_d]=_1e;
_1d(_1e,_1c.request);
}else{
if(this._maxPhotosPerUser[_d]){
_1d(this._maxPhotosPerUser[_d],_1c.request);
}
}
}
_1c.fetchHandler(_1a,_1c.request);
if(_1d){
_1c.request.onBegin=_1d;
}
});
var _20=_1.hitch(this,function(_21){
if(_21.stat!="ok"){
_b(null,_9);
}else{
var _22=this._handlers[_15];
if(!_22){
return;
}
this._handlers[_15]=null;
this._prevRequests[_15]=_21;
var _23=this._processFlickrData(_21,_9,_d);
if(!this._prevRequestRanges[_d]){
this._prevRequestRanges[_d]=[];
}
this._prevRequestRanges[_d].push({start:_9.start,end:_9.start+(_21.photoset?_21.photoset.photo.length:_21.photos.photo.length)});
_3.forEach(_22,function(i){
_19(_23,_21,i);
});
}
});
var _24=this._prevRequests[_15];
if(_24){
this._handlers[_15]=null;
_19(this._cache[_d],_24,_16);
return;
}else{
if(this._checkPrevRanges(_d,_9.start,_9.count)){
this._handlers[_15]=null;
_19(this._cache[_d],null,_16);
return;
}
}
var _25=_4.get(_18);
_25.addCallback(_20);
_25.addErrback(function(_26){
_6.disconnect(_17);
_b(_26,_9);
});
},getAttributes:function(_27){
return ["title","author","imageUrl","imageUrlSmall","imageUrlMedium","imageUrlThumb","imageUrlLarge","imageUrlOriginal","link","dateTaken","datePublished"];
},getValues:function(_28,_29){
this._assertIsItem(_28);
this._assertIsAttribute(_29);
switch(_29){
case "title":
return [this._unescapeHtml(_28.title)];
case "author":
return [_28.ownername];
case "imageUrlSmall":
return [_28.media.s];
case "imageUrl":
return [_28.media.l];
case "imageUrlOriginal":
return [_28.media.o];
case "imageUrlLarge":
return [_28.media.l];
case "imageUrlMedium":
return [_28.media.m];
case "imageUrlThumb":
return [_28.media.t];
case "link":
return ["http://www.flickr.com/photos/"+_28.owner+"/"+_28.id];
case "dateTaken":
return [_28.datetaken];
case "datePublished":
return [_28.datepublished];
default:
return undefined;
}
},_processFlickrData:function(_2a,_2b,_2c){
if(_2a.items){
return _5.prototype._processFlickrData.apply(this,arguments);
}
var _2d=["http://farm",null,".static.flickr.com/",null,"/",null,"_",null];
var _2e=[];
var _2f=(_2a.photoset?_2a.photoset:_2a.photos);
if(_2a.stat=="ok"&&_2f&&_2f.photo){
_2e=_2f.photo;
for(var i=0;i<_2e.length;i++){
var _30=_2e[i];
_30[this._storeRef]=this;
_2d[1]=_30.farm;
_2d[3]=_30.server;
_2d[5]=_30.id;
_2d[7]=_30.secret;
var _31=_2d.join("");
_30.media={s:_31+"_s.jpg",m:_31+"_m.jpg",l:_31+".jpg",t:_31+"_t.jpg",o:_31+"_o.jpg"};
if(!_30.owner&&_2a.photoset){
_30.owner=_2a.photoset.owner;
}
}
}
var _32=_2b.start?_2b.start:0;
var arr=this._cache[_2c];
if(!arr){
this._cache[_2c]=arr=[];
}
_3.forEach(_2e,function(i,idx){
arr[idx+_32]=i;
});
return arr;
},_checkPrevRanges:function(_33,_34,_35){
var end=_34+_35;
var arr=this._prevRequestRanges[_33];
return (!!arr)&&_3.some(arr,function(_36){
return ((_34>=_36.start)&&(end<=_36.end));
});
}});
return _7;
});
