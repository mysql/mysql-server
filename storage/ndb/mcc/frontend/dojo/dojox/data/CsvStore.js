//>>built
define("dojox/data/CsvStore",["dojo/_base/lang","dojo/_base/declare","dojo/_base/xhr","dojo/_base/window","dojo/data/util/filter","dojo/data/util/simpleFetch"],function(_1,_2,_3,_4,_5,_6){
var _7=_2("dojox.data.CsvStore",null,{constructor:function(_8){
this._attributes=[];
this._attributeIndexes={};
this._dataArray=[];
this._arrayOfAllItems=[];
this._loadFinished=false;
if(_8.url){
this.url=_8.url;
}
this._csvData=_8.data;
if(_8.label){
this.label=_8.label;
}else{
if(this.label===""){
this.label=undefined;
}
}
this._storeProp="_csvStore";
this._idProp="_csvId";
this._features={"dojo.data.api.Read":true,"dojo.data.api.Identity":true};
this._loadInProgress=false;
this._queuedFetches=[];
this.identifier=_8.identifier;
if(this.identifier===""){
delete this.identifier;
}else{
this._idMap={};
}
if("separator" in _8){
this.separator=_8.separator;
}
if("urlPreventCache" in _8){
this.urlPreventCache=_8.urlPreventCache?true:false;
}
},url:"",label:"",identifier:"",separator:",",urlPreventCache:false,_assertIsItem:function(_9){
if(!this.isItem(_9)){
throw new Error(this.declaredClass+": a function was passed an item argument that was not an item");
}
},_getIndex:function(_a){
var _b=this.getIdentity(_a);
if(this.identifier){
_b=this._idMap[_b];
}
return _b;
},getValue:function(_c,_d,_e){
this._assertIsItem(_c);
var _f=_e;
if(typeof _d==="string"){
var ai=this._attributeIndexes[_d];
if(ai!=null){
var _10=this._dataArray[this._getIndex(_c)];
_f=_10[ai]||_e;
}
}else{
throw new Error(this.declaredClass+": a function was passed an attribute argument that was not a string");
}
return _f;
},getValues:function(_11,_12){
var _13=this.getValue(_11,_12);
return (_13?[_13]:[]);
},getAttributes:function(_14){
this._assertIsItem(_14);
var _15=[];
var _16=this._dataArray[this._getIndex(_14)];
for(var i=0;i<_16.length;i++){
if(_16[i]!==""){
_15.push(this._attributes[i]);
}
}
return _15;
},hasAttribute:function(_17,_18){
this._assertIsItem(_17);
if(typeof _18==="string"){
var _19=this._attributeIndexes[_18];
var _1a=this._dataArray[this._getIndex(_17)];
return (typeof _19!=="undefined"&&_19<_1a.length&&_1a[_19]!=="");
}else{
throw new Error(this.declaredClass+": a function was passed an attribute argument that was not a string");
}
},containsValue:function(_1b,_1c,_1d){
var _1e=undefined;
if(typeof _1d==="string"){
_1e=_5.patternToRegExp(_1d,false);
}
return this._containsValue(_1b,_1c,_1d,_1e);
},_containsValue:function(_1f,_20,_21,_22){
var _23=this.getValues(_1f,_20);
for(var i=0;i<_23.length;++i){
var _24=_23[i];
if(typeof _24==="string"&&_22){
return (_24.match(_22)!==null);
}else{
if(_21===_24){
return true;
}
}
}
return false;
},isItem:function(_25){
if(_25&&_25[this._storeProp]===this){
var _26=_25[this._idProp];
if(this.identifier){
var _27=this._dataArray[this._idMap[_26]];
if(_27){
return true;
}
}else{
if(_26>=0&&_26<this._dataArray.length){
return true;
}
}
}
return false;
},isItemLoaded:function(_28){
return this.isItem(_28);
},loadItem:function(_29){
},getFeatures:function(){
return this._features;
},getLabel:function(_2a){
if(this.label&&this.isItem(_2a)){
return this.getValue(_2a,this.label);
}
return undefined;
},getLabelAttributes:function(_2b){
if(this.label){
return [this.label];
}
return null;
},_fetchItems:function(_2c,_2d,_2e){
var _2f=this;
var _30=function(_31,_32){
var _33=null;
if(_31.query){
var key,_34;
_33=[];
var _35=_31.queryOptions?_31.queryOptions.ignoreCase:false;
var _36={};
for(key in _31.query){
_34=_31.query[key];
if(typeof _34==="string"){
_36[key]=_5.patternToRegExp(_34,_35);
}
}
for(var i=0;i<_32.length;++i){
var _37=true;
var _38=_32[i];
for(key in _31.query){
_34=_31.query[key];
if(!_2f._containsValue(_38,key,_34,_36[key])){
_37=false;
}
}
if(_37){
_33.push(_38);
}
}
}else{
_33=_32.slice(0,_32.length);
}
_2d(_33,_31);
};
if(this._loadFinished){
_30(_2c,this._arrayOfAllItems);
}else{
if(this.url!==""){
if(this._loadInProgress){
this._queuedFetches.push({args:_2c,filter:_30});
}else{
this._loadInProgress=true;
var _39={url:_2f.url,handleAs:"text",preventCache:_2f.urlPreventCache};
var _3a=_3.get(_39);
_3a.addCallback(function(_3b){
try{
_2f._processData(_3b);
_30(_2c,_2f._arrayOfAllItems);
_2f._handleQueuedFetches();
}
catch(e){
_2e(e,_2c);
}
});
_3a.addErrback(function(_3c){
_2f._loadInProgress=false;
if(_2e){
_2e(_3c,_2c);
}else{
throw _3c;
}
});
var _3d=null;
if(_2c.abort){
_3d=_2c.abort;
}
_2c.abort=function(){
var df=_3a;
if(df&&df.fired===-1){
df.cancel();
df=null;
}
if(_3d){
_3d.call(_2c);
}
};
}
}else{
if(this._csvData){
try{
this._processData(this._csvData);
this._csvData=null;
_30(_2c,this._arrayOfAllItems);
}
catch(e){
_2e(e,_2c);
}
}else{
var _3e=new Error(this.declaredClass+": No CSV source data was provided as either URL or String data input.");
if(_2e){
_2e(_3e,_2c);
}else{
throw _3e;
}
}
}
}
},close:function(_3f){
},_getArrayOfArraysFromCsvFileContents:function(_40){
if(_1.isString(_40)){
var _41=new RegExp("^\\s+","g");
var _42=new RegExp("\\s+$","g");
var _43=new RegExp("\"\"","g");
var _44=[];
var i;
var _45=this._splitLines(_40);
for(i=0;i<_45.length;++i){
var _46=_45[i];
if(_46.length>0){
var _47=_46.split(this.separator);
var j=0;
while(j<_47.length){
var _48=_47[j];
var _49=_48.replace(_41,"");
var _4a=_49.replace(_42,"");
var _4b=_4a.charAt(0);
var _4c=_4a.charAt(_4a.length-1);
var _4d=_4a.charAt(_4a.length-2);
var _4e=_4a.charAt(_4a.length-3);
if(_4a.length===2&&_4a=="\"\""){
_47[j]="";
}else{
if((_4b=="\"")&&((_4c!="\"")||((_4c=="\"")&&(_4d=="\"")&&(_4e!="\"")))){
if(j+1===_47.length){
return;
}
var _4f=_47[j+1];
_47[j]=_49+this.separator+_4f;
_47.splice(j+1,1);
}else{
if((_4b=="\"")&&(_4c=="\"")){
_4a=_4a.slice(1,(_4a.length-1));
_4a=_4a.replace(_43,"\"");
}
_47[j]=_4a;
j+=1;
}
}
}
_44.push(_47);
}
}
this._attributes=_44.shift();
for(i=0;i<this._attributes.length;i++){
this._attributeIndexes[this._attributes[i]]=i;
}
this._dataArray=_44;
}
},_splitLines:function(_50){
var _51=[];
var i;
var _52="";
var _53=false;
for(i=0;i<_50.length;i++){
var c=_50.charAt(i);
switch(c){
case "\"":
_53=!_53;
_52+=c;
break;
case "\r":
if(_53){
_52+=c;
}else{
_51.push(_52);
_52="";
if(i<(_50.length-1)&&_50.charAt(i+1)=="\n"){
i++;
}
}
break;
case "\n":
if(_53){
_52+=c;
}else{
_51.push(_52);
_52="";
}
break;
default:
_52+=c;
}
}
if(_52!==""){
_51.push(_52);
}
return _51;
},_processData:function(_54){
this._getArrayOfArraysFromCsvFileContents(_54);
this._arrayOfAllItems=[];
if(this.identifier){
if(this._attributeIndexes[this.identifier]===undefined){
throw new Error(this.declaredClass+": Identity specified is not a column header in the data set.");
}
}
for(var i=0;i<this._dataArray.length;i++){
var id=i;
if(this.identifier){
var _55=this._dataArray[i];
id=_55[this._attributeIndexes[this.identifier]];
this._idMap[id]=i;
}
this._arrayOfAllItems.push(this._createItemFromIdentity(id));
}
this._loadFinished=true;
this._loadInProgress=false;
},_createItemFromIdentity:function(_56){
var _57={};
_57[this._storeProp]=this;
_57[this._idProp]=_56;
return _57;
},getIdentity:function(_58){
if(this.isItem(_58)){
return _58[this._idProp];
}
return null;
},fetchItemByIdentity:function(_59){
var _5a;
var _5b=_59.scope?_59.scope:_4.global;
if(!this._loadFinished){
var _5c=this;
if(this.url!==""){
if(this._loadInProgress){
this._queuedFetches.push({args:_59});
}else{
this._loadInProgress=true;
var _5d={url:_5c.url,handleAs:"text"};
var _5e=_3.get(_5d);
_5e.addCallback(function(_5f){
try{
_5c._processData(_5f);
var _60=_5c._createItemFromIdentity(_59.identity);
if(!_5c.isItem(_60)){
_60=null;
}
if(_59.onItem){
_59.onItem.call(_5b,_60);
}
_5c._handleQueuedFetches();
}
catch(error){
if(_59.onError){
_59.onError.call(_5b,error);
}
}
});
_5e.addErrback(function(_61){
this._loadInProgress=false;
if(_59.onError){
_59.onError.call(_5b,_61);
}
});
}
}else{
if(this._csvData){
try{
_5c._processData(_5c._csvData);
_5c._csvData=null;
_5a=_5c._createItemFromIdentity(_59.identity);
if(!_5c.isItem(_5a)){
_5a=null;
}
if(_59.onItem){
_59.onItem.call(_5b,_5a);
}
}
catch(e){
if(_59.onError){
_59.onError.call(_5b,e);
}
}
}
}
}else{
_5a=this._createItemFromIdentity(_59.identity);
if(!this.isItem(_5a)){
_5a=null;
}
if(_59.onItem){
_59.onItem.call(_5b,_5a);
}
}
},getIdentityAttributes:function(_62){
if(this.identifier){
return [this.identifier];
}else{
return null;
}
},_handleQueuedFetches:function(){
if(this._queuedFetches.length>0){
for(var i=0;i<this._queuedFetches.length;i++){
var _63=this._queuedFetches[i];
var _64=_63.filter;
var _65=_63.args;
if(_64){
_64(_65,this._arrayOfAllItems);
}else{
this.fetchItemByIdentity(_63.args);
}
}
this._queuedFetches=[];
}
}});
_1.extend(_7,_6);
return _7;
});
