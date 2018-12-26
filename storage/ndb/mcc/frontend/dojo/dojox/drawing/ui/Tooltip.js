//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/drawing/plugins/_Plugin"],function(_1,_2,_3){
_2.provide("dojox.drawing.ui.Tooltip");
_2.require("dojox.drawing.plugins._Plugin");
(function(){
var _4=null;
var _5=_3.drawing.util.oo.declare(_3.drawing.plugins._Plugin,function(_6){
this.createDom();
},{show:function(_7,_8){
this.domNode.innerHTML=_8;
var dx=30;
var px=_7.data.x+_7.data.width;
var py=_7.data.y+_7.data.height;
var x=px+this.mouse.origin.x+dx;
var y=py+this.mouse.origin.y+dx;
_2.style(this.domNode,{display:"inline",left:x+"px",top:y+"px"});
var _9=_2.marginBox(this.domNode);
this.createShape(x-this.mouse.origin.x,y-this.mouse.origin.y,_9.w,_9.h);
},createShape:function(x,y,w,h){
this.balloon&&this.balloon.destroy();
var r=5,x2=x+w,y2=y+h,_a=[];
var _b=function(){
for(var i=0;i<arguments.length;i++){
_a.push(arguments[i]);
}
};
_b({x:x,y:y+5},{t:"Q",x:x,y:y},{x:x+r,y:y});
_b({t:"L",x:x2-r,y:y});
_b({t:"Q",x:x2,y:y},{x:x2,y:y+r});
_b({t:"L",x:x2,y:y2-r});
_b({t:"Q",x:x2,y:y2},{x:x2-r,y:y2});
_b({t:"L",x:x+r,y:y2});
_b({t:"Q",x:x,y:y2},{x:x,y:y2-r});
_b({t:"L",x:x,y:y+r});
this.balloon=this.drawing.addUI("path",{points:_a});
},createDom:function(){
this.domNode=_2.create("span",{"class":"drawingTooltip"},document.body);
_2.style(this.domNode,{display:"none",position:"absolute"});
}});
_3.drawing.ui.Tooltip=_3.drawing.util.oo.declare(_3.drawing.plugins._Plugin,function(_c){
if(!_4){
_4=new _5(_c);
}
if(_c.stencil){
}else{
if(this.button){
this.connect(this.button,"onOver",this,"onOver");
this.connect(this.button,"onOut",this,"onOut");
}
}
},{width:300,height:200,onOver:function(){
_4.show(this.button,this.data.text);
},onOut:function(){
}});
_3.drawing.register({name:"dojox.drawing.ui.Tooltip"},"stencil");
})();
});
