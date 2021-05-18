//>>built
define("dojox/data/SnapLogicStore",["dojo","dojox","dojo/io/script","dojo/data/util/sorter"],function(_1,_2){
return _1.declare("dojox.data.SnapLogicStore",null,{Parts:{DATA:"data",COUNT:"count"},url:"",constructor:function(_3){
if(_3.url){
this.url=_3.url;
}
this._parameters=_3.parameters;
},_assertIsItem:function(_4){
if(!this.isItem(_4)){
throw new Error("dojox.data.SnapLogicStore: a function was passed an item argument that was not an item");
}
},_assertIsAttribute:function(_5){
if(typeof _5!=="string"){
throw new Error("dojox.data.SnapLogicStore: a function was passed an attribute argument that was not an attribute name string");
}
},getFeatures:function(){
return {"dojo.data.api.Read":true};
},getValue:function(_6,_7,_8){
this._assertIsItem(_6);
this._assertIsAttribute(_7);
var i=_1.indexOf(_6.attributes,_7);
if(i!==-1){
return _6.values[i];
}
return _8;
},getAttributes:function(_9){
this._assertIsItem(_9);
return _9.attributes;
},hasAttribute:function(_a,_b){
this._assertIsItem(_a);
this._assertIsAttribute(_b);
for(var i=0;i<_a.attributes.length;++i){
if(_b==_a.attributes[i]){
return true;
}
}
return false;
},isItemLoaded:function(_c){
return this.isItem(_c);
},loadItem:function(_d){
},getLabel:function(_e){
return undefined;
},getLabelAttributes:function(_f){
return null;
},containsValue:function(_10,_11,_12){
return this.getValue(_10,_11)===_12;
},getValues:function(_13,_14){
this._assertIsItem(_13);
this._assertIsAttribute(_14);
var i=_1.indexOf(_13.attributes,_14);
if(i!==-1){
return [_13.values[i]];
}
return [];
},isItem:function(_15){
if(_15&&_15._store===this){
return true;
}
return false;
},close:function(_16){
},_fetchHandler:function(_17){
var _18=_17.scope||_1.global;
if(_17.onBegin){
_17.onBegin.call(_18,_17._countResponse[0],_17);
}
if(_17.onItem||_17.onComplete){
var _19=_17._dataResponse;
if(!_19.length){
_17.onError.call(_18,new Error("dojox.data.SnapLogicStore: invalid response of length 0"),_17);
return;
}else{
if(_17.query!="record count"){
var _1a=_19.shift();
var _1b=[];
for(var i=0;i<_19.length;++i){
if(_17._aborted){
break;
}
_1b.push({attributes:_1a,values:_19[i],_store:this});
}
if(_17.sort&&!_17._aborted){
_1b.sort(_1.data.util.sorter.createSortFunction(_17.sort,self));
}
}else{
_1b=[({attributes:["count"],values:_19,_store:this})];
}
}
if(_17.onItem){
for(var i=0;i<_1b.length;++i){
if(_17._aborted){
break;
}
_17.onItem.call(_18,_1b[i],_17);
}
_1b=null;
}
if(_17.onComplete&&!_17._aborted){
_17.onComplete.call(_18,_1b,_17);
}
}
},_partHandler:function(_1c,_1d,_1e){
if(_1e instanceof Error){
if(_1d==this.Parts.DATA){
_1c._dataHandle=null;
}else{
_1c._countHandle=null;
}
_1c._aborted=true;
if(_1c.onError){
_1c.onError.call(_1c.scope,_1e,_1c);
}
}else{
if(_1c._aborted){
return;
}
if(_1d==this.Parts.DATA){
_1c._dataResponse=_1e;
}else{
_1c._countResponse=_1e;
}
if((!_1c._dataHandle||_1c._dataResponse!==null)&&(!_1c._countHandle||_1c._countResponse!==null)){
this._fetchHandler(_1c);
}
}
},fetch:function(_1f){
_1f._countResponse=null;
_1f._dataResponse=null;
_1f._aborted=false;
_1f.abort=function(){
if(!_1f._aborted){
_1f._aborted=true;
if(_1f._dataHandle&&_1f._dataHandle.cancel){
_1f._dataHandle.cancel();
}
if(_1f._countHandle&&_1f._countHandle.cancel){
_1f._countHandle.cancel();
}
}
};
if(_1f.onItem||_1f.onComplete){
var _20=this._parameters||{};
if(_1f.start){
if(_1f.start<0){
throw new Error("dojox.data.SnapLogicStore: request start value must be 0 or greater");
}
_20["sn.start"]=_1f.start+1;
}
if(_1f.count){
if(_1f.count<0){
throw new Error("dojox.data.SnapLogicStore: request count value 0 or greater");
}
_20["sn.limit"]=_1f.count;
}
_20["sn.content_type"]="application/javascript";
var _21=this;
var _22=function(_23,_24){
if(_23 instanceof Error){
_21._fetchHandler(_23,_1f);
}
};
var _25={url:this.url,content:_20,timeout:60000,callbackParamName:"sn.stream_header",handle:_1.hitch(this,"_partHandler",_1f,this.Parts.DATA)};
_1f._dataHandle=_1.io.script.get(_25);
}
if(_1f.onBegin){
var _20={};
_20["sn.count"]="records";
_20["sn.content_type"]="application/javascript";
var _25={url:this.url,content:_20,timeout:60000,callbackParamName:"sn.stream_header",handle:_1.hitch(this,"_partHandler",_1f,this.Parts.COUNT)};
_1f._countHandle=_1.io.script.get(_25);
}
return _1f;
}});
});
