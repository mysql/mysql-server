//>>built
define("dojox/mobile/TreeView",["dojo/_base/kernel","dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-construct","dijit/registry","./Heading","./ListItem","./ProgressIndicator","./RoundRectList","./ScrollableView","./viewRegistry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
_1.experimental("dojox.mobile.TreeView");
return _3("dojox.mobile.TreeView",_c,{postCreate:function(){
this._load();
this.inherited(arguments);
},_load:function(){
this.model.getRoot(_4.hitch(this,function(_e){
var _f=this;
var _10=new _b();
var _11={};
var _12=new _9({label:_f.model.rootLabel,moveTo:"#",onClick:function(){
_f.handleClick(this);
},item:_e});
_10.addChild(_12);
this.addChild(_10);
}));
},handleClick:function(li){
var _13="view_";
if(li.item[this.model.newItemIdAttr]){
_13+=li.item[this.model.newItemIdAttr];
}else{
_13+="rootView";
}
_13=_13.replace("/","_");
if(_7.byId(_13)){
_7.byNode(li.domNode).transitionTo(_13);
return;
}
var _14=_a.getInstance();
_5.body().appendChild(_14.domNode);
_14.start();
this.model.getChildren(li.item,_4.hitch(this,function(_15){
var _16=this;
var _17=new _b();
_2.forEach(_15,function(_18,i){
var _19={item:_18,label:_18[_16.model.store.label],transition:"slide"};
if(_16.model.mayHaveChildren(_18)){
_19.moveTo="#";
_19.onClick=function(){
_16.handleClick(this);
};
}
var _1a=new _9(_19);
_17.addChild(_1a);
});
var _1b=new _8({label:"Dynamic View",back:"Back",moveTo:_d.getEnclosingView(li.domNode).id});
var _1c=_c({id:_13},_6.create("div",null,_5.body()));
_1c.addChild(_1b);
_1c.addChild(_17);
_1c.startup();
_14.stop();
_7.byNode(li.domNode).transitionTo(_1c.id);
}));
}});
});
