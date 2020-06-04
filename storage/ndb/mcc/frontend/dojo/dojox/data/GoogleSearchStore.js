//>>built
define("dojox/data/GoogleSearchStore",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/query","dojo/dom-construct","dojo/io/script"],function(_1,_2,_3,_4,_5,_6){
_1.experimental("dojox.data.GoogleSearchStore");
var _7=_3("dojox.data.GoogleSearchStore",null,{constructor:function(_8){
if(_8){
if(_8.label){
this.label=_8.label;
}
if(_8.key){
this._key=_8.key;
}
if(_8.lang){
this._lang=_8.lang;
}
if("urlPreventCache" in _8){
this.urlPreventCache=_8.urlPreventCache?true:false;
}
}
this._id=dojox.data.GoogleSearchStore.prototype._id++;
},_id:0,_requestCount:0,_googleUrl:"http://ajax.googleapis.com/ajax/services/search/",_storeRef:"_S",_attributes:["unescapedUrl","url","visibleUrl","cacheUrl","title","titleNoFormatting","content","estimatedResultCount"],_aggregatedAttributes:{estimatedResultCount:"cursor.estimatedResultCount"},label:"titleNoFormatting",_type:"web",urlPreventCache:true,_queryAttrs:{text:"q"},_assertIsItem:function(_9){
if(!this.isItem(_9)){
throw new Error("dojox.data.GoogleSearchStore: a function was passed an item argument that was not an item");
}
},_assertIsAttribute:function(_a){
if(typeof _a!=="string"){
throw new Error("dojox.data.GoogleSearchStore: a function was passed an attribute argument that was not an attribute name string");
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
return this._attributes;
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
var val=_1a[_1b];
if(_2.isArray(val)){
return val;
}else{
if(val!==undefined){
return [val];
}else{
return [];
}
}
},isItem:function(_1c){
if(_1c&&_1c[this._storeRef]===this){
return true;
}
return false;
},close:function(_1d){
},_format:function(_1e,_1f){
return _1e;
},fetch:function(_20){
_20=_20||{};
var _21=_20.scope||_1.global;
if(!_20.query){
if(_20.onError){
_20.onError.call(_21,new Error(this.declaredClass+": A query must be specified."));
return;
}
}
var _22={};
for(var _23 in this._queryAttrs){
_22[_23]=_20.query[_23];
}
_20={query:_22,onComplete:_20.onComplete,onError:_20.onError,onItem:_20.onItem,onBegin:_20.onBegin,start:_20.start,count:_20.count};
var _24=8;
var _25="GoogleSearchStoreCallback_"+this._id+"_"+(++this._requestCount);
var _26=this._createContent(_22,_25,_20);
var _27;
if(typeof (_20.start)==="undefined"||_20.start===null){
_20.start=0;
}
if(!_20.count){
_20.count=_24;
}
_27={start:_20.start-_20.start%_24};
var _28=this;
var _29=this._googleUrl+this._type;
var _2a={url:_29,preventCache:this.urlPreventCache,content:_26};
var _2b=[];
var _2c=0;
var _2d=false;
var _2e=_20.start-1;
var _2f=0;
var _30=[];
function _31(req){
_2f++;
_2a.content.context=_2a.content.start=req.start;
var _32=_6.get(_2a);
_30.push(_32.ioArgs.id);
_32.addErrback(function(_33){
if(_20.onError){
_20.onError.call(_21,_33,_20);
}
});
};
var _34=function(_35,_36){
if(_30.length>0){
_4("#"+_30.splice(0,1)).forEach(_5.destroy);
}
if(_2d){
return;
}
var _37=_28._getItems(_36);
var _38=_36?_36["cursor"]:null;
if(_37){
for(var i=0;i<_37.length&&i+_35<_20.count+_20.start;i++){
_28._processItem(_37[i],_36);
_2b[i+_35]=_37[i];
}
_2c++;
if(_2c==1){
var _39=_38?_38.pages:null;
var _3a=_39?Number(_39[_39.length-1].start):0;
if(_20.onBegin){
var est=_38?_38.estimatedResultCount:_37.length;
var _3b=est?Math.min(est,_3a+_37.length):_3a+_37.length;
_20.onBegin.call(_21,_3b,_20);
}
var _3c=(_20.start-_20.start%_24)+_24;
var _3d=1;
while(_39){
if(!_39[_3d]||Number(_39[_3d].start)>=_20.start+_20.count){
break;
}
if(Number(_39[_3d].start)>=_3c){
_31({start:_39[_3d].start});
}
_3d++;
}
}
if(_20.onItem&&_2b[_2e+1]){
do{
_2e++;
_20.onItem.call(_21,_2b[_2e],_20);
}while(_2b[_2e+1]&&_2e<_20.start+_20.count);
}
if(_2c==_2f){
_2d=true;
_1.global[_25]=null;
if(_20.onItem){
_20.onComplete.call(_21,null,_20);
}else{
_2b=_2b.slice(_20.start,_20.start+_20.count);
_20.onComplete.call(_21,_2b,_20);
}
}
}
};
var _3e=[];
var _3f=_27.start-1;
_1.global[_25]=function(_40,_41,_42,_43){
try{
if(_42!=200){
if(_20.onError){
_20.onError.call(_21,new Error("Response from Google was: "+_42),_20);
}
_1.global[_25]=function(){
};
return;
}
if(_40==_3f+1){
_34(Number(_40),_41);
_3f+=_24;
if(_3e.length>0){
_3e.sort(_28._getSort());
while(_3e.length>0&&_3e[0].start==_3f+1){
_34(Number(_3e[0].start),_3e[0].data);
_3e.splice(0,1);
_3f+=_24;
}
}
}else{
_3e.push({start:_40,data:_41});
}
}
catch(e){
_20.onError.call(_21,e,_20);
}
};
_31(_27);
},_getSort:function(){
return function(a,b){
if(a.start<b.start){
return -1;
}
if(b.start<a.start){
return 1;
}
return 0;
};
},_processItem:function(_44,_45){
_44[this._storeRef]=this;
for(var _46 in this._aggregatedAttributes){
_44[_46]=_2.getObject(this._aggregatedAttributes[_46],false,_45);
}
},_getItems:function(_47){
return _47["results"]||_47;
},_createContent:function(_48,_49,_4a){
var _4b={v:"1.0",rsz:"large",callback:_49,key:this._key,hl:this._lang};
for(var _4c in this._queryAttrs){
_4b[this._queryAttrs[_4c]]=_48[_4c];
}
return _4b;
}});
var _4d=_3("dojox.data.GoogleWebSearchStore",_7,{});
var _4e=_3("dojox.data.GoogleBlogSearchStore",_7,{_type:"blogs",_attributes:["blogUrl","postUrl","title","titleNoFormatting","content","author","publishedDate"],_aggregatedAttributes:{}});
var _4f=_3("dojox.data.GoogleLocalSearchStore",_7,{_type:"local",_attributes:["title","titleNoFormatting","url","lat","lng","streetAddress","city","region","country","phoneNumbers","ddUrl","ddUrlToHere","ddUrlFromHere","staticMapUrl","viewport"],_aggregatedAttributes:{viewport:"viewport"},_queryAttrs:{text:"q",centerLatLong:"sll",searchSpan:"sspn"}});
var _50=_3("dojox.data.GoogleVideoSearchStore",_7,{_type:"video",_attributes:["title","titleNoFormatting","content","url","published","publisher","duration","tbWidth","tbHeight","tbUrl","playUrl"],_aggregatedAttributes:{}});
var _51=_3("dojox.data.GoogleNewsSearchStore",_7,{_type:"news",_attributes:["title","titleNoFormatting","content","url","unescapedUrl","publisher","clusterUrl","location","publishedDate","relatedStories"],_aggregatedAttributes:{}});
var _52=_3("dojox.data.GoogleBookSearchStore",_7,{_type:"books",_attributes:["title","titleNoFormatting","authors","url","unescapedUrl","bookId","pageCount","publishedYear"],_aggregatedAttributes:{}});
var _53=_3("dojox.data.GoogleImageSearchStore",_7,{_type:"images",_attributes:["title","titleNoFormatting","visibleUrl","url","unescapedUrl","originalContextUrl","width","height","tbWidth","tbHeight","tbUrl","content","contentNoFormatting"],_aggregatedAttributes:{}});
return {Search:_7,ImageSearch:_53,BookSearch:_52,NewsSearch:_51,VideoSearch:_50,LocalSearch:_4f,BlogSearch:_4e,WebSearch:_4d};
});
