//>>built
define("dojox/widget/FisheyeLite",["dojo","dojox","dijit/_Widget","dojo/fx/easing"],function(_1,_2,_3,_4){
_1.getObject("widget",true,_2);
_1.experimental("dojox.widget.FisheyeLite");
return _1.declare("dojox.widget.FisheyeLite",dijit._Widget,{durationIn:350,easeIn:_4.backOut,durationOut:1420,easeOut:_4.elasticOut,properties:null,units:"px",constructor:function(_5,_6){
this.properties=_5.properties||{fontSize:2.75};
},postCreate:function(){
this.inherited(arguments);
this._target=_1.query(".fisheyeTarget",this.domNode)[0]||this.domNode;
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
var _7={},_8={},cs=_1.getComputedStyle(this._target);
for(var p in this.properties){
var _9=this.properties[p],_a=_1.isObject(_9),v=parseInt(cs[p]);
_8[p]={end:v,units:this.units};
_7[p]=_a?_9:{end:_9*v,units:this.units};
}
this._runningIn=_1.animateProperty({node:this._target,easing:this.easeIn,duration:this.durationIn,properties:_7});
this._runningOut=_1.animateProperty({node:this._target,duration:this.durationOut,easing:this.easeOut,properties:_8});
this.connect(this._runningIn,"onEnd",_1.hitch(this,"onSelected",this));
},onClick:function(e){
},onSelected:function(e){
}});
});
