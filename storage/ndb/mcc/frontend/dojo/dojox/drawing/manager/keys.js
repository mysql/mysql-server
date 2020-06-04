//>>built
define("dojox/drawing/manager/keys",["dojo","../util/common"],function(_1,_2){
var _3=false;
var _4=true;
var _5="abcdefghijklmnopqrstuvwxyz";
var _6={arrowIncrement:1,arrowShiftIncrement:10,shift:false,ctrl:false,alt:false,cmmd:false,meta:false,onDelete:function(_7){
},onEsc:function(_8){
},onEnter:function(_9){
},onArrow:function(_a){
},onKeyDown:function(_b){
},onKeyUp:function(_c){
},listeners:[],register:function(_d){
var _e=_2.uid("listener");
this.listeners.push({handle:_e,scope:_d.scope||window,callback:_d.callback,keyCode:_d.keyCode});
},_getLetter:function(_f){
if(!_f.meta&&_f.keyCode>=65&&_f.keyCode<=90){
return _5.charAt(_f.keyCode-65);
}
return null;
},_mixin:function(evt){
evt.meta=this.meta;
evt.shift=this.shift;
evt.alt=this.alt;
evt.cmmd=this.cmmd;
evt.ctrl=this.ctrl;
evt.letter=this._getLetter(evt);
return evt;
},editMode:function(_10){
_3=_10;
},enable:function(_11){
_4=_11;
},scanForFields:function(){
if(this._fieldCons){
_1.forEach(this._fieldCons,_1.disconnect,_1);
}
this._fieldCons=[];
_1.query("input").forEach(function(n){
var a=_1.connect(n,"focus",this,function(evt){
this.enable(false);
});
var b=_1.connect(n,"blur",this,function(evt){
this.enable(true);
});
this._fieldCons.push(a);
this._fieldCons.push(b);
},this);
},init:function(){
setTimeout(_1.hitch(this,"scanForFields"),500);
_1.connect(document,"blur",this,function(evt){
this.meta=this.shift=this.ctrl=this.cmmd=this.alt=false;
});
_1.connect(document,"keydown",this,function(evt){
if(!_4){
return;
}
if(evt.keyCode==16){
this.shift=true;
}
if(evt.keyCode==17){
this.ctrl=true;
}
if(evt.keyCode==18){
this.alt=true;
}
if(evt.keyCode==224){
this.cmmd=true;
}
this.meta=this.shift||this.ctrl||this.cmmd||this.alt;
if(!_3){
this.onKeyDown(this._mixin(evt));
if(evt.keyCode==8||evt.keyCode==46){
_1.stopEvent(evt);
}
}
});
_1.connect(document,"keyup",this,function(evt){
if(!_4){
return;
}
var _12=false;
if(evt.keyCode==16){
this.shift=false;
}
if(evt.keyCode==17){
this.ctrl=false;
}
if(evt.keyCode==18){
this.alt=false;
}
if(evt.keyCode==224){
this.cmmd=false;
}
this.meta=this.shift||this.ctrl||this.cmmd||this.alt;
!_3&&this.onKeyUp(this._mixin(evt));
if(evt.keyCode==13){
console.warn("KEY ENTER");
this.onEnter(evt);
_12=true;
}
if(evt.keyCode==27){
this.onEsc(evt);
_12=true;
}
if(evt.keyCode==8||evt.keyCode==46){
this.onDelete(evt);
_12=true;
}
if(_12&&!_3){
_1.stopEvent(evt);
}
});
_1.connect(document,"keypress",this,function(evt){
if(!_4){
return;
}
var inc=this.shift?this.arrowIncrement*this.arrowShiftIncrement:this.arrowIncrement,_13=evt.alt||this.cmmd;
var x=0,y=0;
if(evt.keyCode==32&&!_3){
_1.stopEvent(evt);
}
if(evt.keyCode==37&&!_13){
x=-inc;
}
if(evt.keyCode==38&&!_13){
y=-inc;
}
if(evt.keyCode==39&&!_13){
x=inc;
}
if(evt.keyCode==40&&!_13){
y=inc;
}
if(x||y){
evt.x=x;
evt.y=y;
evt.shift=this.shift;
if(!_3){
this.onArrow(evt);
_1.stopEvent(evt);
}
}
});
}};
_1.addOnLoad(_6,"init");
return _6;
});
