//>>built
define("dojox/widget/rotator/Controller",["dojo/_base/declare","dojo/_base/lang","dojo/_base/html","dojo/_base/event","dojo/_base/array","dojo/_base/connect","dojo/query"],function(_1,_2,_3,_4,_5,_6,_7){
var _8="dojoxRotator",_9=_8+"Play",_a=_8+"Pause",_b=_8+"Number",_c=_8+"Tab",_d=_8+"Selected";
return _1("dojox.widget.rotator.Controller",null,{rotator:null,commands:"prev,play/pause,info,next",constructor:function(_e,_f){
_2.mixin(this,_e);
var r=this.rotator;
if(r){
while(_f.firstChild){
_f.removeChild(_f.firstChild);
}
var ul=this._domNode=_3.create("ul",null,_f),_10=" "+_8+"Icon",cb=function(_11,css,_12){
_3.create("li",{className:css,innerHTML:"<a href=\"#\"><span>"+_11+"</span></a>",onclick:function(e){
_4.stop(e);
if(r){
r.control.apply(r,_12);
}
}},ul);
};
_5.forEach(this.commands.split(","),function(b,i){
switch(b){
case "prev":
cb("Prev",_8+"Prev"+_10,["prev"]);
break;
case "play/pause":
cb("Play",_9+_10,["play"]);
cb("Pause",_a+_10,["pause"]);
break;
case "info":
this._info=_3.create("li",{className:_8+"Info",innerHTML:this._buildInfo(r)},ul);
break;
case "next":
cb("Next",_8+"Next"+_10,["next"]);
break;
case "#":
case "titles":
for(var j=0;j<r.panes.length;j++){
cb(b=="#"?j+1:r.panes[j].title||"Tab "+(j+1),(b=="#"?_b:_c)+" "+(j==r.idx?_d:"")+" "+_8+"Pane"+j,["go",j]);
}
break;
}
},this);
_7("li:first-child",ul).addClass(_8+"First");
_7("li:last-child",ul).addClass(_8+"Last");
this._togglePlay();
this._con=_6.connect(r,"onUpdate",this,"_onUpdate");
}
},destroy:function(){
_6.disconnect(this._con);
_3.destroy(this._domNode);
},_togglePlay:function(_13){
var p=this.rotator.playing;
_7("."+_9,this._domNode).style("display",p?"none":"");
_7("."+_a,this._domNode).style("display",p?"":"none");
},_buildInfo:function(r){
return "<span>"+(r.idx+1)+" / "+r.panes.length+"</span>";
},_onUpdate:function(_14){
var r=this.rotator;
switch(_14){
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
_3.addClass(n[r.idx],_d);
}
};
s(_7("."+_b,this._domNode).removeClass(_d));
s(_7("."+_c,this._domNode).removeClass(_d));
break;
}
}});
});
