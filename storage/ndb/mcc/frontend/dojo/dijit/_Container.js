//>>built
define("dijit/_Container",["dojo/_base/array","dojo/_base/declare","dojo/dom-construct","./registry"],function(_1,_2,_3,_4){
return _2("dijit._Container",null,{buildRendering:function(){
this.inherited(arguments);
if(!this.containerNode){
this.containerNode=this.domNode;
}
},addChild:function(_5,_6){
var _7=this.containerNode;
if(_6&&typeof _6=="number"){
var _8=this.getChildren();
if(_8&&_8.length>=_6){
_7=_8[_6-1].domNode;
_6="after";
}
}
_3.place(_5.domNode,_7,_6);
if(this._started&&!_5._started){
_5.startup();
}
},removeChild:function(_9){
if(typeof _9=="number"){
_9=this.getChildren()[_9];
}
if(_9){
var _a=_9.domNode;
if(_a&&_a.parentNode){
_a.parentNode.removeChild(_a);
}
}
},hasChildren:function(){
return this.getChildren().length>0;
},_getSiblingOfChild:function(_b,_c){
var _d=_b.domNode,_e=(_c>0?"nextSibling":"previousSibling");
do{
_d=_d[_e];
}while(_d&&(_d.nodeType!=1||!_4.byNode(_d)));
return _d&&_4.byNode(_d);
},getIndexOfChild:function(_f){
return _1.indexOf(this.getChildren(),_f);
}});
});
