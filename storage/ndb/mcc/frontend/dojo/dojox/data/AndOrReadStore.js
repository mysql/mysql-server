//>>built
define("dojox/data/AndOrReadStore",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/data/util/filter","dojo/data/util/simpleFetch","dojo/_base/array","dojo/date/stamp","dojo/_base/json","dojo/_base/window","dojo/_base/xhr"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
var _b=_2("dojox.data.AndOrReadStore",null,{constructor:function(_c){
this._arrayOfAllItems=[];
this._arrayOfTopLevelItems=[];
this._loadFinished=false;
this._jsonFileUrl=_c.url;
this._ccUrl=_c.url;
this.url=_c.url;
this._jsonData=_c.data;
this.data=null;
this._datatypeMap=_c.typeMap||{};
if(!this._datatypeMap["Date"]){
this._datatypeMap["Date"]={type:Date,deserialize:function(_d){
return _7.fromISOString(_d);
}};
}
this._features={"dojo.data.api.Read":true,"dojo.data.api.Identity":true};
this._itemsByIdentity=null;
this._storeRefPropName="_S";
this._itemNumPropName="_0";
this._rootItemPropName="_RI";
this._reverseRefMap="_RRM";
this._loadInProgress=false;
this._queuedFetches=[];
if(_c.urlPreventCache!==undefined){
this.urlPreventCache=_c.urlPreventCache?true:false;
}
if(_c.hierarchical!==undefined){
this.hierarchical=_c.hierarchical?true:false;
}
if(_c.clearOnClose){
this.clearOnClose=true;
}
},url:"",_ccUrl:"",data:null,typeMap:null,clearOnClose:false,urlPreventCache:false,hierarchical:true,_assertIsItem:function(_e){
if(!this.isItem(_e)){
throw new Error("dojox.data.AndOrReadStore: Invalid item argument.");
}
},_assertIsAttribute:function(_f){
if(typeof _f!=="string"){
throw new Error("dojox.data.AndOrReadStore: Invalid attribute argument.");
}
},getValue:function(_10,_11,_12){
var _13=this.getValues(_10,_11);
return (_13.length>0)?_13[0]:_12;
},getValues:function(_14,_15){
this._assertIsItem(_14);
this._assertIsAttribute(_15);
var arr=_14[_15]||[];
return arr.slice(0,arr.length);
},getAttributes:function(_16){
this._assertIsItem(_16);
var _17=[];
for(var key in _16){
if((key!==this._storeRefPropName)&&(key!==this._itemNumPropName)&&(key!==this._rootItemPropName)&&(key!==this._reverseRefMap)){
_17.push(key);
}
}
return _17;
},hasAttribute:function(_18,_19){
this._assertIsItem(_18);
this._assertIsAttribute(_19);
return (_19 in _18);
},containsValue:function(_1a,_1b,_1c){
var _1d=undefined;
if(typeof _1c==="string"){
_1d=_4.patternToRegExp(_1c,false);
}
return this._containsValue(_1a,_1b,_1c,_1d);
},_containsValue:function(_1e,_1f,_20,_21){
return _6.some(this.getValues(_1e,_1f),function(_22){
if(_22!==null&&!_3.isObject(_22)&&_21){
if(_22.toString().match(_21)){
return true;
}
}else{
if(_20===_22){
return true;
}else{
return false;
}
}
});
},isItem:function(_23){
if(_23&&_23[this._storeRefPropName]===this){
if(this._arrayOfAllItems[_23[this._itemNumPropName]]===_23){
return true;
}
}
return false;
},isItemLoaded:function(_24){
return this.isItem(_24);
},loadItem:function(_25){
this._assertIsItem(_25.item);
},getFeatures:function(){
return this._features;
},getLabel:function(_26){
if(this._labelAttr&&this.isItem(_26)){
return this.getValue(_26,this._labelAttr);
}
return undefined;
},getLabelAttributes:function(_27){
if(this._labelAttr){
return [this._labelAttr];
}
return null;
},_fetchItems:function(_28,_29,_2a){
var _2b=this;
var _2c=function(_2d,_2e){
var _2f=[];
if(_2d.query){
var _30=_8.fromJson(_8.toJson(_2d.query));
if(typeof _30=="object"){
var _31=0;
var p;
for(p in _30){
_31++;
}
if(_31>1&&_30.complexQuery){
var cq=_30.complexQuery;
var _32=false;
for(p in _30){
if(p!=="complexQuery"){
if(!_32){
cq="( "+cq+" )";
_32=true;
}
var v=_2d.query[p];
if(_3.isString(v)){
v="'"+v+"'";
}
cq+=" AND "+p+":"+v;
delete _30[p];
}
}
_30.complexQuery=cq;
}
}
var _33=_2d.queryOptions?_2d.queryOptions.ignoreCase:false;
if(typeof _30!="string"){
_30=_8.toJson(_30);
_30=_30.replace(/\\\\/g,"\\");
}
_30=_30.replace(/\\"/g,"\"");
var _34=_3.trim(_30.replace(/{|}/g,""));
var _35,i;
if(_34.match(/"? *complexQuery *"?:/)){
_34=_3.trim(_34.replace(/"?\s*complexQuery\s*"?:/,""));
var _36=["'","\""];
var _37,_38;
var _39=false;
for(i=0;i<_36.length;i++){
_37=_34.indexOf(_36[i]);
_35=_34.indexOf(_36[i],1);
_38=_34.indexOf(":",1);
if(_37===0&&_35!=-1&&_38<_35){
_39=true;
break;
}
}
if(_39){
_34=_34.replace(/^\"|^\'|\"$|\'$/g,"");
}
}
var _3a=_34;
var _3b=/^,|^NOT |^AND |^OR |^\(|^\)|^!|^&&|^\|\|/i;
var _3c="";
var op="";
var val="";
var pos=-1;
var err=false;
var key="";
var _3d="";
var tok="";
_35=-1;
for(i=0;i<_2e.length;++i){
var _3e=true;
var _3f=_2e[i];
if(_3f===null){
_3e=false;
}else{
_34=_3a;
_3c="";
while(_34.length>0&&!err){
op=_34.match(_3b);
while(op&&!err){
_34=_3.trim(_34.replace(op[0],""));
op=_3.trim(op[0]).toUpperCase();
op=op=="NOT"?"!":op=="AND"||op==","?"&&":op=="OR"?"||":op;
op=" "+op+" ";
_3c+=op;
op=_34.match(_3b);
}
if(_34.length>0){
pos=_34.indexOf(":");
if(pos==-1){
err=true;
break;
}else{
key=_3.trim(_34.substring(0,pos).replace(/\"|\'/g,""));
_34=_3.trim(_34.substring(pos+1));
tok=_34.match(/^\'|^\"/);
if(tok){
tok=tok[0];
pos=_34.indexOf(tok);
_35=_34.indexOf(tok,pos+1);
if(_35==-1){
err=true;
break;
}
_3d=_34.substring(pos+1,_35);
if(_35==_34.length-1){
_34="";
}else{
_34=_3.trim(_34.substring(_35+1));
}
_3c+=_2b._containsValue(_3f,key,_3d,_4.patternToRegExp(_3d,_33));
}else{
tok=_34.match(/\s|\)|,/);
if(tok){
var _40=new Array(tok.length);
for(var j=0;j<tok.length;j++){
_40[j]=_34.indexOf(tok[j]);
}
pos=_40[0];
if(_40.length>1){
for(var j=1;j<_40.length;j++){
pos=Math.min(pos,_40[j]);
}
}
_3d=_3.trim(_34.substring(0,pos));
_34=_3.trim(_34.substring(pos));
}else{
_3d=_3.trim(_34);
_34="";
}
_3c+=_2b._containsValue(_3f,key,_3d,_4.patternToRegExp(_3d,_33));
}
}
}
}
_3e=eval(_3c);
}
if(_3e){
_2f.push(_3f);
}
}
if(err){
_2f=[];
}
_29(_2f,_2d);
}else{
for(var i=0;i<_2e.length;++i){
var _41=_2e[i];
if(_41!==null){
_2f.push(_41);
}
}
_29(_2f,_2d);
}
};
if(this._loadFinished){
_2c(_28,this._getItemsArray(_28.queryOptions));
}else{
if(this._jsonFileUrl!==this._ccUrl){
_1.deprecated("dojox.data.AndOrReadStore: ","To change the url, set the url property of the store,"+" not _jsonFileUrl.  _jsonFileUrl support will be removed in 2.0");
this._ccUrl=this._jsonFileUrl;
this.url=this._jsonFileUrl;
}else{
if(this.url!==this._ccUrl){
this._jsonFileUrl=this.url;
this._ccUrl=this.url;
}
}
if(this.data!=null&&this._jsonData==null){
this._jsonData=this.data;
this.data=null;
}
if(this._jsonFileUrl){
if(this._loadInProgress){
this._queuedFetches.push({args:_28,filter:_2c});
}else{
this._loadInProgress=true;
var _42={url:_2b._jsonFileUrl,handleAs:"json-comment-optional",preventCache:this.urlPreventCache};
var _43=_a.get(_42);
_43.addCallback(function(_44){
try{
_2b._getItemsFromLoadedData(_44);
_2b._loadFinished=true;
_2b._loadInProgress=false;
_2c(_28,_2b._getItemsArray(_28.queryOptions));
_2b._handleQueuedFetches();
}
catch(e){
_2b._loadFinished=true;
_2b._loadInProgress=false;
_2a(e,_28);
}
});
_43.addErrback(function(_45){
_2b._loadInProgress=false;
_2a(_45,_28);
});
var _46=null;
if(_28.abort){
_46=_28.abort;
}
_28.abort=function(){
var df=_43;
if(df&&df.fired===-1){
df.cancel();
df=null;
}
if(_46){
_46.call(_28);
}
};
}
}else{
if(this._jsonData){
try{
this._loadFinished=true;
this._getItemsFromLoadedData(this._jsonData);
this._jsonData=null;
_2c(_28,this._getItemsArray(_28.queryOptions));
}
catch(e){
_2a(e,_28);
}
}else{
_2a(new Error("dojox.data.AndOrReadStore: No JSON source data was provided as either URL or a nested Javascript object."),_28);
}
}
}
},_handleQueuedFetches:function(){
if(this._queuedFetches.length>0){
for(var i=0;i<this._queuedFetches.length;i++){
var _47=this._queuedFetches[i];
var _48=_47.args;
var _49=_47.filter;
if(_49){
_49(_48,this._getItemsArray(_48.queryOptions));
}else{
this.fetchItemByIdentity(_48);
}
}
this._queuedFetches=[];
}
},_getItemsArray:function(_4a){
if(_4a&&_4a.deep){
return this._arrayOfAllItems;
}
return this._arrayOfTopLevelItems;
},close:function(_4b){
if(this.clearOnClose&&this._loadFinished&&!this._loadInProgress){
if(((this._jsonFileUrl==""||this._jsonFileUrl==null)&&(this.url==""||this.url==null))&&this.data==null){
}
this._arrayOfAllItems=[];
this._arrayOfTopLevelItems=[];
this._loadFinished=false;
this._itemsByIdentity=null;
this._loadInProgress=false;
this._queuedFetches=[];
}
},_getItemsFromLoadedData:function(_4c){
var _4d=this;
function _4e(_4f){
var _50=((_4f!==null)&&(typeof _4f==="object")&&(!_3.isArray(_4f))&&(!_3.isFunction(_4f))&&(_4f.constructor==Object)&&(typeof _4f._reference==="undefined")&&(typeof _4f._type==="undefined")&&(typeof _4f._value==="undefined")&&_4d.hierarchical);
return _50;
};
function _51(_52){
_4d._arrayOfAllItems.push(_52);
for(var _53 in _52){
var _54=_52[_53];
if(_54){
if(_3.isArray(_54)){
var _55=_54;
for(var k=0;k<_55.length;++k){
var _56=_55[k];
if(_4e(_56)){
_51(_56);
}
}
}else{
if(_4e(_54)){
_51(_54);
}
}
}
}
};
this._labelAttr=_4c.label;
var i;
var _57;
this._arrayOfAllItems=[];
this._arrayOfTopLevelItems=_4c.items;
for(i=0;i<this._arrayOfTopLevelItems.length;++i){
_57=this._arrayOfTopLevelItems[i];
_51(_57);
_57[this._rootItemPropName]=true;
}
var _58={};
var key;
for(i=0;i<this._arrayOfAllItems.length;++i){
_57=this._arrayOfAllItems[i];
for(key in _57){
if(key!==this._rootItemPropName){
var _59=_57[key];
if(_59!==null){
if(!_3.isArray(_59)){
_57[key]=[_59];
}
}else{
_57[key]=[null];
}
}
_58[key]=key;
}
}
while(_58[this._storeRefPropName]){
this._storeRefPropName+="_";
}
while(_58[this._itemNumPropName]){
this._itemNumPropName+="_";
}
while(_58[this._reverseRefMap]){
this._reverseRefMap+="_";
}
var _5a;
var _5b=_4c.identifier;
if(_5b){
this._itemsByIdentity={};
this._features["dojo.data.api.Identity"]=_5b;
for(i=0;i<this._arrayOfAllItems.length;++i){
_57=this._arrayOfAllItems[i];
_5a=_57[_5b];
var _5c=_5a[0];
if(!this._itemsByIdentity[_5c]){
this._itemsByIdentity[_5c]=_57;
}else{
if(this._jsonFileUrl){
throw new Error("dojox.data.AndOrReadStore:  The json data as specified by: ["+this._jsonFileUrl+"] is malformed.  Items within the list have identifier: ["+_5b+"].  Value collided: ["+_5c+"]");
}else{
if(this._jsonData){
throw new Error("dojox.data.AndOrReadStore:  The json data provided by the creation arguments is malformed.  Items within the list have identifier: ["+_5b+"].  Value collided: ["+_5c+"]");
}
}
}
}
}else{
this._features["dojo.data.api.Identity"]=Number;
}
for(i=0;i<this._arrayOfAllItems.length;++i){
_57=this._arrayOfAllItems[i];
_57[this._storeRefPropName]=this;
_57[this._itemNumPropName]=i;
}
for(i=0;i<this._arrayOfAllItems.length;++i){
_57=this._arrayOfAllItems[i];
for(key in _57){
_5a=_57[key];
for(var j=0;j<_5a.length;++j){
_59=_5a[j];
if(_59!==null&&typeof _59=="object"){
if(("_type" in _59)&&("_value" in _59)){
var _5d=_59._type;
var _5e=this._datatypeMap[_5d];
if(!_5e){
throw new Error("dojox.data.AndOrReadStore: in the typeMap constructor arg, no object class was specified for the datatype '"+_5d+"'");
}else{
if(_3.isFunction(_5e)){
_5a[j]=new _5e(_59._value);
}else{
if(_3.isFunction(_5e.deserialize)){
_5a[j]=_5e.deserialize(_59._value);
}else{
throw new Error("dojox.data.AndOrReadStore: Value provided in typeMap was neither a constructor, nor a an object with a deserialize function");
}
}
}
}
if(_59._reference){
var _5f=_59._reference;
if(!_3.isObject(_5f)){
_5a[j]=this._getItemByIdentity(_5f);
}else{
for(var k=0;k<this._arrayOfAllItems.length;++k){
var _60=this._arrayOfAllItems[k];
var _61=true;
for(var _62 in _5f){
if(_60[_62]!=_5f[_62]){
_61=false;
}
}
if(_61){
_5a[j]=_60;
}
}
}
if(this.referenceIntegrity){
var _63=_5a[j];
if(this.isItem(_63)){
this._addReferenceToMap(_63,_57,key);
}
}
}else{
if(this.isItem(_59)){
if(this.referenceIntegrity){
this._addReferenceToMap(_59,_57,key);
}
}
}
}
}
}
}
},_addReferenceToMap:function(_64,_65,_66){
},getIdentity:function(_67){
var _68=this._features["dojo.data.api.Identity"];
if(_68===Number){
return _67[this._itemNumPropName];
}else{
var _69=_67[_68];
if(_69){
return _69[0];
}
}
return null;
},fetchItemByIdentity:function(_6a){
if(!this._loadFinished){
var _6b=this;
if(this._jsonFileUrl!==this._ccUrl){
_1.deprecated("dojox.data.AndOrReadStore: ","To change the url, set the url property of the store,"+" not _jsonFileUrl.  _jsonFileUrl support will be removed in 2.0");
this._ccUrl=this._jsonFileUrl;
this.url=this._jsonFileUrl;
}else{
if(this.url!==this._ccUrl){
this._jsonFileUrl=this.url;
this._ccUrl=this.url;
}
}
if(this.data!=null&&this._jsonData==null){
this._jsonData=this.data;
this.data=null;
}
if(this._jsonFileUrl){
if(this._loadInProgress){
this._queuedFetches.push({args:_6a});
}else{
this._loadInProgress=true;
var _6c={url:_6b._jsonFileUrl,handleAs:"json-comment-optional",preventCache:this.urlPreventCache};
var _6d=_a.get(_6c);
_6d.addCallback(function(_6e){
var _6f=_6a.scope?_6a.scope:_9.global;
try{
_6b._getItemsFromLoadedData(_6e);
_6b._loadFinished=true;
_6b._loadInProgress=false;
var _70=_6b._getItemByIdentity(_6a.identity);
if(_6a.onItem){
_6a.onItem.call(_6f,_70);
}
_6b._handleQueuedFetches();
}
catch(error){
_6b._loadInProgress=false;
if(_6a.onError){
_6a.onError.call(_6f,error);
}
}
});
_6d.addErrback(function(_71){
_6b._loadInProgress=false;
if(_6a.onError){
var _72=_6a.scope?_6a.scope:_9.global;
_6a.onError.call(_72,_71);
}
});
}
}else{
if(this._jsonData){
_6b._getItemsFromLoadedData(_6b._jsonData);
_6b._jsonData=null;
_6b._loadFinished=true;
var _73=_6b._getItemByIdentity(_6a.identity);
if(_6a.onItem){
var _74=_6a.scope?_6a.scope:_9.global;
_6a.onItem.call(_74,_73);
}
}
}
}else{
var _73=this._getItemByIdentity(_6a.identity);
if(_6a.onItem){
var _74=_6a.scope?_6a.scope:_9.global;
_6a.onItem.call(_74,_73);
}
}
},_getItemByIdentity:function(_75){
var _76=null;
if(this._itemsByIdentity){
_76=this._itemsByIdentity[_75];
}else{
_76=this._arrayOfAllItems[_75];
}
if(_76===undefined){
_76=null;
}
return _76;
},getIdentityAttributes:function(_77){
var _78=this._features["dojo.data.api.Identity"];
if(_78===Number){
return null;
}else{
return [_78];
}
},_forceLoad:function(){
var _79=this;
if(this._jsonFileUrl!==this._ccUrl){
_1.deprecated("dojox.data.AndOrReadStore: ","To change the url, set the url property of the store,"+" not _jsonFileUrl.  _jsonFileUrl support will be removed in 2.0");
this._ccUrl=this._jsonFileUrl;
this.url=this._jsonFileUrl;
}else{
if(this.url!==this._ccUrl){
this._jsonFileUrl=this.url;
this._ccUrl=this.url;
}
}
if(this.data!=null&&this._jsonData==null){
this._jsonData=this.data;
this.data=null;
}
if(this._jsonFileUrl){
var _7a={url:_79._jsonFileUrl,handleAs:"json-comment-optional",preventCache:this.urlPreventCache,sync:true};
var _7b=_a.get(_7a);
_7b.addCallback(function(_7c){
try{
if(_79._loadInProgress!==true&&!_79._loadFinished){
_79._getItemsFromLoadedData(_7c);
_79._loadFinished=true;
}else{
if(_79._loadInProgress){
throw new Error("dojox.data.AndOrReadStore:  Unable to perform a synchronous load, an async load is in progress.");
}
}
}
catch(e){
throw e;
}
});
_7b.addErrback(function(_7d){
throw _7d;
});
}else{
if(this._jsonData){
_79._getItemsFromLoadedData(_79._jsonData);
_79._jsonData=null;
_79._loadFinished=true;
}
}
}});
_3.extend(_b,_5);
return _b;
});
