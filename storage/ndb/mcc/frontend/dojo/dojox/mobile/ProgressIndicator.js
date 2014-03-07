//>>built
define("dojox/mobile/ProgressIndicator",["dojo/_base/config","dojo/_base/declare","dojo/dom-construct","dojo/dom-style","dojo/has"],function(_1,_2,_3,_4,_5){
var _6=_2("dojox.mobile.ProgressIndicator",null,{interval:100,colors:["#C0C0C0","#C0C0C0","#C0C0C0","#C0C0C0","#C0C0C0","#C0C0C0","#B8B9B8","#AEAFAE","#A4A5A4","#9A9A9A","#8E8E8E","#838383"],constructor:function(){
this._bars=[];
this.domNode=_3.create("DIV");
this.domNode.className="mblProgContainer";
if(_1["mblAndroidWorkaround"]!==false&&_5("android")>=2.2&&_5("android")<3){
_4.set(this.domNode,"webkitTransform","translate3d(0,0,0)");
}
this.spinnerNode=_3.create("DIV",null,this.domNode);
for(var i=0;i<this.colors.length;i++){
var _7=_3.create("DIV",{className:"mblProg mblProg"+i},this.spinnerNode);
this._bars.push(_7);
}
},start:function(){
if(this.imageNode){
var _8=this.imageNode;
var l=Math.round((this.domNode.offsetWidth-_8.offsetWidth)/2);
var t=Math.round((this.domNode.offsetHeight-_8.offsetHeight)/2);
_8.style.margin=t+"px "+l+"px";
return;
}
var _9=0;
var _a=this;
var n=this.colors.length;
this.timer=setInterval(function(){
_9--;
_9=_9<0?n-1:_9;
var c=_a.colors;
for(var i=0;i<n;i++){
var _b=(_9+i)%n;
_a._bars[i].style.backgroundColor=c[_b];
}
},this.interval);
},stop:function(){
if(this.timer){
clearInterval(this.timer);
}
this.timer=null;
if(this.domNode.parentNode){
this.domNode.parentNode.removeChild(this.domNode);
}
},setImage:function(_c){
if(_c){
this.imageNode=_3.create("IMG",{src:_c},this.domNode);
this.spinnerNode.style.display="none";
}else{
if(this.imageNode){
this.domNode.removeChild(this.imageNode);
this.imageNode=null;
}
this.spinnerNode.style.display="";
}
}});
_6._instance=null;
_6.getInstance=function(){
if(!_6._instance){
_6._instance=new _6();
}
return _6._instance;
};
return _6;
});
