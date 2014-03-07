//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.widget.rotator.Controller");
(function(d){
var _4="dojoxRotator",_5=_4+"Play",_6=_4+"Pause",_7=_4+"Number",_8=_4+"Tab",_9=_4+"Selected";
d.declare("dojox.widget.rotator.Controller",null,{rotator:null,commands:"prev,play/pause,info,next",constructor:function(_a,_b){
d.mixin(this,_a);
var r=this.rotator;
if(r){
while(_b.firstChild){
_b.removeChild(_b.firstChild);
}
var ul=this._domNode=d.create("ul",null,_b),_c=" "+_4+"Icon",cb=function(_d,_e,_f){
d.create("li",{className:_e,innerHTML:"<a href=\"#\"><span>"+_d+"</span></a>",onclick:function(e){
d.stopEvent(e);
if(r){
r.control.apply(r,_f);
}
}},ul);
};
d.forEach(this.commands.split(","),function(b,i){
switch(b){
case "prev":
cb("Prev",_4+"Prev"+_c,["prev"]);
break;
case "play/pause":
cb("Play",_5+_c,["play"]);
cb("Pause",_6+_c,["pause"]);
break;
case "info":
this._info=d.create("li",{className:_4+"Info",innerHTML:this._buildInfo(r)},ul);
break;
case "next":
cb("Next",_4+"Next"+_c,["next"]);
break;
case "#":
case "titles":
for(var j=0;j<r.panes.length;j++){
cb(b=="#"?j+1:r.panes[j].title||"Tab "+(j+1),(b=="#"?_7:_8)+" "+(j==r.idx?_9:"")+" "+_4+"Pane"+j,["go",j]);
}
break;
}
},this);
d.query("li:first-child",ul).addClass(_4+"First");
d.query("li:last-child",ul).addClass(_4+"Last");
this._togglePlay();
this._con=d.connect(r,"onUpdate",this,"_onUpdate");
}
},destroy:function(){
d.disconnect(this._con);
d.destroy(this._domNode);
},_togglePlay:function(_10){
var p=this.rotator.playing;
d.query("."+_5,this._domNode).style("display",p?"none":"");
d.query("."+_6,this._domNode).style("display",p?"":"none");
},_buildInfo:function(r){
return "<span>"+(r.idx+1)+" / "+r.panes.length+"</span>";
},_onUpdate:function(_11){
var r=this.rotator;
switch(_11){
case "play":
case "pause":
this._togglePlay();
break;
case "onAfterTransition":
if(this._info){
this._info.innerHTML=this._buildInfo(r);
}
var s=function(n){
if(r.idx<n.length){
d.addClass(n[r.idx],_9);
}
};
s(d.query("."+_7,this._domNode).removeClass(_9));
s(d.query("."+_8,this._domNode).removeClass(_9));
break;
}
}});
})(_2);
});
