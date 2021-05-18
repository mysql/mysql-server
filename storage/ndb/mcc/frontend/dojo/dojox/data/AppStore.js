//>>built
define("dojox/data/AppStore",["dojo","dojox","dojo/data/util/simpleFetch","dojo/data/util/filter","dojox/atom/io/Connection"],function(_1,_2){
_1.experimental("dojox.data.AppStore");
var _3=_1.declare("dojox.data.AppStore",null,{url:"",urlPreventCache:false,xmethod:false,_atomIO:null,_feed:null,_requests:null,_processing:null,_updates:null,_adds:null,_deletes:null,constructor:function(_4){
if(_4&&_4.url){
this.url=_4.url;
}
if(_4&&_4.urlPreventCache){
this.urlPreventCache=_4.urlPreventCache;
}
if(!this.url){
throw new Error("A URL is required to instantiate an APP Store object");
}
},_setFeed:function(_5,_6){
this._feed=_5;
var i;
for(i=0;i<this._feed.entries.length;i++){
this._feed.entries[i].store=this;
}
if(this._requests){
for(i=0;i<this._requests.length;i++){
var _7=this._requests[i];
if(_7.request&&_7.fh&&_7.eh){
this._finishFetchItems(_7.request,_7.fh,_7.eh);
}else{
if(_7.clear){
this._feed=null;
}else{
if(_7.add){
this._feed.addEntry(_7.add);
}else{
if(_7.remove){
this._feed.removeEntry(_7.remove);
}
}
}
}
}
}
this._requests=null;
},_getAllItems:function(){
var _8=[];
for(var i=0;i<this._feed.entries.length;i++){
_8.push(this._feed.entries[i]);
}
return _8;
},_assertIsItem:function(_9){
if(!this.isItem(_9)){
throw new Error("This error message is provided when a function is called in the following form: "+"getAttribute(argument, attributeName).  The argument variable represents the member "+"or owner of the object. The error is created when an item that does not belong "+"to this store is specified as an argument.");
}
},_assertIsAttribute:function(_a){
if(typeof _a!=="string"){
throw new Error("The attribute argument must be a string. The error is created "+"when a different type of variable is specified such as an array or object.");
}
for(var _b in _2.atom.io.model._actions){
if(_b==_a){
return true;
}
}
return false;
},_addUpdate:function(_c){
if(!this._updates){
this._updates=[_c];
}else{
this._updates.push(_c);
}
},getValue:function(_d,_e,_f){
var _10=this.getValues(_d,_e);
return (_10.length>0)?_10[0]:_f;
},getValues:function(_11,_12){
this._assertIsItem(_11);
var _13=this._assertIsAttribute(_12);
if(_13){
if((_12==="author"||_12==="contributor"||_12==="link")&&_11[_12+"s"]){
return _11[_12+"s"];
}
if(_12==="category"&&_11.categories){
return _11.categories;
}
if(_11[_12]){
_11=_11[_12];
if(_11.nodeType=="Content"){
return [_11.value];
}
return [_11];
}
}
return [];
},getAttributes:function(_14){
this._assertIsItem(_14);
var _15=[];
for(var key in _2.atom.io.model._actions){
if(this.hasAttribute(_14,key)){
_15.push(key);
}
}
return _15;
},hasAttribute:function(_16,_17){
return this.getValues(_16,_17).length>0;
},containsValue:function(_18,_19,_1a){
var _1b=undefined;
if(typeof _1a==="string"){
_1b=_1.data.util.filter.patternToRegExp(_1a,false);
}
return this._containsValue(_18,_19,_1a,_1b);
},_containsValue:function(_1c,_1d,_1e,_1f,_20){
var _21=this.getValues(_1c,_1d);
for(var i=0;i<_21.length;++i){
var _22=_21[i];
if(typeof _22==="string"&&_1f){
if(_20){
_22=_22.replace(new RegExp(/^\s+/),"");
_22=_22.replace(new RegExp(/\s+$/),"");
}
_22=_22.replace(/\r|\n|\r\n/g,"");
return (_22.match(_1f)!==null);
}else{
if(_1e===_22){
return true;
}
}
}
return false;
},isItem:function(_23){
return _23&&_23.store&&_23.store===this;
},isItemLoaded:function(_24){
return this.isItem(_24);
},loadItem:function(_25){
this._assertIsItem(_25.item);
},_fetchItems:function(_26,_27,_28){
if(this._feed){
this._finishFetchItems(_26,_27,_28);
}else{
var _29=false;
if(!this._requests){
this._requests=[];
_29=true;
}
this._requests.push({request:_26,fh:_27,eh:_28});
if(_29){
this._atomIO=new _2.atom.io.Connection(false,this.urlPreventCache);
this._atomIO.getFeed(this.url,this._setFeed,null,this);
}
}
},_finishFetchItems:function(_2a,_2b,_2c){
var _2d=null;
var _2e=this._getAllItems();
if(_2a.query){
var _2f=_2a.queryOptions?_2a.queryOptions.ignoreCase:false;
_2d=[];
var _30={};
var key;
var _31;
for(key in _2a.query){
_31=_2a.query[key]+"";
if(typeof _31==="string"){
_30[key]=_1.data.util.filter.patternToRegExp(_31,_2f);
}
}
for(var i=0;i<_2e.length;++i){
var _32=true;
var _33=_2e[i];
for(key in _2a.query){
_31=_2a.query[key]+"";
if(!this._containsValue(_33,key,_31,_30[key],_2a.trim)){
_32=false;
}
}
if(_32){
_2d.push(_33);
}
}
}else{
if(_2e.length>0){
_2d=_2e.slice(0,_2e.length);
}
}
try{
_2b(_2d,_2a);
}
catch(e){
_2c(e,_2a);
}
},getFeatures:function(){
return {"dojo.data.api.Read":true,"dojo.data.api.Write":true,"dojo.data.api.Identity":true};
},close:function(_34){
this._feed=null;
},getLabel:function(_35){
if(this.isItem(_35)){
return this.getValue(_35,"title","No Title");
}
return undefined;
},getLabelAttributes:function(_36){
return ["title"];
},getIdentity:function(_37){
this._assertIsItem(_37);
return this.getValue(_37,"id");
},getIdentityAttributes:function(_38){
return ["id"];
},fetchItemByIdentity:function(_39){
this._fetchItems({query:{id:_39.identity},onItem:_39.onItem,scope:_39.scope},function(_3a,_3b){
var _3c=_3b.scope;
if(!_3c){
_3c=_1.global;
}
if(_3a.length<1){
_3b.onItem.call(_3c,null);
}else{
_3b.onItem.call(_3c,_3a[0]);
}
},_39.onError);
},newItem:function(_3d){
var _3e=new _2.atom.io.model.Entry();
var _3f=null;
var _40=null;
var i;
for(var key in _3d){
if(this._assertIsAttribute(key)){
_3f=_3d[key];
switch(key){
case "link":
for(i in _3f){
_40=_3f[i];
_3e.addLink(_40.href,_40.rel,_40.hrefLang,_40.title,_40.type);
}
break;
case "author":
for(i in _3f){
_40=_3f[i];
_3e.addAuthor(_40.name,_40.email,_40.uri);
}
break;
case "contributor":
for(i in _3f){
_40=_3f[i];
_3e.addContributor(_40.name,_40.email,_40.uri);
}
break;
case "category":
for(i in _3f){
_40=_3f[i];
_3e.addCategory(_40.scheme,_40.term,_40.label);
}
break;
case "icon":
case "id":
case "logo":
case "xmlBase":
case "rights":
_3e[key]=_3f;
break;
case "updated":
case "published":
case "issued":
case "modified":
_3e[key]=_2.atom.io.model.util.createDate(_3f);
break;
case "content":
case "summary":
case "title":
case "subtitle":
_3e[key]=new _2.atom.io.model.Content(key);
_3e[key].value=_3f;
break;
default:
_3e[key]=_3f;
break;
}
}
}
_3e.store=this;
_3e.isDirty=true;
if(!this._adds){
this._adds=[_3e];
}else{
this._adds.push(_3e);
}
if(this._feed){
this._feed.addEntry(_3e);
}else{
if(this._requests){
this._requests.push({add:_3e});
}else{
this._requests=[{add:_3e}];
this._atomIO=new _2.atom.io.Connection(false,this.urlPreventCache);
this._atomIO.getFeed(this.url,_1.hitch(this,this._setFeed));
}
}
return true;
},deleteItem:function(_41){
this._assertIsItem(_41);
if(!this._deletes){
this._deletes=[_41];
}else{
this._deletes.push(_41);
}
if(this._feed){
this._feed.removeEntry(_41);
}else{
if(this._requests){
this._requests.push({remove:_41});
}else{
this._requests=[{remove:_41}];
this._atomIO=new _2.atom.io.Connection(false,this.urlPreventCache);
this._atomIO.getFeed(this.url,_1.hitch(this,this._setFeed));
}
}
_41=null;
return true;
},setValue:function(_42,_43,_44){
this._assertIsItem(_42);
var _45={item:_42};
if(this._assertIsAttribute(_43)){
switch(_43){
case "link":
_45.links=_42.links;
this._addUpdate(_45);
_42.links=null;
_42.addLink(_44.href,_44.rel,_44.hrefLang,_44.title,_44.type);
_42.isDirty=true;
return true;
case "author":
_45.authors=_42.authors;
this._addUpdate(_45);
_42.authors=null;
_42.addAuthor(_44.name,_44.email,_44.uri);
_42.isDirty=true;
return true;
case "contributor":
_45.contributors=_42.contributors;
this._addUpdate(_45);
_42.contributors=null;
_42.addContributor(_44.name,_44.email,_44.uri);
_42.isDirty=true;
return true;
case "category":
_45.categories=_42.categories;
this._addUpdate(_45);
_42.categories=null;
_42.addCategory(_44.scheme,_44.term,_44.label);
_42.isDirty=true;
return true;
case "icon":
case "id":
case "logo":
case "xmlBase":
case "rights":
_45[_43]=_42[_43];
this._addUpdate(_45);
_42[_43]=_44;
_42.isDirty=true;
return true;
case "updated":
case "published":
case "issued":
case "modified":
_45[_43]=_42[_43];
this._addUpdate(_45);
_42[_43]=_2.atom.io.model.util.createDate(_44);
_42.isDirty=true;
return true;
case "content":
case "summary":
case "title":
case "subtitle":
_45[_43]=_42[_43];
this._addUpdate(_45);
_42[_43]=new _2.atom.io.model.Content(_43);
_42[_43].value=_44;
_42.isDirty=true;
return true;
default:
_45[_43]=_42[_43];
this._addUpdate(_45);
_42[_43]=_44;
_42.isDirty=true;
return true;
}
}
return false;
},setValues:function(_46,_47,_48){
if(_48.length===0){
return this.unsetAttribute(_46,_47);
}
this._assertIsItem(_46);
var _49={item:_46};
var _4a;
var i;
if(this._assertIsAttribute(_47)){
switch(_47){
case "link":
_49.links=_46.links;
_46.links=null;
for(i in _48){
_4a=_48[i];
_46.addLink(_4a.href,_4a.rel,_4a.hrefLang,_4a.title,_4a.type);
}
_46.isDirty=true;
return true;
case "author":
_49.authors=_46.authors;
_46.authors=null;
for(i in _48){
_4a=_48[i];
_46.addAuthor(_4a.name,_4a.email,_4a.uri);
}
_46.isDirty=true;
return true;
case "contributor":
_49.contributors=_46.contributors;
_46.contributors=null;
for(i in _48){
_4a=_48[i];
_46.addContributor(_4a.name,_4a.email,_4a.uri);
}
_46.isDirty=true;
return true;
case "categories":
_49.categories=_46.categories;
_46.categories=null;
for(i in _48){
_4a=_48[i];
_46.addCategory(_4a.scheme,_4a.term,_4a.label);
}
_46.isDirty=true;
return true;
case "icon":
case "id":
case "logo":
case "xmlBase":
case "rights":
_49[_47]=_46[_47];
_46[_47]=_48[0];
_46.isDirty=true;
return true;
case "updated":
case "published":
case "issued":
case "modified":
_49[_47]=_46[_47];
_46[_47]=_2.atom.io.model.util.createDate(_48[0]);
_46.isDirty=true;
return true;
case "content":
case "summary":
case "title":
case "subtitle":
_49[_47]=_46[_47];
_46[_47]=new _2.atom.io.model.Content(_47);
_46[_47].values[0]=_48[0];
_46.isDirty=true;
return true;
default:
_49[_47]=_46[_47];
_46[_47]=_48[0];
_46.isDirty=true;
return true;
}
}
this._addUpdate(_49);
return false;
},unsetAttribute:function(_4b,_4c){
this._assertIsItem(_4b);
if(this._assertIsAttribute(_4c)){
if(_4b[_4c]!==null){
var _4d={item:_4b};
switch(_4c){
case "author":
case "contributor":
case "link":
_4d[_4c+"s"]=_4b[_4c+"s"];
break;
case "category":
_4d.categories=_4b.categories;
break;
default:
_4d[_4c]=_4b[_4c];
break;
}
_4b.isDirty=true;
_4b[_4c]=null;
this._addUpdate(_4d);
return true;
}
}
return false;
},save:function(_4e){
var i;
for(i in this._adds){
this._atomIO.addEntry(this._adds[i],null,function(){
},_4e.onError,false,_4e.scope);
}
this._adds=null;
for(i in this._updates){
this._atomIO.updateEntry(this._updates[i].item,function(){
},_4e.onError,false,this.xmethod,_4e.scope);
}
this._updates=null;
for(i in this._deletes){
this._atomIO.removeEntry(this._deletes[i],function(){
},_4e.onError,this.xmethod,_4e.scope);
}
this._deletes=null;
this._atomIO.getFeed(this.url,_1.hitch(this,this._setFeed));
if(_4e.onComplete){
var _4f=_4e.scope||_1.global;
_4e.onComplete.call(_4f);
}
},revert:function(){
var i;
for(i in this._adds){
this._feed.removeEntry(this._adds[i]);
}
this._adds=null;
var _50,_51,key;
for(i in this._updates){
_50=this._updates[i];
_51=_50.item;
for(key in _50){
if(key!=="item"){
_51[key]=_50[key];
}
}
}
this._updates=null;
for(i in this._deletes){
this._feed.addEntry(this._deletes[i]);
}
this._deletes=null;
return true;
},isDirty:function(_52){
if(_52){
this._assertIsItem(_52);
return _52.isDirty?true:false;
}
return (this._adds!==null||this._updates!==null);
}});
_1.extend(_3,_1.data.util.simpleFetch);
return _3;
});
