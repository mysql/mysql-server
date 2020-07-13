//>>built
define("dojox/widget/FisheyeLite",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/on","dojo/query","dojo/dom-style","dojo/_base/fx","dijit/_WidgetBase","dojo/fx/easing"],function(_1,_2,_3,on,_4,_5,fx,_6,_7){
_3.getObject("widget",true,dojox);
_1.experimental("dojox/widget/FisheyeLite");
return dojo.declare("dojox.widget.FisheyeLite",[_6],{durationIn:350,easeIn:_7.backOut,durationOut:1420,easeOut:_7.elasticOut,properties:null,units:"px",constructor:function(_8,_9){
this.properties=_8.properties||{fontSize:2.75};
},postCreate:function(){
this.inherited(arguments);
this._target=_4(".fisheyeTarget",this.domNode)[0]||this.domNode;
this._makeAnims();
this.connect(this.domNode,"onmouseover","show");
this.connect(this.domNode,"onmouseout","hide");
this.connect(this._target,"onclick","onClick");
},show:function(){
this._runningOut.stop();
this._runningIn.play();
},hide:function(){
this._runningIn.stop();
this._runningOut.play();
},_makeAnims:function(){
var _a={},_b={},cs=_5.getComputedStyle(this._target);
for(var p in this.properties){
var _c=this.properties[p],_d=_3.isObject(_c),v=parseFloat(cs[p]);
_b[p]={end:v,units:this.units};
_a[p]=_d?_c:{end:_c*v,units:this.units};
}
this._runningIn=fx.animateProperty({node:this._target,easing:this.easeIn,duration:this.durationIn,properties:_a});
this._runningOut=fx.animateProperty({node:this._target,duration:this.durationOut,easing:this.easeOut,properties:_b});
this.connect(this._runningIn,"onEnd",_3.hitch(this,"onSelected",this));
},onClick:function(e){
},onSelected:function(e){
}});
});
