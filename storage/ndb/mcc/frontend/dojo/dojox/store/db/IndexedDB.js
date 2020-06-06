//>>built
define("dojox/store/db/IndexedDB",["dojo/_base/declare","dojo/_base/lang","dojo/Deferred","dojo/when","dojo/promise/all","dojo/store/util/SimpleQueryEngine","dojo/store/util/QueryResults"],function(_1,_2,_3,_4,_5,_6,_7){
function _8(_9){
var _a=new _3();
_9.onsuccess=function(_b){
_a.resolve(_b.target.result);
};
_9.onerror=function(){
_9.error.message=_9.webkitErrorMessage;
_a.reject(_9.error);
};
return _a.promise;
};
var _c=[];
var _d=1;
var _e=0;
var _f=/(.*)\*$/;
function _10(_11,_12,_13){
if(_e||_c.length){
if(_11){
_c.push({cursor:_11,priority:_12,retry:_13});
_c.sort(function(a,b){
return a.priority>b.priority?1:-1;
});
}
if(_e>=_d){
return;
}
var _14=_c.pop();
_11=_14&&_14.cursor;
}
if(_11){
try{
_11["continue"]();
_e++;
}
catch(e){
if((e.name==="TransactionInactiveError"||e.name===0)&&_14){
_14.retry();
}else{
throw e;
}
}
}
};
function yes(){
return true;
};
function _15(_16){
var _17,_18,_19=[];
function _1a(_1b,_1c){
if(_18){
_1b&&_17.then(function(_1d){
_1d.forEach(_1b,_1c);
});
}else{
_1b&&_19.push(_1b);
if(!_17){
_17=_16.filter(function(_1e){
_18=true;
for(var i=0,l=_19.length;i<l;i++){
_19[i].call(_1c,_1e);
}
return true;
});
}
}
return _17;
};
return {total:_16.total,filter:function(_1f,_20){
var _21;
return _1a(function(_22){
if(!_21){
_21=!_1f.call(_20,_22);
}
});
},forEach:_1a,map:function(_23,_24){
var _25=[];
return _1a(function(_26){
_25.push(_23.call(_24,_26));
}).then(function(){
return _25;
});
},then:function(_27,_28){
return _1a().then(_27,_28);
}};
};
var _29=window.IDBKeyRange||window.webkitIDBKeyRange;
return _1(null,{constructor:function(_2a){
_1.safeMixin(this,_2a);
var _2b=this;
var _2c=this.dbConfig;
this.indices=_2c.stores[this.storeName];
this.cachedCount={};
for(var _2d in _2b.indices){
var _2e=_2b.indices[_2d];
if(typeof _2e==="number"){
_2b.indices[_2d]={preference:_2e};
}
}
this.db=this.db||_2c.db;
if(!this.db){
var _2f=_2c.openRequest;
if(!_2f){
_2f=_2c.openRequest=window.indexedDB.open(_2c.name||"dojo-db",parseInt(_2c.version,10));
_2f.onupgradeneeded=function(){
var db=_2b.db=_2f.result;
for(var _30 in _2c.stores){
var _31=_2c.stores[_30];
if(!db.objectStoreNames.contains(_30)){
var _32=_31.idProperty||"id";
var _33=db.createObjectStore(_30,{keyPath:_32,autoIncrement:_31[_32]&&_31[_32].autoIncrement||false});
}else{
_33=_2f.transaction.objectStore(_30);
}
for(var _34 in _31){
if(!_33.indexNames.contains(_34)&&_34!=="autoIncrement"&&_31[_34].indexed!==false){
_33.createIndex(_34,_34,_31[_34]);
}
}
}
};
_2c.available=_8(_2f);
}
this.available=_2c.available.then(function(db){
return _2b.db=db;
});
}
},idProperty:"id",storeName:"",indices:{},queryEngine:_6,transaction:function(){
var _35=this;
this._currentTransaction=null;
return {abort:function(){
_35._currentTransaction.abort();
},commit:function(){
_35._currentTransaction=null;
}};
},_getTransaction:function(){
if(!this._currentTransaction){
this._currentTransaction=this.db.transaction([this.storeName],"readwrite");
var _36=this;
this._currentTransaction.oncomplete=function(){
_36._currentTransaction=null;
};
this._currentTransaction.onerror=function(_37){
console.error(_37);
};
}
return this._currentTransaction;
},_callOnStore:function(_38,_39,_3a,_3b){
var _3c=this;
return _4(this.available,function callOnStore(){
var _3d=_3c._currentTransaction;
if(_3d){
var _3e=true;
}else{
_3d=_3c._getTransaction();
}
var _3f,_40;
if(_3e){
try{
_40=_3d.objectStore(_3c.storeName);
if(_3a){
_40=_40.index(_3a);
}
_3f=_40[_38].apply(_40,_39);
}
catch(e){
if(e.name==="TransactionInactiveError"||e.name==="InvalidStateError"){
_3c._currentTransaction=null;
return _41();
}else{
throw e;
}
}
}else{
_40=_3d.objectStore(_3c.storeName);
if(_3a){
_40=_40.index(_3a);
}
_3f=_40[_38].apply(_40,_39);
}
return _3b?_3f:_8(_3f);
});
},get:function(id){
return this._callOnStore("get",[id]);
},getIdentity:function(_42){
return _42[this.idProperty];
},put:function(_43,_44){
_44=_44||{};
this.cachedCount={};
return this._callOnStore(_44.overwrite===false?"add":"put",[_43]);
},add:function(_45,_46){
_46=_46||{};
_46.overwrite=false;
return this.put(_45,_46);
},remove:function(id){
this.cachedCount={};
return this._callOnStore("delete",[id]);
},query:function(_47,_48){
_48=_48||{};
var _49=_48.start||0;
var _4a=_48.count||Infinity;
var _4b=_48.sort;
var _4c=this;
if(_47.forEach){
var _4d={sort:_4b};
var _4e=this.queryEngine({},_4d);
var _4f=[];
var _50=0;
var _51=0;
return _15({total:{then:function(){
return _5(_4f).then(function(_52){
return _52.reduce(function(a,b){
return a+b;
})*_50/(_51||1);
}).then.apply(this,arguments);
}},filter:function(_53,_54){
var _55=0;
var _56=[];
var _57;
var _58={};
var _59=[];
return _5(_47.map(function(_5a,i){
var _5b=_56[i]=[];
function _5c(_5d){
_5b.push(_5d);
var _5e=[];
var _5f=[];
while(_56.every(function(_60){
if(_60.length>0){
var _61=_60[0];
if(_61){
_5f.push(_61);
}
return _5e.push(_61);
}
})){
if(_55>=_49+_4a||_5f.length===0){
_57=true;
return;
}
var _62=_4e(_5f)[0];
_56[_5e.indexOf(_62)].shift();
if(_55++>=_49){
_59.push(_62);
if(!_53.call(_54,_62)){
_57=true;
return;
}
}
_5e=[];
_5f=[];
}
return true;
};
var _63=_4c.query(_5a,_4d);
_4f[i]=_63.total;
return _63.filter(function(_64){
if(_57){
return;
}
var id=_4c.getIdentity(_64);
_51++;
if(id in _58){
return true;
}
_50++;
_58[id]=true;
return _5c(_64);
}).then(function(_65){
_5c(null);
return _65;
});
})).then(function(){
return _59;
});
}});
}
var _66;
var _67;
var _68=JSON.stringify(_47)+"-"+JSON.stringify(_48.sort);
var _69;
var _6a,_6b=0;
var _6c=0;
var _6d;
function _6e(_6f,_70,_71){
_6c++;
var _72=_4c.indices[_6f];
if(_72&&_72.indexed!==false){
_70=_70||_72.preference*(_71||1)||0.001;
if(_70>_6b){
_6b=_70;
_6a=_6f;
return true;
}
}
_6c++;
};
for(var i in _47){
_6d=_47[i];
var _73=false;
var _74,_75=null;
if(typeof _6d==="boolean"){
continue;
}
if(_6d){
if(_6d.from||_6d.to){
_73=true;
(function(_76,to){
_75={test:function(_77){
return !_76||_76<=_77&&(!to||to>=_77);
},keyRange:_76?to?_29.bound(_76,to,_6d.excludeFrom,_6d.excludeTo):_29.lowerBound(_76,_6d.excludeFrom):_29.upperBound(to,_6d.excludeTo)};
})(_6d.from,_6d.to);
}else{
if(typeof _6d==="object"&&_6d.contains){
(function(_78){
var _79,_7a=_78[0];
var _7b=_7a&&_7a.match&&_7a.match(_f);
if(_7b){
_7a=_7b[1];
_79=_29.bound(_7a,_7a+"~");
}else{
_79=_29.only(_7a);
}
_75={test:function(_7c){
return _78.every(function(_7d){
var _7e=_7d&&_7d.match&&_7d.match(_f);
if(_7e){
_7d=_7e[1];
return _7c&&_7c.some(function(_7f){
return _7f.slice(0,_7d.length)===_7d;
});
}
return _7c&&_7c.indexOf(_7d)>-1;
});
},keyRange:_79};
})(_6d.contains);
}else{
if((_74=_6d.match&&_6d.match(_f))){
var _80=_74[1];
_75=new RegExp("^"+_80);
_75.keyRange=_29.bound(_80,_80+"~");
}
}
}
}
if(_75){
_47[i]=_75;
}
_6e(i,null,_73?0.1:1);
}
var _81;
if(_4b){
var _82=_4b[0];
if(_82.attribute===_6a||_6e(_82.attribute,1)){
_81=_82.descending;
}else{
var _83=true;
_49=0;
_4a=Infinity;
}
}
var _84;
if(_6a){
if(_6a in _47){
_6d=_47[_6a];
if(_6d&&(_6d.keyRange)){
_66=_6d.keyRange;
}else{
_66=_29.only(_6d);
}
_67=_6a;
}else{
_66=null;
}
_84=[_66,_81?"prev":"next"];
}else{
_84=[];
}
var _85=_4c.cachedPosition;
if(_85&&_85.queryId===_68&&_85.offset<_49&&_6c>1){
_69=_85.preFilterOffset+1;
_4c.cachedPosition=_85=_2.mixin({},_85);
}else{
_85=_4c.cachedPosition={offset:-1,preFilterOffset:-1,queryId:_68};
if(_6c<2){
_85.offset=_85.preFilterOffset=(_69=_49)-1;
}
}
var _86=this.queryEngine(_47);
var _87={total:{then:function(_88){
var _89=_4c.cachedCount[_68];
if(_89){
return _88(_8a(_89));
}else{
var _8b=(_66?_4c._callOnStore("count",[_66],_6a):_4c._callOnStore("count"));
return (this.then=_8b.then(_8a)).then.apply(this,arguments);
}
function _8a(_8c){
_4c.cachedCount[_68]=_8c;
return Math.round((_85.offset+1.01)/(_85.preFilterOffset+1.01)*_8c);
};
}},filter:function(_8d,_8e){
var _8f=new _3();
var all=[];
function _90(){
_4(_4c._callOnStore("openCursor",_84,_6a,true),function(_91){
_e++;
_91.onsuccess=function(_92){
_e--;
var _93=_92.target.result;
if(_93){
if(_69){
_93.advance(_69);
_e++;
_69=false;
return;
}
_85.preFilterOffset++;
try{
var _94=_93.value;
if(_48.join){
_94=_48.join(_94);
}
return _4(_94,function(_95){
if(_86.matches(_95)){
_85.offset++;
if(_85.offset>=_49){
all.push(_95);
if(!_8d.call(_8e,_95)||_85.offset>=_49+_4a-1){
_91.lastCursor=_93;
_8f.resolve(all);
_10();
return;
}
}
}
return _10(_93,_48.priority,function(){
_69=_85.preFilterOffset;
_90();
});
});
}
catch(e){
_8f.reject(e);
}
}else{
_8f.resolve(all);
}
_10();
};
_91.onerror=function(_96){
_e--;
_8f.reject(_96);
_10();
};
});
};
_90();
return _8f.promise;
}};
if(_83){
var _4e=this.queryEngine({},_48);
var _97=_2.delegate(_87.filter(yes).then(function(_98){
return _4e(_98);
}));
_97.total=_87.total;
return new _7(_97);
}
return _48.rawResults?_87:_15(_87);
}});
});
