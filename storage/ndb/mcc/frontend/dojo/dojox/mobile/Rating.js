//>>built
define("dojox/mobile/Rating",["dojo/_base/declare","dojo/_base/lang","dojo/dom-construct","dijit/_WidgetBase","./iconUtils"],function(_1,_2,_3,_4,_5){
return _1("dojox.mobile.Rating",_4,{image:"",numStars:5,value:0,alt:"",baseClass:"mblRating",buildRendering:function(){
this.inherited(arguments);
this.domNode.style.display="inline-block";
var _6=this.imgNode=_3.create("img");
this.connect(_6,"onload",_2.hitch(this,function(){
this.set("value",this.value);
}));
_5.createIcon(this.image,null,_6);
},_setValueAttr:function(_7){
this._set("value",_7);
var h=this.imgNode.height;
if(h==0){
return;
}
_3.empty(this.domNode);
var i,_8,w=this.imgNode.width/3;
for(i=0;i<this.numStars;i++){
if(i<=_7-1){
_8=0;
}else{
if(i>=_7){
_8=w;
}else{
_8=w*2;
}
}
var _9=_3.create("div",{style:{"float":"left"}},this.domNode);
_5.createIcon(this.image,"0,"+_8+","+w+","+h,null,this.alt,_9);
}
}});
});
