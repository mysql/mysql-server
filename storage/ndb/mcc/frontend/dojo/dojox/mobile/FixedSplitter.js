//>>built
define("dojox/mobile/FixedSplitter",["dojo/_base/array","dojo/_base/declare","dojo/_base/window","dojo/dom-class","dojo/dom-geometry","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./FixedSplitterPane"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.mobile.FixedSplitter",[_8,_7,_6],{orientation:"H",buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef?this.srcNodeRef:_3.doc.createElement("DIV");
_4.add(this.domNode,"mblFixedSpliter");
},startup:function(){
if(this._started){
return;
}
var _a=_1.filter(this.domNode.childNodes,function(_b){
return _b.nodeType==1;
});
_1.forEach(_a,function(_c){
_4.add(_c,"mblFixedSplitterPane"+this.orientation);
},this);
this.inherited(arguments);
var _d=this;
setTimeout(function(){
var _e=_d.getParent&&_d.getParent();
if(!_e||!_e.resize){
_d.resize();
}
},0);
},resize:function(){
this.layout();
},layout:function(){
var sz=this.orientation=="H"?"w":"h";
var _f=_1.filter(this.domNode.childNodes,function(_10){
return _10.nodeType==1;
});
var _11=0;
for(var i=0;i<_f.length;i++){
_5.setMarginBox(_f[i],this.orientation=="H"?{l:_11}:{t:_11});
if(i<_f.length-1){
_11+=_5.getMarginBox(_f[i])[sz];
}
}
var h;
if(this.orientation=="V"){
if(this.domNode.parentNode.tagName=="BODY"){
if(_1.filter(_3.body().childNodes,function(_12){
return _12.nodeType==1;
}).length==1){
h=(_3.global.innerHeight||_3.doc.documentElement.clientHeight);
}
}
}
var l=(h||_5.getMarginBox(this.domNode)[sz])-_11;
var _13={};
_13[sz]=l;
_5.setMarginBox(_f[_f.length-1],_13);
_1.forEach(this.getChildren(),function(_14){
if(_14.resize){
_14.resize();
}
});
},addChild:function(_15,_16){
_4.add(_15.domNode,"mblFixedSplitterPane"+this.orientation);
this.inherited(arguments);
}});
});
