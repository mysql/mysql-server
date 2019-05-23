//>>built
define("dijit/form/_SearchMixin",["dojo/data/util/filter","dojo/_base/declare","dojo/_base/event","dojo/keys","dojo/_base/lang","dojo/query","dojo/sniff","dojo/string","dojo/when","../registry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dijit.form._SearchMixin",null,{pageSize:Infinity,store:null,fetchProperties:{},query:{},list:"",_setListAttr:function(_b){
this._set("list",_b);
},searchDelay:200,searchAttr:"name",queryExpr:"${0}*",ignoreCase:true,_abortQuery:function(){
if(this.searchTimer){
this.searchTimer=this.searchTimer.remove();
}
if(this._queryDeferHandle){
this._queryDeferHandle=this._queryDeferHandle.remove();
}
if(this._fetchHandle){
if(this._fetchHandle.abort){
this._cancelingQuery=true;
this._fetchHandle.abort();
this._cancelingQuery=false;
}
if(this._fetchHandle.cancel){
this._cancelingQuery=true;
this._fetchHandle.cancel();
this._cancelingQuery=false;
}
this._fetchHandle=null;
}
},_processInput:function(_c){
if(this.disabled||this.readOnly){
return;
}
var _d=_c.charOrCode;
var _e=false;
this._prev_key_backspace=false;
switch(_d){
case _4.DELETE:
case _4.BACKSPACE:
this._prev_key_backspace=true;
this._maskValidSubsetError=true;
_e=true;
break;
default:
_e=typeof _d=="string"||_d==229;
}
if(_e){
if(!this.store){
this.onSearch();
}else{
this.searchTimer=this.defer("_startSearchFromInput",1);
}
}
},onSearch:function(){
},_startSearchFromInput:function(){
this._startSearch(this.focusNode.value);
},_startSearch:function(_f){
this._abortQuery();
var _10=this,_6=_5.clone(this.query),_11={start:0,count:this.pageSize,queryOptions:{ignoreCase:this.ignoreCase,deep:true}},qs=_8.substitute(this.queryExpr,[_f.replace(/([\\\*\?])/g,"\\$1")]),q,_12=function(){
var _13=_10._fetchHandle=_10.store.query(_6,_11);
if(_10.disabled||_10.readOnly||(q!==_10._lastQuery)){
return;
}
_9(_13,function(res){
_10._fetchHandle=null;
if(!_10.disabled&&!_10.readOnly&&(q===_10._lastQuery)){
_9(_13.total,function(_14){
res.total=_14;
var _15=_10.pageSize;
if(isNaN(_15)||_15>res.total){
_15=res.total;
}
res.nextPage=function(_16){
_11.direction=_16=_16!==false;
_11.count=_15;
if(_16){
_11.start+=res.length;
if(_11.start>=res.total){
_11.count=0;
}
}else{
_11.start-=_15;
if(_11.start<0){
_11.count=Math.max(_15+_11.start,0);
_11.start=0;
}
}
if(_11.count<=0){
res.length=0;
_10.onSearch(res,_6,_11);
}else{
_12();
}
};
_10.onSearch(res,_6,_11);
});
}
},function(err){
_10._fetchHandle=null;
if(!_10._cancelingQuery){
console.error(_10.declaredClass+" "+err.toString());
}
});
};
_5.mixin(_11,this.fetchProperties);
if(this.store._oldAPI){
q=qs;
}else{
q=_1.patternToRegExp(qs,this.ignoreCase);
q.toString=function(){
return qs;
};
}
this._lastQuery=_6[this.searchAttr]=q;
this._queryDeferHandle=this.defer(_12,this.searchDelay);
},constructor:function(){
this.query={};
this.fetchProperties={};
},postMixInProperties:function(){
if(!this.store){
var _17=this.list;
if(_17){
this.store=_a.byId(_17);
}
}
this.inherited(arguments);
}});
});
