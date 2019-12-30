//>>built
define("dojox/drawing/stencil/Text",["dojo","../util/oo","./_Base","../manager/_registry","../util/typeset"],function(_1,oo,_2,_3,_4){
var _5=oo.declare(_2,function(_6){
},{type:"dojox.drawing.stencil.Text",anchorType:"none",baseRender:true,align:"start",valign:"top",_lineHeight:1,typesetter:function(_7){
this._rawText=_7;
return _4.convertLaTeX(_7);
},setText:function(_8){
if(this.enabled){
_8=this.typesetter(_8);
}
this._text=_8;
this._textArray=[];
this.created&&this.render(_8);
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
},render:function(_9){
this.remove(this.shape,this.hit);
!this.annotation&&this.renderHit&&this._renderOutline();
if(_9!=undefined){
this._text=_9;
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
_1.forEach(this._textArray,function(_a,i){
var tb=this.shape.createText({x:x,y:y+(h*i),text:unescape(_a),align:this.align}).setFont(this.style.currentText).setFill(this.style.currentText.color);
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
},makeFit:function(_b,w){
var _c=_1.create("span",{innerHTML:_b,id:"foo"},document.body);
var sz=1;
_1.style(_c,"fontSize",sz+"px");
var _d=30;
while(_1.marginBox(_c).w<w){
sz++;
_1.style(_c,"fontSize",sz+"px");
if(_d--<=0){
break;
}
}
sz--;
var _e=_1.marginBox(_c);
_1.destroy(_c);
return {size:sz,box:_e};
}});
_1.setObject("dojox.drawing.stencil.Text",_5);
_3.register({name:"dojox.drawing.stencil.Text"},"stencil");
return _5;
});
