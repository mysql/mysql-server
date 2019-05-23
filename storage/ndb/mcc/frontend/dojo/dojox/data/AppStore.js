//>>built
define("dojox/data/AppStore",["dojo","dojox","dojo/data/util/simpleFetch","dojo/data/util/filter","dojox/atom/io/Connection"],function(_1,_2){
_1.experimental("dojox.data.AppStore");
_1.declare("dojox.data.AppStore",null,{url:"",urlPreventCache:false,xmethod:false,_atomIO:null,_feed:null,_requests:null,_processing:null,_updates:null,_adds:null,_deletes:null,constructor:function(_3){
if(_3&&_3.url){
this.url=_3.url;
}
if(_3&&_3.urlPreventCache){
this.urlPreventCache=_3.urlPreventCache;
}
if(!this.url){
throw new Error("A URL is required to instantiate an APP Store object");
}
},_setFeed:function(_4,_5){
this._feed=_4;
var i;
for(i=0;i<this._feed.entries.length;i++){
this._feed.entries[i].store=this;
}
if(this._requests){
for(i=0;i<this._requests.length;i++){
var _6=this._requests[i];
if(_6.request&&_6.fh&&_6.eh){
this._finishFetchItems(_6.request,_6.fh,_6.eh);
}else{
if(_6.clear){
this._feed=null;
}else{
if(_6.add){
this._feed.addEntry(_6.add);
}else{
if(_6.remove){
this._feed.removeEntry(_6.remove);
}
}
}
}
}
}
this._requests=null;
},_getAllItems:function(){
var _7=[];
for(var i=0;i<this._feed.entries.length;i++){
_7.push(this._feed.entries[i]);
}
return _7;
},_assertIsItem:function(_8){
if(!this.isItem(_8)){
throw new Error("This error message is provided when a function is called in the following form: "+"getAttribute(argument, attributeName).  The argument variable represents the member "+"or owner of the object. The error is created when an item that does not belong "+"to this store is specified as an argument.");
}
},_assertIsAttribute:function(_9){
if(typeof _9!=="string"){
throw new Error("The attribute argument must be a string. The error is created "+"when a different type of variable is specified such as an array or object.");
}
for(var _a in _2.atom.io.model._actions){
if(_a==_9){
return true;
}
}
return false;
},_addUpdate:function(_b){
if(!this._updates){
this._updates=[_b];
}else{
this._updates.push(_b);
}
},getValue:function(_c,_d,_e){
var _f=this.getValues(_c,_d);
return (_f.length>0)?_f[0]:_e;
},getValues:function(_10,_11){
this._assertIsItem(_10);
var _12=this._assertIsAttribute(_11);
if(_12){
if((_11==="author"||_11==="contributor"||_11==="link")&&_10[_11+"s"]){
return _10[_11+"s"];
}
if(_11==="category"&&_10.categories){
return _10.categories;
}
if(_10[_11]){
_10=_10[_11];
if(_10.nodeType=="Content"){
return [_10.value];
}
return [_10];
}
}
return [];
},getAttributes:function(_13){
this._assertIsItem(_13);
var _14=[];
for(var key in _2.atom.io.model._actions){
if(this.hasAttribute(_13,key)){
_14.push(key);
}
}
return _14;
},hasAttribute:function(_15,_16){
return this.getValues(_15,_16).length>0;
},containsValue:function(_17,_18,_19){
var _1a=undefined;
if(typeof _19==="string"){
_1a=_1.data.util.filter.patternToRegExp(_19,false);
}
return this._containsValue(_17,_18,_19,_1a);
},_containsValue:function(_1b,_1c,_1d,_1e,_1f){
var _20=this.getValues(_1b,_1c);
for(var i=0;i<_20.length;++i){
var _21=_20[i];
if(typeof _21==="string"&&_1e){
if(_1f){
_21=_21.replace(new RegExp(/^\s+/),"");
_21=_21.replace(new RegExp(/\s+$/),"");
}
_21=_21.replace(/\r|\n|\r\n/g,"");
return (_21.match(_1e)!==null);
}else{
if(_1d===_21){
return true;
}
}
}
return false;
},isItem:function(_22){
return _22&&_22.store&&_22.store===this;
},isItemLoaded:function(_23){
return this.isItem(_23);
},loadItem:function(_24){
this._assertIsItem(_24.item);
},_fetchItems:function(_25,_26,_27){
if(this._feed){
this._finishFetchItems(_25,_26,_27);
}else{
var _28=false;
if(!this._requests){
this._requests=[];
_28=true;
}
this._requests.push({request:_25,fh:_26,eh:_27});
if(_28){
this._atomIO=new _2.atom.io.Connection(false,this.urlPreventCache);
this._atomIO.getFeed(this.url,this._setFeed,null,this);
}
}
},_finishFetchItems:function(_29,_2a,_2b){
var _2c=null;
var _2d=this._getAllItems();
if(_29.query){
var _2e=_29.queryOptions?_29.queryOptions.ignoreCase:false;
_2c=[];
var _2f={};
var key;
var _30;
for(key in _29.query){
_30=_29.query[key]+"";
if(typeof _30==="string"){
_2f[key]=_1.data.util.filter.patternToRegExp(_30,_2e);
}
}
for(var i=0;i<_2d.length;++i){
var _31=true;
var _32=_2d[i];
for(key in _29.query){
_30=_29.query[key]+"";
if(!this._containsValue(_32,key,_30,_2f[key],_29.trim)){
_31=false;
}
}
if(_31){
_2c.push(_32);
}
}
}else{
if(_2d.length>0){
_2c=_2d.slice(0,_2d.length);
}
}
try{
_2a(_2c,_29);
}
catch(e){
_2b(e,_29);
}
},getFeatures:function(){
return {"dojo.data.api.Read":true,"dojo.data.api.Write":true,"dojo.data.api.Identity":true};
},close:function(_33){
this._feed=null;
},getLabel:function(_34){
if(this.isItem(_34)){
return this.getValue(_34,"title","No Title");
}
return undefined;
},getLabelAttributes:function(_35){
return ["title"];
},getIdentity:function(_36){
this._assertIsItem(_36);
return this.getValue(_36,"id");
},getIdentityAttributes:function(_37){
return ["id"];
},fetchItemByIdentity:function(_38){
this._fetchItems({query:{id:_38.identity},onItem:_38.onItem,scope:_38.scope},function(_39,_3a){
var _3b=_3a.scope;
if(!_3b){
_3b=_1.global;
}
if(_39.length<1){
_3a.onItem.call(_3b,null);
}else{
_3a.onItem.call(_3b,_39[0]);
}
},_38.onError);
},newItem:function(_3c){
var _3d=new _2.atom.io.model.Entry();
var _3e=null;
var _3f=null;
var i;
for(var key in _3c){
if(this._assertIsAttribute(key)){
_3e=_3c[key];
switch(key){
case "link":
for(i in _3e){
_3f=_3e[i];
_3d.addLink(_3f.href,_3f.rel,_3f.hrefLang,_3f.title,_3f.type);
}
break;
case "author":
for(i in _3e){
_3f=_3e[i];
_3d.addAuthor(_3f.name,_3f.email,_3f.uri);
}
break;
case "contributor":
for(i in _3e){
_3f=_3e[i];
_3d.addContributor(_3f.name,_3f.email,_3f.uri);
}
break;
case "category":
for(i in _3e){
_3f=_3e[i];
_3d.addCategory(_3f.scheme,_3f.term,_3f.label);
}
break;
case "icon":
case "id":
case "logo":
case "xmlBase":
case "rights":
_3d[key]=_3e;
break;
case "updated":
case "published":
case "issued":
case "modified":
_3d[key]=_2.atom.io.model.util.createDate(_3e);
break;
case "content":
case "summary":
case "title":
case "subtitle":
_3d[key]=new _2.atom.io.model.Content(key);
_3d[key].value=_3e;
break;
default:
_3d[key]=_3e;
break;
}
}
}
_3d.store=this;
_3d.isDirty=true;
if(!this._adds){
this._adds=[_3d];
}else{
this._adds.push(_3d);
}
if(this._feed){
this._feed.addEntry(_3d);
}else{
if(this._requests){
this._requests.push({add:_3d});
}else{
this._requests=[{add:_3d}];
this._atomIO=new _2.atom.io.Connection(false,this.urlPreventCache);
this._atomIO.getFeed(this.url,_1.hitch(this,this._setFeed));
}
}
return true;
},deleteItem:function(_40){
this._assertIsItem(_40);
if(!this._deletes){
this._deletes=[_40];
}else{
this._deletes.push(_40);
}
if(this._feed){
this._feed.removeEntry(_40);
}else{
if(this._requests){
this._requests.push({remove:_40});
}else{
this._requests=[{remove:_40}];
this._atomIO=new _2.atom.io.Connection(false,this.urlPreventCache);
this._atomIO.getFeed(this.url,_1.hitch(this,this._setFeed));
}
}
_40=null;
return true;
},setValue:function(_41,_42,_43){
this._assertIsItem(_41);
var _44={item:_41};
if(this._assertIsAttribute(_42)){
switch(_42){
case "link":
_44.links=_41.links;
this._addUpdate(_44);
_41.links=null;
_41.addLink(_43.href,_43.rel,_43.hrefLang,_43.title,_43.type);
_41.isDirty=true;
return true;
case "author":
_44.authors=_41.authors;
this._addUpdate(_44);
_41.authors=null;
_41.addAuthor(_43.name,_43.email,_43.uri);
_41.isDirty=true;
return true;
case "contributor":
_44.contributors=_41.contributors;
this._addUpdate(_44);
_41.contributors=null;
_41.addContributor(_43.name,_43.email,_43.uri);
_41.isDirty=true;
return true;
case "category":
_44.categories=_41.categories;
this._addUpdate(_44);
_41.categories=null;
_41.addCategory(_43.scheme,_43.term,_43.label);
_41.isDirty=true;
return true;
case "icon":
case "id":
case "logo":
case "xmlBase":
case "rights":
_44[_42]=_41[_42];
this._addUpdate(_44);
_41[_42]=_43;
_41.isDirty=true;
return true;
case "updated":
case "published":
case "issued":
case "modified":
_44[_42]=_41[_42];
this._addUpdate(_44);
_41[_42]=_2.atom.io.model.util.createDate(_43);
_41.isDirty=true;
return true;
case "content":
case "summary":
case "title":
case "subtitle":
_44[_42]=_41[_42];
this._addUpdate(_44);
_41[_42]=new _2.atom.io.model.Content(_42);
_41[_42].value=_43;
_41.isDirty=true;
return true;
default:
_44[_42]=_41[_42];
this._addUpdate(_44);
_41[_42]=_43;
_41.isDirty=true;
return true;
}
}
return false;
},setValues:function(_45,_46,_47){
if(_47.length===0){
return this.unsetAttribute(_45,_46);
}
this._assertIsItem(_45);
var _48={item:_45};
var _49;
var i;
if(this._assertIsAttribute(_46)){
switch(_46){
case "link":
_48.links=_45.links;
_45.links=null;
for(i in _47){
_49=_47[i];
_45.addLink(_49.href,_49.rel,_49.hrefLang,_49.title,_49.type);
}
_45.isDirty=true;
return true;
case "author":
_48.authors=_45.authors;
_45.authors=null;
for(i in _47){
_49=_47[i];
_45.addAuthor(_49.name,_49.email,_49.uri);
}
_45.isDirty=true;
return true;
case "contributor":
_48.contributors=_45.contributors;
_45.contributors=null;
for(i in _47){
_49=_47[i];
_45.addContributor(_49.name,_49.email,_49.uri);
}
_45.isDirty=true;
return true;
case "categories":
_48.categories=_45.categories;
_45.categories=null;
for(i in _47){
_49=_47[i];
_45.addCategory(_49.scheme,_49.term,_49.label);
}
_45.isDirty=true;
return true;
case "icon":
case "id":
case "logo":
case "xmlBase":
case "rights":
_48[_46]=_45[_46];
_45[_46]=_47[0];
_45.isDirty=true;
return true;
case "updated":
case "published":
case "issued":
case "modified":
_48[_46]=_45[_46];
_45[_46]=_2.atom.io.model.util.createDate(_47[0]);
_45.isDirty=true;
return true;
case "content":
case "summary":
case "title":
case "subtitle":
_48[_46]=_45[_46];
_45[_46]=new _2.atom.io.model.Content(_46);
_45[_46].values[0]=_47[0];
_45.isDirty=true;
return true;
default:
_48[_46]=_45[_46];
_45[_46]=_47[0];
_45.isDirty=true;
return true;
}
}
this._addUpdate(_48);
return false;
},unsetAttribute:function(_4a,_4b){
this._assertIsItem(_4a);
if(this._assertIsAttribute(_4b)){
if(_4a[_4b]!==null){
var _4c={item:_4a};
switch(_4b){
case "author":
case "contributor":
case "link":
_4c[_4b+"s"]=_4a[_4b+"s"];
break;
case "category":
_4c.categories=_4a.categories;
break;
default:
_4c[_4b]=_4a[_4b];
break;
}
_4a.isDirty=true;
_4a[_4b]=null;
this._addUpdate(_4c);
return true;
}
}
return false;
},save:function(_4d){
var i;
for(i in this._adds){
this._atomIO.addEntry(this._adds[i],null,function(){
},_4d.onError,false,_4d.scope);
}
this._adds=null;
for(i in this._updates){
this._atomIO.updateEntry(this._updates[i].item,function(){
},_4d.onError,false,this.xmethod,_4d.scope);
}
this._updates=null;
for(i in this._deletes){
this._atomIO.removeEntry(this._deletes[i],function(){
},_4d.onError,this.xmethod,_4d.scope);
}
this._deletes=null;
this._atomIO.getFeed(this.url,_1.hitch(this,this._setFeed));
if(_4d.onComplete){
var _4e=_4d.scope||_1.global;
_4d.onComplete.call(_4e);
}
},revert:function(){
var i;
for(i in this._adds){
this._feed.removeEntry(this._adds[i]);
}
this._adds=null;
var _4f,_50,key;
for(i in this._updates){
_4f=this._updates[i];
_50=_4f.item;
for(key in _4f){
if(key!=="item"){
_50[key]=_4f[key];
}
}
}
this._updates=null;
for(i in this._deletes){
this._feed.addEntry(this._deletes[i]);
}
this._deletes=null;
return true;
},isDirty:function(_51){
if(_51){
this._assertIsItem(_51);
return _51.isDirty?true:false;
}
return (this._adds!==null||this._updates!==null);
}});
_1.extend(_2.data.AppStore,_1.data.util.simpleFetch);
return _2.data.AppStore;
});
