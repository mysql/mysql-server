//>>built
define("dojox/widget/Roller",["dojo","dijit","dijit/_Widget"],function(_1,_2){
var _3=_1.declare("dojox.widget.Roller",_2._Widget,{delay:2000,autoStart:true,itemSelector:"> li",durationIn:400,durationOut:275,_idx:-1,postCreate:function(){
if(!this["items"]){
this.items=[];
}
_1.addClass(this.domNode,"dojoxRoller");
_1.query(this.itemSelector,this.domNode).forEach(function(_4,i){
this.items.push(_4.innerHTML);
if(i==0){
this._roller=_4;
this._idx=0;
}else{
_1.destroy(_4);
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
var _5=this._anim;
this.connect(_5["out"],"onEnd",function(){
this._setIndex(this._idx+1);
_5["in"].play(15);
});
this.connect(_5["in"],"onEnd",function(){
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
_3.RollerSlide=_1.declare("dojox.widget.RollerSlide",dojox.widget.Roller,{durationOut:175,makeAnims:function(){
var n=this.domNode,_6="position",_7={top:{end:0,start:25},opacity:1};
_1.style(n,_6,"relative");
_1.style(this._roller,_6,"absolute");
_1.mixin(this,{_anim:{"in":_1.animateProperty({node:n,duration:this.durationIn,properties:_7}),"out":_1.fadeOut({node:n,duration:this.durationOut})}});
this._setupConnects();
}});
_3._Hover=_1.declare("dojox.widget._RollerHover",null,{postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onmouseenter","stop");
this.connect(this.domNode,"onmouseleave","start");
}});
return _3;
});
