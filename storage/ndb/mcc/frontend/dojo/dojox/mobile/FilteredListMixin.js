//>>built
define("dojox/mobile/FilteredListMixin",["require","dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/aspect","dijit/registry","./SearchBox","./ScrollableView","./viewRegistry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
return _3("dojox.mobile.FilteredListMixin",null,{filterBoxRef:null,placeHolder:"",filterBoxVisible:true,_filterBox:null,_createdFilterBox:null,_createdScrollableView:null,startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
if(this.filterBoxRef){
this._filterBox=_9.byId(this.filterBoxRef);
if(this._filterBox&&this._filterBox.isInstanceOf(_a)){
this._filterBox.set("searchAttr",this.labelProperty?this.labelProperty:"label");
if(!this._filterBox.placeHolder){
this._filterBox.set("placeHolder",this.placeHolder);
}
this._filterBox.on("search",_4.hitch(this,"_onFilter"));
}else{
throw new Error("Cannot find a widget of type dojox/mobile/SearchBox or subclass "+"at the specified filterBoxRef: "+this.filterBoxRef);
}
}else{
this._filterBox=new _a({searchAttr:this.labelProperty?this.labelProperty:"label",ignoreCase:true,incremental:true,onSearch:_4.hitch(this,"_onFilter"),selectOnClick:true,placeHolder:this.placeHolder});
this._createdFilterBox=this._filterBox;
this._createdScrollableView=new _b();
var _d=this.domNode,_e=this.domNode.parentNode;
_e.replaceChild(this._createdScrollableView.domNode,this.domNode);
_7.place(_d,this._createdScrollableView.containerNode);
var _f=_7.create("div");
_7.place(this._createdFilterBox.domNode,_f);
_7.place(_f,this._createdScrollableView.domNode,"before");
if(this.filterBoxClass){
_6.add(_f,this.filterBoxClass);
}
this._createdFilterBox.startup();
this._createdScrollableView.startup();
this._createdScrollableView.resize();
}
var sv=_c.getEnclosingScrollable(this.domNode);
if(sv){
this.connect(sv,"onFlickAnimationEnd",_4.hitch(this,function(){
if(!this._filterBox.focusNode.value){
this._previousUnfilteredScrollPos=sv.getPos();
}
}));
}
if(!this.store){
this._createStore(this._initStore);
}else{
this._initStore();
}
},_setFilterBoxVisibleAttr:function(_10){
this._set("filterBoxVisible",_10);
if(this._filterBox&&this._filterBox.domNode){
this._filterBox.domNode.style.display=_10?"":"none";
}
},_setPlaceHolderAttr:function(_11){
this._set("placeHolder",_11);
if(this._filterBox){
this._filterBox.set("placeHolder",_11);
}
},getFilterBox:function(){
return this._filterBox;
},getScrollableView:function(){
return this._createdScrollableView;
},_initStore:function(){
var _12=this.store;
if(!_12.get||!_12.query){
_1(["dojo/store/DataStore"],_4.hitch(this,function(_13){
_12=new _13({store:_12});
this._filterBox.store=_12;
}));
}else{
this._filterBox.store=_12;
}
},_createStore:function(_14){
_1(["./_StoreListMixin","dojo/store/Memory"],_4.hitch(this,function(_15,_16){
_3.safeMixin(this,new _15());
this.append=true;
this.createListItem=function(_17){
return _17.listItem;
};
_8.before(this,"generateList",function(){
_2.forEach(this.getChildren(),function(_18){
_18.domNode.parentNode.removeChild(_18.domNode);
});
});
var _19=[];
var _1a=null;
_2.forEach(this.getChildren(),function(_1b){
_1a=_1b.label?_1b.label:(_1b.domNode.innerText||_1b.domNode.textContent);
_19.push({label:_1a,listItem:_1b});
});
var _1c={items:_19};
var _1d=new _16({idProperty:"label",data:_1c});
this.store=null;
this.query={};
this.setStore(_1d,this.query,this.queryOptions);
_4.hitch(this,_14)();
}));
},_onFilter:function(_1e,_1f,_20){
if(this.onFilter(_1e,_1f,_20)===false){
return;
}
this.setQuery(_1f);
var sv=_c.getEnclosingScrollable(this.domNode);
if(sv){
sv.scrollTo(this._filterBox.focusNode.value?{x:0,y:0}:this._previousUnfilteredScrollPos||{x:0,y:0});
}
},onFilter:function(){
},destroy:function(_21){
this.inherited(arguments);
if(this._createdFilterBox){
this._createdFilterBox.destroy(_21);
this._createdFilterBox=null;
}
if(this._createdScrollableView){
this._createdScrollableView.destroy(_21);
this._createdScrollableView=null;
}
}});
});
