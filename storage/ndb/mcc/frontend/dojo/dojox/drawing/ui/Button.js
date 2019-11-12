//>>built
define("dojox/drawing/ui/Button",["dojo","../util/oo","../stencil/Rect","../stencil/Ellipse","../stencil/Text","../manager/_registry"],function(_1,oo,_2,_3,_4,_5){
var _6=oo.declare(function(_7){
_7.subShape=true;
_1.mixin(this,_7);
this.width=_7.data.width||_7.data.rx*2;
this.height=_7.data.height||_7.data.ry*2;
this.y=_7.data.y||_7.data.cy-_7.data.ry;
this.id=this.id||this.util.uid(this.type);
this.util.attr(this.container,"id",this.id);
if(this.callback){
this.hitched=_1.hitch(this.scope||window,this.callback,this);
}
_7.drawingType="ui";
if(_7.data.width&&_7.data.height){
this.shape=new _2(_7);
}else{
this.shape=new _3(_7);
}
var _8=function(s,p,v){
_1.forEach(["norm","over","down","selected"],function(nm){
s[nm].fill[p]=v;
});
};
_8(this.style.button,"y2",this.height+this.y);
_8(this.style.button,"y1",this.y);
if(_7.icon&&!_7.icon.text){
var _9=this.drawing.getConstructor(_7.icon.type);
var o=this.makeOptions(_7.icon);
o.data=_1.mixin(o.data,this.style.button.icon.norm);
if(o.data&&o.data.borderWidth===0){
o.data.fill=this.style.button.icon.norm.fill=o.data.color;
}else{
if(_7.icon.type=="line"||(_7.icon.type=="path"&&!_7.icon.closePath)){
this.style.button.icon.selected.color=this.style.button.icon.selected.fill;
}else{
}
}
this.icon=new _9(o);
}else{
if(_7.text||(_7.icon&&_7.icon.text)){
o=this.makeOptions(_7.text||_7.icon.text);
o.data.color=this.style.button.icon.norm.color;
this.style.button.icon.selected.color=this.style.button.icon.selected.fill;
this.icon=new _4(o);
this.icon.attr({height:this.icon._lineHeight,y:((this.height-this.icon._lineHeight)/2)+this.y});
}
}
var c=this.drawing.getConstructor(this.toolType);
if(c){
this.drawing.addUI("tooltip",{data:{text:c.setup.tooltip},button:this});
}
this.onOut();
},{callback:null,scope:null,hitched:null,toolType:"",onClick:function(_a){
},makeOptions:function(d,s){
s=s||1;
d=_1.clone(d);
var o={util:this.util,mouse:this.mouse,container:this.container,subShape:true};
if(typeof (d)=="string"){
o.data={x:this.data.x-5,y:this.data.y+2,width:this.data.width,height:this.data.height,text:d,makeFit:true};
}else{
if(d.points){
_1.forEach(d.points,function(pt){
pt.x=pt.x*this.data.width*0.01*s+this.data.x;
pt.y=pt.y*this.data.height*0.01*s+this.data.y;
},this);
o.data={};
for(var n in d){
if(n!="points"){
o.data[n]=d[n];
}
}
o.points=d.points;
}else{
for(n in d){
if(/x|width/.test(n)){
d[n]=d[n]*this.data.width*0.01*s;
}else{
if(/y|height/.test(n)){
d[n]=d[n]*this.data.height*0.01*s;
}
}
if(/x/.test(n)&&!/r/.test(n)){
d[n]+=this.data.x;
}else{
if(/y/.test(n)&&!/r/.test(n)){
d[n]+=this.data.y;
}
}
}
delete d.type;
o.data=d;
}
}
o.drawingType="ui";
return o;
if(d.borderWidth!==undefined){
o.data.borderWidth=d.borderWidth;
}
return o;
},enabled:true,selected:false,type:"drawing.library.UI.Button",select:function(){
this.selected=true;
if(this.icon){
this.icon.attr(this.style.button.icon.selected);
}
this._change(this.style.button.selected);
this.shape.shadow&&this.shape.shadow.hide();
},deselect:function(){
this.selected=false;
if(this.icon){
this.icon.attr(this.style.button.icon.norm);
}
this.shape.shadow&&this.shape.shadow.show();
this._change(this.style.button.norm);
},disable:function(){
if(!this.enabled){
return;
}
this.enabled=false;
this._change(this.style.button.disabled);
this.icon.attr({color:this.style.button.norm.color});
},enable:function(){
if(this.enabled){
return;
}
this.enabled=true;
this._change(this.style.button.norm);
this.icon.attr({color:this.style.button.icon.norm.color});
},_change:function(_b){
this.shape.attr(_b);
this.shape.shadow&&this.shape.shadow.container.moveToBack();
if(this.icon){
this.icon.shape.moveToFront();
}
},onOver:function(){
if(this.selected||!this.enabled){
return;
}
this._change(this.style.button.over);
},onOut:function(){
if(this.selected){
return;
}
this._change(this.style.button.norm);
},onDown:function(){
if(this.selected||!this.enabled){
return;
}
this._change(this.style.button.selected);
},onUp:function(){
if(!this.enabled){
return;
}
this._change(this.style.button.over);
if(this.hitched){
this.hitched();
}
this.onClick(this);
},attr:function(_c){
if(this.icon){
this.icon.attr(_c);
}
}});
_1.setObject("dojox.drawing.ui.Button",_6);
_5.register({name:"dojox.drawing.ui.Button"},"stencil");
return _6;
});
