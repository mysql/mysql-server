//>>built
define("dojox/drawing/ui/Tooltip",["dojo","../util/oo","../plugins/_Plugin","../manager/_registry"],function(_1,oo,_2,_3){
var _4=null;
var _5=oo.declare(_2,function(_6){
this.createDom();
},{show:function(_7,_8){
this.domNode.innerHTML=_8;
var dx=30;
var px=_7.data.x+_7.data.width;
var py=_7.data.y+_7.data.height;
var x=px+this.mouse.origin.x+dx;
var y=py+this.mouse.origin.y+dx;
_1.style(this.domNode,{display:"inline",left:x+"px",top:y+"px"});
var _9=_1.marginBox(this.domNode);
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
this.domNode=_1.create("span",{"class":"drawingTooltip"},document.body);
_1.style(this.domNode,{display:"none",position:"absolute"});
}});
var _c=oo.declare(_2,function(_d){
if(!_4){
_4=new _5(_d);
}
if(_d.stencil){
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
_1.setObject("dojox.drawing.ui.Tooltip",_c);
_3.register({name:"dojox.drawing.ui.Tooltip"},"stencil");
return _c;
});
