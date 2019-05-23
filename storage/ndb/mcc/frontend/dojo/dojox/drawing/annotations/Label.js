//>>built
define("dojox/drawing/annotations/Label",["exports","dojo/_base/lang","../util/oo","../stencil/Text"],function(_1,_2,oo,_3){
_1.Label=oo.declare(_3,function(_4){
this.master=_4.stencil;
this.labelPosition=_4.labelPosition||"BR";
if(_2.isFunction(this.labelPosition)){
this.setLabel=this.setLabelCustom;
}
this.setLabel(_4.text||"");
this.connect(this.master,"onTransform",this,"setLabel");
this.connect(this.master,"destroy",this,"destroy");
if(this.style.labelSameColor){
this.connect(this.master,"attr",this,"beforeAttr");
}
},{_align:"start",drawingType:"label",setLabelCustom:function(_5){
var d=_2.hitch(this.master,this.labelPosition)();
this.setData({x:d.x,y:d.y,width:d.w||this.style.text.minWidth,height:d.h||this._lineHeight});
if(_5&&!_5.split){
_5=this.getText();
}
this.render(this.typesetter(_5));
},setLabel:function(_6){
var x,y,_7=this.master.getBounds();
if(/B/.test(this.labelPosition)){
y=_7.y2-this._lineHeight;
}else{
y=_7.y1;
}
if(/R/.test(this.labelPosition)){
x=_7.x2;
}else{
y=_7.y1;
this._align="end";
}
if(!this.labelWidth||(_6&&_6.split&&_6!=this.getText())){
this.setData({x:x,y:y,height:this._lineHeight,width:this.style.text.minWidth});
this.labelWidth=this.style.text.minWidth;
this.render(this.typesetter(_6));
}else{
this.setData({x:x,y:y,height:this.data.height,width:this.data.width});
this.render();
}
},beforeAttr:function(_8,_9){
if(_9!==undefined){
var k=_8;
_8={};
_8[k]=_9;
}
delete _8.x;
delete _8.y;
delete _8.width;
delete _8.height;
this.attr(_8);
!this.created&&this.render();
}});
});
