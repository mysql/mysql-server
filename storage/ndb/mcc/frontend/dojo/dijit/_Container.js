//>>built
define("dijit/_Container",["dojo/_base/array","dojo/_base/declare","dojo/dom-construct"],function(_1,_2,_3){
return _2("dijit._Container",null,{buildRendering:function(){
this.inherited(arguments);
if(!this.containerNode){
this.containerNode=this.domNode;
}
},addChild:function(_4,_5){
var _6=this.containerNode;
if(_5&&typeof _5=="number"){
var _7=this.getChildren();
if(_7&&_7.length>=_5){
_6=_7[_5-1].domNode;
_5="after";
}
}
_3.place(_4.domNode,_6,_5);
if(this._started&&!_4._started){
_4.startup();
}
},removeChild:function(_8){
if(typeof _8=="number"){
_8=this.getChildren()[_8];
}
if(_8){
var _9=_8.domNode;
if(_9&&_9.parentNode){
_9.parentNode.removeChild(_9);
}
}
},hasChildren:function(){
return this.getChildren().length>0;
},_getSiblingOfChild:function(_a,_b){
var _c=this.getChildren(),_d=_1.indexOf(this.getChildren(),_a);
return _c[_d+_b];
},getIndexOfChild:function(_e){
return _1.indexOf(this.getChildren(),_e);
}});
});
