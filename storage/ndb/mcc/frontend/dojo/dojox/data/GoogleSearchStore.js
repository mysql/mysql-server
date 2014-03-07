//>>built
define("dojox/data/GoogleSearchStore",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/window","dojo/_base/query","dojo/dom-construct","dojo/io/script"],function(_1,_2,_3,_4,_5,_6,_7){
_1.experimental("dojox.data.GoogleSearchStore");
var _8=_3("dojox.data.GoogleSearchStore",null,{constructor:function(_9){
if(_9){
if(_9.label){
this.label=_9.label;
}
if(_9.key){
this._key=_9.key;
}
if(_9.lang){
this._lang=_9.lang;
}
if("urlPreventCache" in _9){
this.urlPreventCache=_9.urlPreventCache?true:false;
}
}
this._id=dojox.data.GoogleSearchStore.prototype._id++;
},_id:0,_requestCount:0,_googleUrl:"http://ajax.googleapis.com/ajax/services/search/",_storeRef:"_S",_attributes:["unescapedUrl","url","visibleUrl","cacheUrl","title","titleNoFormatting","content","estimatedResultCount"],_aggregatedAttributes:{estimatedResultCount:"cursor.estimatedResultCount"},label:"titleNoFormatting",_type:"web",urlPreventCache:true,_queryAttrs:{text:"q"},_assertIsItem:function(_a){
if(!this.isItem(_a)){
throw new Error("dojox.data.GoogleSearchStore: a function was passed an item argument that was not an item");
}
},_assertIsAttribute:function(_b){
if(typeof _b!=="string"){
throw new Error("dojox.data.GoogleSearchStore: a function was passed an attribute argument that was not an attribute name string");
}
},getFeatures:function(){
return {"dojo.data.api.Read":true};
},getValue:function(_c,_d,_e){
var _f=this.getValues(_c,_d);
if(_f&&_f.length>0){
return _f[0];
}
return _e;
},getAttributes:function(_10){
return this._attributes;
},hasAttribute:function(_11,_12){
if(this.getValue(_11,_12)){
return true;
}
return false;
},isItemLoaded:function(_13){
return this.isItem(_13);
},loadItem:function(_14){
},getLabel:function(_15){
return this.getValue(_15,this.label);
},getLabelAttributes:function(_16){
return [this.label];
},containsValue:function(_17,_18,_19){
var _1a=this.getValues(_17,_18);
for(var i=0;i<_1a.length;i++){
if(_1a[i]===_19){
return true;
}
}
return false;
},getValues:function(_1b,_1c){
this._assertIsItem(_1b);
this._assertIsAttribute(_1c);
var val=_1b[_1c];
if(_2.isArray(val)){
return val;
}else{
if(val!==undefined){
return [val];
}else{
return [];
}
}
},isItem:function(_1d){
if(_1d&&_1d[this._storeRef]===this){
return true;
}
return false;
},close:function(_1e){
},_format:function(_1f,_20){
return _1f;
},fetch:function(_21){
_21=_21||{};
var _22=_21.scope||_4.global;
if(!_21.query){
if(_21.onError){
_21.onError.call(_22,new Error(this.declaredClass+": A query must be specified."));
return;
}
}
var _23={};
for(var _24 in this._queryAttrs){
_23[_24]=_21.query[_24];
}
_21={query:_23,onComplete:_21.onComplete,onError:_21.onError,onItem:_21.onItem,onBegin:_21.onBegin,start:_21.start,count:_21.count};
var _25=8;
var _26="GoogleSearchStoreCallback_"+this._id+"_"+(++this._requestCount);
var _27=this._createContent(_23,_26,_21);
var _28;
if(typeof (_21.start)==="undefined"||_21.start===null){
_21.start=0;
}
if(!_21.count){
_21.count=_25;
}
_28={start:_21.start-_21.start%_25};
var _29=this;
var _2a=this._googleUrl+this._type;
var _2b={url:_2a,preventCache:this.urlPreventCache,content:_27};
var _2c=[];
var _2d=0;
var _2e=false;
var _2f=_21.start-1;
var _30=0;
var _31=[];
function _32(req){
_30++;
_2b.content.context=_2b.content.start=req.start;
var _33=_7.get(_2b);
_31.push(_33.ioArgs.id);
_33.addErrback(function(_34){
if(_21.onError){
_21.onError.call(_22,_34,_21);
}
});
};
var _35=function(_36,_37){
if(_31.length>0){
_5("#"+_31.splice(0,1)).forEach(_6.destroy);
}
if(_2e){
return;
}
var _38=_29._getItems(_37);
var _39=_37?_37["cursor"]:null;
if(_38){
for(var i=0;i<_38.length&&i+_36<_21.count+_21.start;i++){
_29._processItem(_38[i],_37);
_2c[i+_36]=_38[i];
}
_2d++;
if(_2d==1){
var _3a=_39?_39.pages:null;
var _3b=_3a?Number(_3a[_3a.length-1].start):0;
if(_21.onBegin){
var est=_39?_39.estimatedResultCount:_38.length;
var _3c=est?Math.min(est,_3b+_38.length):_3b+_38.length;
_21.onBegin.call(_22,_3c,_21);
}
var _3d=(_21.start-_21.start%_25)+_25;
var _3e=1;
while(_3a){
if(!_3a[_3e]||Number(_3a[_3e].start)>=_21.start+_21.count){
break;
}
if(Number(_3a[_3e].start)>=_3d){
_32({start:_3a[_3e].start});
}
_3e++;
}
}
if(_21.onItem&&_2c[_2f+1]){
do{
_2f++;
_21.onItem.call(_22,_2c[_2f],_21);
}while(_2c[_2f+1]&&_2f<_21.start+_21.count);
}
if(_2d==_30){
_2e=true;
_4.global[_26]=null;
if(_21.onItem){
_21.onComplete.call(_22,null,_21);
}else{
_2c=_2c.slice(_21.start,_21.start+_21.count);
_21.onComplete.call(_22,_2c,_21);
}
}
}
};
var _3f=[];
var _40=_28.start-1;
_4.global[_26]=function(_41,_42,_43,_44){
try{
if(_43!=200){
if(_21.onError){
_21.onError.call(_22,new Error("Response from Google was: "+_43),_21);
}
_4.global[_26]=function(){
};
return;
}
if(_41==_40+1){
_35(Number(_41),_42);
_40+=_25;
if(_3f.length>0){
_3f.sort(_29._getSort());
while(_3f.length>0&&_3f[0].start==_40+1){
_35(Number(_3f[0].start),_3f[0].data);
_3f.splice(0,1);
_40+=_25;
}
}
}else{
_3f.push({start:_41,data:_42});
}
}
catch(e){
_21.onError.call(_22,e,_21);
}
};
_32(_28);
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
},_processItem:function(_45,_46){
_45[this._storeRef]=this;
for(var _47 in this._aggregatedAttributes){
_45[_47]=_2.getObject(this._aggregatedAttributes[_47],false,_46);
}
},_getItems:function(_48){
return _48["results"]||_48;
},_createContent:function(_49,_4a,_4b){
var _4c={v:"1.0",rsz:"large",callback:_4a,key:this._key,hl:this._lang};
for(var _4d in this._queryAttrs){
_4c[this._queryAttrs[_4d]]=_49[_4d];
}
return _4c;
}});
var _4e=_3("dojox.data.GoogleWebSearchStore",_8,{});
var _4f=_3("dojox.data.GoogleBlogSearchStore",_8,{_type:"blogs",_attributes:["blogUrl","postUrl","title","titleNoFormatting","content","author","publishedDate"],_aggregatedAttributes:{}});
var _50=_3("dojox.data.GoogleLocalSearchStore",_8,{_type:"local",_attributes:["title","titleNoFormatting","url","lat","lng","streetAddress","city","region","country","phoneNumbers","ddUrl","ddUrlToHere","ddUrlFromHere","staticMapUrl","viewport"],_aggregatedAttributes:{viewport:"viewport"},_queryAttrs:{text:"q",centerLatLong:"sll",searchSpan:"sspn"}});
var _51=_3("dojox.data.GoogleVideoSearchStore",_8,{_type:"video",_attributes:["title","titleNoFormatting","content","url","published","publisher","duration","tbWidth","tbHeight","tbUrl","playUrl"],_aggregatedAttributes:{}});
var _52=_3("dojox.data.GoogleNewsSearchStore",_8,{_type:"news",_attributes:["title","titleNoFormatting","content","url","unescapedUrl","publisher","clusterUrl","location","publishedDate","relatedStories"],_aggregatedAttributes:{}});
var _53=_3("dojox.data.GoogleBookSearchStore",_8,{_type:"books",_attributes:["title","titleNoFormatting","authors","url","unescapedUrl","bookId","pageCount","publishedYear"],_aggregatedAttributes:{}});
var _54=_3("dojox.data.GoogleImageSearchStore",_8,{_type:"images",_attributes:["title","titleNoFormatting","visibleUrl","url","unescapedUrl","originalContextUrl","width","height","tbWidth","tbHeight","tbUrl","content","contentNoFormatting"],_aggregatedAttributes:{}});
return {Search:_8,ImageSearch:_54,BookSearch:_53,NewsSearch:_52,VideoSearch:_51,LocalSearch:_50,BlogSearch:_4f,WebSearch:_4e};
});
