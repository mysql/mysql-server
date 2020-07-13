//>>built
define("dojox/mobile/FixedSplitter",["dojo/_base/array","dojo/_base/declare","dojo/_base/window","dojo/dom-class","dojo/dom-geometry","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","dojo/has","./common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.mobile.FixedSplitter",[_8,_7,_6],{orientation:"H",variablePane:-1,screenSizeAware:false,screenSizeAwareClass:"dojox/mobile/ScreenSizeAware",baseClass:"mblFixedSplitter",startup:function(){
if(this._started){
return;
}
_4.add(this.domNode,this.baseClass+this.orientation);
var _a=this.getParent(),f;
if(!_a||!_a.resize){
var _b=this;
f=function(){
_b.defer(function(){
_b.resize();
});
};
}
if(this.screenSizeAware){
require([this.screenSizeAwareClass],function(_c){
_c.getInstance();
f&&f();
});
}else{
f&&f();
}
this.inherited(arguments);
},resize:function(){
var _d=_5.getPadExtents(this.domNode).t;
var wh=this.orientation==="H"?"w":"h",tl=this.orientation==="H"?"l":"t",_e={},_f={},i,c,h,a=[],_10=0,_11=0,_12=_1.filter(this.domNode.childNodes,function(_13){
return _13.nodeType==1;
}),idx=this.variablePane==-1?_12.length-1:this.variablePane;
for(i=0;i<_12.length;i++){
if(i!=idx){
a[i]=_5.getMarginBox(_12[i])[wh];
_11+=a[i];
}
}
if(this.orientation=="V"){
if(this.domNode.parentNode.tagName=="BODY"){
if(_1.filter(_3.body().childNodes,function(_14){
return _14.nodeType==1;
}).length==1){
h=(_3.global.innerHeight||_3.doc.documentElement.clientHeight);
}
}
}
var l=(h||_5.getMarginBox(this.domNode)[wh])-_11;
if(this.orientation==="V"){
l-=_d;
}
_f[wh]=a[idx]=l;
c=_12[idx];
_5.setMarginBox(c,_f);
c.style[this.orientation==="H"?"height":"width"]="";
if(_9("dojo-bidi")&&(this.orientation=="H"&&!this.isLeftToRight())){
_10=l;
for(i=_12.length-1;i>=0;i--){
c=_12[i];
_e[tl]=l-_10;
_5.setMarginBox(c,_e);
if(this.orientation==="H"){
c.style.top=_d+"px";
}else{
c.style.left="";
}
_10-=a[i];
}
}else{
if(this.orientation==="V"){
_10=_10?_10+_d:_d;
}
for(i=0;i<_12.length;i++){
c=_12[i];
_e[tl]=_10;
_5.setMarginBox(c,_e);
if(this.orientation==="H"){
c.style.top=_d+"px";
}else{
c.style.left="";
}
_10+=a[i];
}
}
_1.forEach(this.getChildren(),function(_15){
if(_15.resize){
_15.resize();
}
});
},_setOrientationAttr:function(_16){
var s=this.baseClass;
_4.replace(this.domNode,s+_16,s+this.orientation);
this.orientation=_16;
if(this._started){
this.resize();
}
}});
});
