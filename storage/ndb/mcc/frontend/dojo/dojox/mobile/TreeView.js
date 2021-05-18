//>>built
define("dojox/mobile/TreeView",["dojo/_base/kernel","dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-construct","dijit/registry","./Heading","./ListItem","./ProgressIndicator","./RoundRectList","./ScrollableView","./viewRegistry","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/TreeView"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
_1.experimental("dojox.mobile.TreeView");
var _10=_3(_e("dojo-bidi")?"dojox.mobile.NonBidiTreeView":"dojox.mobile.TreeView",_c,{postCreate:function(){
this._load();
this.inherited(arguments);
},_customizeListItem:function(_11){
},_load:function(){
this.model.getRoot(_4.hitch(this,function(_12){
var _13=this;
var _14=new _b();
var _15={};
var _16={label:_13.model.rootLabel,moveTo:"#",onClick:function(){
_13.handleClick(this);
},item:_12};
this._customizeListItem(_16);
var _17=new _9(_16);
_14.addChild(_17);
this.addChild(_14);
}));
},handleClick:function(li){
var _18="view_";
if(li.item[this.model.newItemIdAttr]){
_18+=li.item[this.model.newItemIdAttr];
}else{
_18+="rootView";
}
_18=_18.replace("/","_");
if(_7.byId(_18)){
_7.byNode(li.domNode).transitionTo(_18);
return;
}
var _19=_a.getInstance();
_5.body().appendChild(_19.domNode);
_19.start();
this.model.getChildren(li.item,_4.hitch(this,function(_1a){
var _1b=this;
var _1c=new _b();
_2.forEach(_1a,function(_1d,i){
var _1e={item:_1d,label:_1d[_1b.model.store.label],transition:"slide"};
_1b._customizeListItem(_1e);
if(_1b.model.mayHaveChildren(_1d)){
_1e.moveTo="#";
_1e.onClick=function(){
_1b.handleClick(this);
};
}
var _1f=new _9(_1e);
_1c.addChild(_1f);
});
var _20=new _8({label:"Dynamic View",back:"Back",moveTo:_d.getEnclosingView(li.domNode).id,dir:this.isLeftToRight()?"ltr":"rtl"});
var _21=_c({id:_18,dir:this.isLeftToRight()?"ltr":"rtl"},_6.create("div",null,_5.body()));
_21.addChild(_20);
_21.addChild(_1c);
_21.startup();
_19.stop();
_7.byNode(li.domNode).transitionTo(_21.id);
}));
}});
return _e("dojo-bidi")?_3("dojox.mobile.TreeView",[_10,_f]):_10;
});
