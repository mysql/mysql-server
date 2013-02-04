//>>built
define("dojox/data/GoogleFeedStore",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojox/data/GoogleSearchStore"],function(_1,_2,_3,_4){
_1.experimental("dojox.data.GoogleFeedStore");
var _5=_4.Search;
return _3("dojox.data.GoogleFeedStore",_5,{_type:"",_googleUrl:"http://ajax.googleapis.com/ajax/services/feed/load",_attributes:["title","link","author","published","content","summary","categories"],_queryAttrs:{"url":"q"},getFeedValue:function(_6,_7){
var _8=this.getFeedValues(_6,_7);
if(_2.isArray(_8)){
return _8[0];
}
return _8;
},getFeedValues:function(_9,_a){
if(!this._feedMetaData){
return _a;
}
return this._feedMetaData[_9]||_a;
},_processItem:function(_b,_c){
this.inherited(arguments);
_b["summary"]=_b["contentSnippet"];
_b["published"]=_b["publishedDate"];
},_getItems:function(_d){
if(_d["feed"]){
this._feedMetaData={title:_d.feed.title,desc:_d.feed.description,url:_d.feed.link,author:_d.feed.author};
return _d.feed.entries;
}
return null;
},_createContent:function(_e,_f,_10){
var cb=this.inherited(arguments);
cb.num=(_10.count||10)+(_10.start||0);
return cb;
}});
});
