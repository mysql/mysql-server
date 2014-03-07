//>>built
define("dojox/widget/Roller",["dojo","dijit","dijit/_Widget"],function(_1,_2){
_1.declare("dojox.widget.Roller",_2._Widget,{delay:2000,autoStart:true,itemSelector:"> li",durationIn:400,durationOut:275,_idx:-1,postCreate:function(){
if(!this["items"]){
this.items=[];
}
_1.addClass(this.domNode,"dojoxRoller");
_1.query(this.itemSelector,this.domNode).forEach(function(_3,i){
this.items.push(_3.innerHTML);
if(i==0){
this._roller=_3;
this._idx=0;
}else{
_1.destroy(_3);
}
},this);
if(!this._roller){
this._roller=_1.create("li",null,this.domNode);
}
this.makeAnims();
if(this.autoStart){
this.start();
}
},makeAnims:function(){
var n=this.domNode;
_1.mixin(this,{_anim:{"in":_1.fadeIn({node:n,duration:this.durationIn}),"out":_1.fadeOut({node:n,duration:this.durationOut})}});
this._setupConnects();
},_setupConnects:function(){
var _4=this._anim;
this.connect(_4["out"],"onEnd",function(){
this._setIndex(this._idx+1);
_4["in"].play(15);
});
this.connect(_4["in"],"onEnd",function(){
this._timeout=setTimeout(_1.hitch(this,"_run"),this.delay);
});
},start:function(){
if(!this.rolling){
this.rolling=true;
this._run();
}
},_run:function(){
this._anim["out"].gotoPercent(0,true);
},stop:function(){
this.rolling=false;
var m=this._anim,t=this._timeout;
if(t){
clearTimeout(t);
}
m["in"].stop();
m["out"].stop();
},_setIndex:function(i){
var l=this.items.length-1;
if(i<0){
i=l;
}
if(i>l){
i=0;
}
this._roller.innerHTML=this.items[i]||"error!";
this._idx=i;
}});
_1.declare("dojox.widget.RollerSlide",dojox.widget.Roller,{durationOut:175,makeAnims:function(){
var n=this.domNode,_5="position",_6={top:{end:0,start:25},opacity:1};
_1.style(n,_5,"relative");
_1.style(this._roller,_5,"absolute");
_1.mixin(this,{_anim:{"in":_1.animateProperty({node:n,duration:this.durationIn,properties:_6}),"out":_1.fadeOut({node:n,duration:this.durationOut})}});
this._setupConnects();
}});
_1.declare("dojox.widget._RollerHover",null,{postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onmouseenter","stop");
this.connect(this.domNode,"onmouseleave","start");
}});
return dojox.widget.Roller;
});
