//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.tools.Rect");
_3.drawing.tools.Rect=_3.drawing.util.oo.declare(_3.drawing.stencil.Rect,function(){
},{draws:true,onDrag:function(_4){
var s=_4.start,e=_4;
var x=s.x<e.x?s.x:e.x,y=s.y<e.y?s.y:e.y,w=s.x<e.x?e.x-s.x:s.x-e.x,h=s.y<e.y?e.y-s.y:s.y-e.y;
if(this.keys.shift){
w=h=Math.max(w,h);
}
if(this.keys.alt){
x-=w;
y-=h;
w*=2;
h*=2;
x=Math.max(x,0);
y=Math.max(y,0);
}
this.setPoints([{x:x,y:y},{x:x+w,y:y},{x:x+w,y:y+h},{x:x,y:y+h}]);
this.render();
},onUp:function(_5){
if(this.created||!this._downOnCanvas){
return;
}
this._downOnCanvas=false;
if(!this.shape){
var s=_5.start;
var e=this.minimumSize*4;
this.setPoints([{x:s.x,y:s.y},{x:s.x+e,y:s.y},{x:s.x+e,y:s.y+e},{x:s.x,y:s.y+e}]);
this.render();
}else{
var o=this.data;
if(o.width<this.minimumSize&&o.height<this.minimumSize){
this.remove(this.shape,this.hit);
return;
}
}
this.onRender(this);
}});
_3.drawing.tools.Rect.setup={name:"dojox.drawing.tools.Rect",tooltip:"<span class=\"drawingTipTitle\">Rectangle Tool</span><br/>"+"<span class=\"drawingTipDesc\">SHIFT - constrain to square</span>",iconClass:"iconRect"};
_3.drawing.register(_3.drawing.tools.Rect.setup,"tool");
});
