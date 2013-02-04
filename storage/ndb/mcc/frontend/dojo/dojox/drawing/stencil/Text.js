//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.stencil.Text");
_3.drawing.stencil.Text=_3.drawing.util.oo.declare(_3.drawing.stencil._Base,function(_4){
},{type:"dojox.drawing.stencil.Text",anchorType:"none",baseRender:true,align:"start",valign:"top",_lineHeight:1,typesetter:function(_5){
if(_3.drawing.util.typeset){
this._rawText=_5;
return _3.drawing.util.typeset.convertLaTeX(_5);
}
return _5;
},setText:function(_6){
if(this.enabled){
_6=this.typesetter(_6);
}
this._text=_6;
this._textArray=[];
this.created&&this.render(_6);
},getText:function(){
return this._rawText||this._text;
},dataToPoints:function(o){
o=o||this.data;
var w=o.width=="auto"?1:o.width;
var h=o.height||this._lineHeight;
this.points=[{x:o.x,y:o.y},{x:o.x+w,y:o.y},{x:o.x+w,y:o.y+h},{x:o.x,y:o.y+h}];
return this.points;
},pointsToData:function(p){
p=p||this.points;
var s=p[0];
var e=p[2];
this.data={x:s.x,y:s.y,width:e.x-s.x,height:e.y-s.y};
return this.data;
},render:function(_7){
this.remove(this.shape,this.hit);
!this.annotation&&this.renderHit&&this._renderOutline();
if(_7!=undefined){
this._text=_7;
this._textArray=this._text.split("\n");
}
var d=this.pointsToData();
var h=this._lineHeight;
var x=d.x+this.style.text.pad*2;
var y=d.y+this._lineHeight-(this.textSize*0.4);
if(this.valign=="middle"){
y-=h/2;
}
this.shape=this.container.createGroup();
_2.forEach(this._textArray,function(_8,i){
var tb=this.shape.createText({x:x,y:y+(h*i),text:unescape(_8),align:this.align}).setFont(this.style.currentText).setFill(this.style.currentText.color);
this._setNodeAtts(tb);
},this);
this._setNodeAtts(this.shape);
},_renderOutline:function(){
if(this.annotation){
return;
}
var d=this.pointsToData();
if(this.align=="middle"){
d.x-=d.width/2-this.style.text.pad*2;
}else{
if(this.align=="start"){
d.x+=this.style.text.pad;
}else{
if(this.align=="end"){
d.x-=d.width-this.style.text.pad*3;
}
}
}
if(this.valign=="middle"){
d.y-=(this._lineHeight)/2-this.style.text.pad;
}
this.hit=this.container.createRect(d).setStroke(this.style.currentHit).setFill(this.style.currentHit.fill);
this._setNodeAtts(this.hit);
this.hit.moveToBack();
},makeFit:function(_9,w){
var _a=_2.create("span",{innerHTML:_9,id:"foo"},document.body);
var sz=1;
_2.style(_a,"fontSize",sz+"px");
var _b=30;
while(_2.marginBox(_a).w<w){
sz++;
_2.style(_a,"fontSize",sz+"px");
if(_b--<=0){
break;
}
}
sz--;
var _c=_2.marginBox(_a);
_2.destroy(_a);
return {size:sz,box:_c};
}});
_3.drawing.register({name:"dojox.drawing.stencil.Text"},"stencil");
});
