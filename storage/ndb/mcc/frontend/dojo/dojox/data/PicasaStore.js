//>>built
define("dojox/data/PicasaStore",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojo/io/script","dojo/data/util/simpleFetch","dojo/date/stamp"],function(_1,_2,_3,_4,_5,_6){
var _7=_2("dojox.data.PicasaStore",null,{constructor:function(_8){
if(_8&&_8.label){
this.label=_8.label;
}
if(_8&&"urlPreventCache" in _8){
this.urlPreventCache=_8.urlPreventCache?true:false;
}
if(_8&&"maxResults" in _8){
this.maxResults=parseInt(_8.maxResults);
if(!this.maxResults){
this.maxResults=20;
}
}
},_picasaUrl:"http://picasaweb.google.com/data/feed/api/all",_storeRef:"_S",label:"title",urlPreventCache:false,maxResults:20,_assertIsItem:function(_9){
if(!this.isItem(_9)){
throw new Error("dojox.data.PicasaStore: a function was passed an item argument that was not an item");
}
},_assertIsAttribute:function(_a){
if(typeof _a!=="string"){
throw new Error("dojox.data.PicasaStore: a function was passed an attribute argument that was not an attribute name string");
}
},getFeatures:function(){
return {"dojo.data.api.Read":true};
},getValue:function(_b,_c,_d){
var _e=this.getValues(_b,_c);
if(_e&&_e.length>0){
return _e[0];
}
return _d;
},getAttributes:function(_f){
return ["id","published","updated","category","title$type","title","summary$type","summary","rights$type","rights","link","author","gphoto$id","gphoto$name","location","imageUrlSmall","imageUrlMedium","imageUrl","datePublished","dateTaken","description"];
},hasAttribute:function(_10,_11){
if(this.getValue(_10,_11)){
return true;
}
return false;
},isItemLoaded:function(_12){
return this.isItem(_12);
},loadItem:function(_13){
},getLabel:function(_14){
return this.getValue(_14,this.label);
},getLabelAttributes:function(_15){
return [this.label];
},containsValue:function(_16,_17,_18){
var _19=this.getValues(_16,_17);
for(var i=0;i<_19.length;i++){
if(_19[i]===_18){
return true;
}
}
return false;
},getValues:function(_1a,_1b){
this._assertIsItem(_1a);
this._assertIsAttribute(_1b);
if(_1b==="title"){
return [this._unescapeHtml(_1a.title)];
}else{
if(_1b==="author"){
return [this._unescapeHtml(_1a.author[0].name)];
}else{
if(_1b==="datePublished"){
return [dateAtamp.fromISOString(_1a.published)];
}else{
if(_1b==="dateTaken"){
return [_6.fromISOString(_1a.published)];
}else{
if(_1b==="updated"){
return [_6.fromISOString(_1a.updated)];
}else{
if(_1b==="imageUrlSmall"){
return [_1a.media.thumbnail[1].url];
}else{
if(_1b==="imageUrl"){
return [_1a.content$src];
}else{
if(_1b==="imageUrlMedium"){
return [_1a.media.thumbnail[2].url];
}else{
if(_1b==="link"){
return [_1a.link[1]];
}else{
if(_1b==="tags"){
return _1a.tags.split(" ");
}else{
if(_1b==="description"){
return [this._unescapeHtml(_1a.summary)];
}
}
}
}
}
}
}
}
}
}
}
return [];
},isItem:function(_1c){
if(_1c&&_1c[this._storeRef]===this){
return true;
}
return false;
},close:function(_1d){
},_fetchItems:function(_1e,_1f,_20){
if(!_1e.query){
_1e.query={};
}
var _21={alt:"jsonm",pp:"1",psc:"G"};
_21["start-index"]="1";
if(_1e.query.start){
_21["start-index"]=_1e.query.start;
}
if(_1e.query.tags){
_21.q=_1e.query.tags;
}
if(_1e.query.userid){
_21.uname=_1e.query.userid;
}
if(_1e.query.userids){
_21.ids=_1e.query.userids;
}
if(_1e.query.lang){
_21.hl=_1e.query.lang;
}
_21["max-results"]=this.maxResults;
var _22=this;
var _23=null;
var _24=function(_25){
if(_23!==null){
_3.disconnect(_23);
}
_1f(_22._processPicasaData(_25),_1e);
};
var _26={url:this._picasaUrl,preventCache:this.urlPreventCache,content:_21,callbackParamName:"callback",handle:_24};
var _27=_4.get(_26);
_27.addErrback(function(_28){
_3.disconnect(_23);
_20(_28,_1e);
});
},_processPicasaData:function(_29){
var _2a=[];
if(_29.feed){
_2a=_29.feed.entry;
for(var i=0;i<_2a.length;i++){
var _2b=_2a[i];
_2b[this._storeRef]=this;
}
}
return _2a;
},_unescapeHtml:function(str){
if(str){
str=str.replace(/&amp;/gm,"&").replace(/&lt;/gm,"<").replace(/&gt;/gm,">").replace(/&quot;/gm,"\"");
str=str.replace(/&#39;/gm,"'");
}
return str;
}});
_1.extend(_7,_5);
return _7;
});
