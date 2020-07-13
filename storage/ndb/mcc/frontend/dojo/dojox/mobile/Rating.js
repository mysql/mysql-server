//>>built
define("dojox/mobile/Rating",["dojo/_base/declare","dojo/_base/lang","dojo/dom-construct","dijit/_WidgetBase","./iconUtils","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/Rating"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_1(_6("dojo-bidi")?"dojox.mobile.NonBidiRating":"dojox.mobile.Rating",[_4],{image:"",numStars:5,value:0,alt:"",baseClass:"mblRating",buildRendering:function(){
this.inherited(arguments);
this.domNode.style.display="inline-block";
var _9=this.imgNode=_3.create("img");
this.connect(_9,"onload",_2.hitch(this,function(){
this.set("value",this.value);
}));
_5.createIcon(this.image,null,_9);
},_setValueAttr:function(_a){
this._set("value",_a);
var h=this.imgNode.height;
if(h==0){
return;
}
_3.empty(this.domNode);
var i,_b,w=this.imgNode.width/3;
for(i=0;i<this.numStars;i++){
if(i<=_a-1){
_b=0;
}else{
if(i>=_a){
_b=w;
}else{
_b=w*2;
}
}
var _c=_3.create("div",{style:{"float":"left"}},this.domNode);
if(!this.isLeftToRight()){
_c=this._setCustomTransform(_c);
}
_5.createIcon(this.image,"0,"+_b+","+w+","+h,null,this.alt,_c);
}
},_setCustomTransform:function(_d){
return _d;
}});
return _6("dojo-bidi")?_1("dojox.mobile.Rating",[_8,_7]):_8;
});
