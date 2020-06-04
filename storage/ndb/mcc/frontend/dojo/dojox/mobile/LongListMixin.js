//>>built
define("dojox/mobile/LongListMixin",["dojo/_base/array","dojo/_base/lang","dojo/_base/declare","dojo/sniff","dojo/dom-construct","dojo/dom-geometry","dijit/registry","./common","./viewRegistry"],function(_1,_2,_3,_4,_5,_6,_7,dm,_8){
return _3("dojox.mobile.LongListMixin",null,{pageSize:20,maxPages:5,unloadPages:1,startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
if(!this.editable){
this._sv=_8.getEnclosingScrollable(this.domNode);
if(this._sv){
this._items=this.getChildren();
this._clearItems();
this.containerNode=_5.create("div",null,this.domNode);
this.connect(this._sv,"scrollTo",_2.hitch(this,this._loadItems),true);
this.connect(this._sv,"slideTo",_2.hitch(this,this._loadItems),true);
this._topDiv=_5.create("div",null,this.domNode,"first");
this._bottomDiv=_5.create("div",null,this.domNode,"last");
this._reloadItems();
}
}
},_loadItems:function(_9){
var sv=this._sv;
var h=sv.getDim().d.h;
if(h<=0){
return;
}
var _a=-sv.getPos().y;
var _b=_9?-_9.y:_a;
var _c=Math.min(_a,_b),_d=Math.max(_a,_b)+h;
while(this._loadedYMin>_c&&this._addBefore()){
}
while(this._loadedYMax<_d&&this._addAfter()){
}
},_reloadItems:function(){
this._clearItems();
this._loadedYMin=this._loadedYMax=0;
this._firstIndex=0;
this._lastIndex=-1;
this._topDiv.style.height="0px";
this._loadItems();
},_clearItems:function(){
var c=this.containerNode;
_1.forEach(_7.findWidgets(c),function(_e){
c.removeChild(_e.domNode);
});
},_addBefore:function(){
var i,_f;
var _10=_6.getMarginBox(this.containerNode);
for(_f=0,i=this._firstIndex-1;_f<this.pageSize&&i>=0;_f++,i--){
var _11=this._items[i];
_5.place(_11.domNode,this.containerNode,"first");
if(!_11._started){
_11.startup();
}
this._firstIndex=i;
}
var _12=_6.getMarginBox(this.containerNode);
this._adjustTopDiv(_10,_12);
if(this._lastIndex-this._firstIndex>=this.maxPages*this.pageSize){
var _13=this.unloadPages*this.pageSize;
for(i=0;i<_13;i++){
this.containerNode.removeChild(this._items[this._lastIndex-i].domNode);
}
this._lastIndex-=_13;
_12=_6.getMarginBox(this.containerNode);
}
this._adjustBottomDiv(_12);
return _f==this.pageSize;
},_addAfter:function(){
var i,_14;
var _15=null;
for(_14=0,i=this._lastIndex+1;_14<this.pageSize&&i<this._items.length;_14++,i++){
var _16=this._items[i];
_5.place(_16.domNode,this.containerNode);
if(!_16._started){
_16.startup();
}
this._lastIndex=i;
}
if(this._lastIndex-this._firstIndex>=this.maxPages*this.pageSize){
_15=_6.getMarginBox(this.containerNode);
var _17=this.unloadPages*this.pageSize;
for(i=0;i<_17;i++){
this.containerNode.removeChild(this._items[this._firstIndex+i].domNode);
}
this._firstIndex+=_17;
}
var _18=_6.getMarginBox(this.containerNode);
if(_15){
this._adjustTopDiv(_15,_18);
}
this._adjustBottomDiv(_18);
return _14==this.pageSize;
},_adjustTopDiv:function(_19,_1a){
this._loadedYMin-=_1a.h-_19.h;
this._topDiv.style.height=this._loadedYMin+"px";
},_adjustBottomDiv:function(_1b){
var h=this._lastIndex>0?(this._loadedYMin+_1b.h)/this._lastIndex:0;
h*=this._items.length-1-this._lastIndex;
this._bottomDiv.style.height=h+"px";
this._loadedYMax=this._loadedYMin+_1b.h;
},_childrenChanged:function(){
if(!this._qs_timer){
this._qs_timer=this.defer(function(){
delete this._qs_timer;
this._reloadItems();
});
}
},resize:function(){
this.inherited(arguments);
if(this._items){
this._loadItems();
}
},addChild:function(_1c,_1d){
if(this._items){
if(typeof _1d=="number"){
this._items.splice(_1d,0,_1c);
}else{
this._items.push(_1c);
}
this._childrenChanged();
}else{
this.inherited(arguments);
}
},removeChild:function(_1e){
if(this._items){
this._items.splice(typeof _1e=="number"?_1e:this._items.indexOf(_1e),1);
this._childrenChanged();
}else{
this.inherited(arguments);
}
},getChildren:function(){
if(this._items){
return this._items.slice(0);
}else{
return this.inherited(arguments);
}
},_getSiblingOfChild:function(_1f,dir){
if(this._items){
var _20=this._items.indexOf(_1f);
if(_20>=0){
_20=dir>0?_20++:_20--;
}
return this._items[_20];
}else{
return this.inherited(arguments);
}
},generateList:function(_21){
if(this._items&&!this.append){
_1.forEach(this.getChildren(),function(_22){
_22.destroyRecursive();
});
this._items=[];
}
this.inherited(arguments);
}});
});
