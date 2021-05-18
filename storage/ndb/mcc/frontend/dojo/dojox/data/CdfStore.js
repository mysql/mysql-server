//>>built
define("dojox/data/CdfStore",["dojo","dojox","dojo/data/util/sorter"],function(_1,_2){
_2.data.ASYNC_MODE=0;
_2.data.SYNC_MODE=1;
return _1.declare("dojox.data.CdfStore",null,{identity:"jsxid",url:"",xmlStr:"",data:null,label:"",mode:_2.data.ASYNC_MODE,constructor:function(_3){
if(_3){
this.url=_3.url;
this.xmlStr=_3.xmlStr||_3.str;
if(_3.data){
this.xmlStr=this._makeXmlString(_3.data);
}
this.identity=_3.identity||this.identity;
this.label=_3.label||this.label;
this.mode=_3.mode!==undefined?_3.mode:this.mode;
}
this._modifiedItems={};
this.byId=this.fetchItemByIdentity;
},getValue:function(_4,_5,_6){
return _4.getAttribute(_5)||_6;
},getValues:function(_7,_8){
var v=this.getValue(_7,_8,[]);
return _1.isArray(v)?v:[v];
},getAttributes:function(_9){
return _9.getAttributeNames();
},hasAttribute:function(_a,_b){
return (this.getValue(_a,_b)!==undefined);
},hasProperty:function(_c,_d){
return this.hasAttribute(_c,_d);
},containsValue:function(_e,_f,_10){
var _11=this.getValues(_e,_f);
for(var i=0;i<_11.length;i++){
if(_11[i]===null){
continue;
}
if((typeof _10==="string")){
if(_11[i].toString&&_11[i].toString()===_10){
return true;
}
}else{
if(_11[i]===_10){
return true;
}
}
}
return false;
},isItem:function(_12){
if(_12.getClass&&_12.getClass().equals(jsx3.xml.Entity.jsxclass)){
return true;
}
return false;
},isItemLoaded:function(_13){
return this.isItem(_13);
},loadItem:function(_14){
},getFeatures:function(){
return {"dojo.data.api.Read":true,"dojo.data.api.Write":true,"dojo.data.api.Identity":true};
},getLabel:function(_15){
if((this.label!=="")&&this.isItem(_15)){
var _16=this.getValue(_15,this.label);
if(_16){
return _16.toString();
}
}
return undefined;
},getLabelAttributes:function(_17){
if(this.label!==""){
return [this.label];
}
return null;
},fetch:function(_18){
_18=_18||{};
if(!_18.store){
_18.store=this;
}
if(_18.mode!==undefined){
this.mode=_18.mode;
}
var _19=this;
var _1a=function(_1b){
if(_18.onError){
var _1c=_18.scope||_1.global;
_18.onError.call(_1c,_1b,_18);
}else{
console.error("cdfStore Error:",_1b);
}
};
var _1d=function(_1e,_1f){
_1f=_1f||_18;
var _20=_1f.abort||null;
var _21=false;
var _22=_1f.start?_1f.start:0;
var _23=(_1f.count&&(_1f.count!==Infinity))?(_22+_1f.count):_1e.length;
_1f.abort=function(){
_21=true;
if(_20){
_20.call(_1f);
}
};
var _24=_1f.scope||_1.global;
if(!_1f.store){
_1f.store=_19;
}
if(_1f.onBegin){
_1f.onBegin.call(_24,_1e.length,_1f);
}
if(_1f.sort){
_1e.sort(_1.data.util.sorter.createSortFunction(_1f.sort,_19));
}
if(_1f.onItem){
for(var i=_22;(i<_1e.length)&&(i<_23);++i){
var _25=_1e[i];
if(!_21){
_1f.onItem.call(_24,_25,_1f);
}
}
}
if(_1f.onComplete&&!_21){
if(!_1f.onItem){
_1e=_1e.slice(_22,_23);
if(_1f.byId){
_1e=_1e[0];
}
}
_1f.onComplete.call(_24,_1e,_1f);
}else{
_1e=_1e.slice(_22,_23);
if(_1f.byId){
_1e=_1e[0];
}
}
return _1e;
};
if(!this.url&&!this.data&&!this.xmlStr){
_1a(new Error("No URL or data specified."));
return false;
}
var _26=_18||"*";
if(this.mode==_2.data.SYNC_MODE){
var res=this._loadCDF();
if(res instanceof Error){
if(_18.onError){
_18.onError.call(_18.scope||_1.global,res,_18);
}else{
console.error("CdfStore Error:",res);
}
return res;
}
this.cdfDoc=res;
var _27=this._getItems(this.cdfDoc,_26);
if(_27&&_27.length>0){
_27=_1d(_27,_18);
}else{
_27=_1d([],_18);
}
return _27;
}else{
var dfd=this._loadCDF();
dfd.addCallbacks(_1.hitch(this,function(_28){
var _29=this._getItems(this.cdfDoc,_26);
if(_29&&_29.length>0){
_1d(_29,_18);
}else{
_1d([],_18);
}
}),_1.hitch(this,function(err){
_1a(err,_18);
}));
return dfd;
}
},_loadCDF:function(){
var dfd=new _1.Deferred();
if(this.cdfDoc){
if(this.mode==_2.data.SYNC_MODE){
return this.cdfDoc;
}else{
setTimeout(_1.hitch(this,function(){
dfd.callback(this.cdfDoc);
}),0);
return dfd;
}
}
this.cdfDoc=jsx3.xml.CDF.Document.newDocument();
this.cdfDoc.subscribe("response",this,function(evt){
dfd.callback(this.cdfDoc);
});
this.cdfDoc.subscribe("error",this,function(err){
dfd.errback(err);
});
this.cdfDoc.setAsync(!this.mode);
if(this.url){
this.cdfDoc.load(this.url);
}else{
if(this.xmlStr){
this.cdfDoc.loadXML(this.xmlStr);
if(this.cdfDoc.getError().code){
return new Error(this.cdfDoc.getError().description);
}
}
}
if(this.mode==_2.data.SYNC_MODE){
return this.cdfDoc;
}else{
return dfd;
}
},_getItems:function(_2a,_2b){
var itr=_2a.selectNodes(_2b.query,false,1);
var _2c=[];
while(itr.hasNext()){
_2c.push(itr.next());
}
return _2c;
},close:function(_2d){
},newItem:function(_2e,_2f){
_2e=(_2e||{});
if(_2e.tagName){
if(_2e.tagName!="record"){
console.warn("Only record inserts are supported at this time");
}
delete _2e.tagName;
}
_2e.jsxid=_2e.jsxid||this.cdfDoc.getKey();
if(this.isItem(_2f)){
_2f=this.getIdentity(_2f);
}
var _30=this.cdfDoc.insertRecord(_2e,_2f);
this._makeDirty(_30);
return _30;
},deleteItem:function(_31){
this.cdfDoc.deleteRecord(this.getIdentity(_31));
this._makeDirty(_31);
return true;
},setValue:function(_32,_33,_34){
this._makeDirty(_32);
_32.setAttribute(_33,_34);
return true;
},setValues:function(_35,_36,_37){
this._makeDirty(_35);
console.warn("cdfStore.setValues only partially implemented.");
return _35.setAttribute(_36,_37);
},unsetAttribute:function(_38,_39){
this._makeDirty(_38);
_38.removeAttribute(_39);
return true;
},revert:function(){
delete this.cdfDoc;
this._modifiedItems={};
return true;
},isDirty:function(_3a){
if(_3a){
return !!this._modifiedItems[this.getIdentity(_3a)];
}else{
var _3b=false;
for(var nm in this._modifiedItems){
_3b=true;
break;
}
return _3b;
}
},_makeDirty:function(_3c){
var id=this.getIdentity(_3c);
this._modifiedItems[id]=_3c;
},_makeXmlString:function(obj){
var _3d=function(obj,_3e){
var _3f="";
var nm;
if(_1.isArray(obj)){
for(var i=0;i<obj.length;i++){
_3f+=_3d(obj[i],_3e);
}
}else{
if(_1.isObject(obj)){
_3f+="<"+_3e+" ";
for(nm in obj){
if(!_1.isObject(obj[nm])){
_3f+=nm+"=\""+obj[nm]+"\" ";
}
}
_3f+=">";
for(nm in obj){
if(_1.isObject(obj[nm])){
_3f+=_3d(obj[nm],nm);
}
}
_3f+="</"+_3e+">";
}
}
return _3f;
};
return _3d(obj,"data");
},getIdentity:function(_40){
return this.getValue(_40,this.identity);
},getIdentityAttributes:function(_41){
return [this.identity];
},fetchItemByIdentity:function(_42){
if(_1.isString(_42)){
var id=_42;
_42={query:"//record[@jsxid='"+id+"']",mode:_2.data.SYNC_MODE};
}else{
if(_42){
_42.query="//record[@jsxid='"+_42.identity+"']";
}
if(!_42.mode){
_42.mode=this.mode;
}
}
_42.byId=true;
return this.fetch(_42);
},byId:function(_43){
}});
});
