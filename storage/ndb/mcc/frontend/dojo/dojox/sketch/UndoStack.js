//>>built
define("dojox/sketch/UndoStack",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","../xml/DomParser"],function(_1){
_1.getObject("sketch",true,dojox);
var ta=dojox.sketch;
ta.CommandTypes={Create:"Create",Move:"Move",Modify:"Modify",Delete:"Delete",Convert:"Convert"};
_1.declare("dojox.sketch.UndoStack",null,{constructor:function(_2){
this.figure=_2;
this._steps=[];
this._undoedSteps=[];
},apply:function(_3,_4,to){
if(!_4&&!to&&_3.fullText){
this.figure.setValue(_3.fullText);
return;
}
var _5=_4.shapeText;
var _6=to.shapeText;
if(_5.length==0&&_6.length==0){
return;
}
if(_5.length==0){
var o=dojox.xml.DomParser.parse(_6).documentElement;
var a=this.figure._loadAnnotation(o);
if(a){
this.figure._add(a);
}
return;
}
if(_6.length==0){
var _7=this.figure.getAnnotator(_4.shapeId);
this.figure._delete([_7],true);
return;
}
var _8=this.figure.getAnnotator(to.shapeId);
var no=dojox.xml.DomParser.parse(_6).documentElement;
_8.draw(no);
this.figure.select(_8);
return;
},add:function(_9,_a,_b){
var id=_a?_a.id:"";
var _c=_a?_a.serialize():"";
if(_9==ta.CommandTypes.Delete){
_c="";
}
var _d={cmdname:_9,before:{shapeId:id,shapeText:_b||""},after:{shapeId:id,shapeText:_c}};
this._steps.push(_d);
this._undoedSteps=[];
},destroy:function(){
},undo:function(){
var _e=this._steps.pop();
if(_e){
this._undoedSteps.push(_e);
this.apply(_e,_e.after,_e.before);
}
},redo:function(){
var _f=this._undoedSteps.pop();
if(_f){
this._steps.push(_f);
this.apply(_f,_f.before,_f.after);
}
}});
return dojox.sketch.UndoStack;
});
