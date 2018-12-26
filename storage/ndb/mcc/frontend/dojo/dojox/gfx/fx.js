//>>built
define("dojox/gfx/fx",["dojo/_base/lang","./_base","./matrix","dojo/_base/Color","dojo/_base/array","dojo/_base/fx","dojo/_base/connect"],function(_1,g,m,_2,_3,fx,_4){
var _5=g.fx={};
function _6(_7,_8){
this.start=_7,this.end=_8;
};
_6.prototype.getValue=function(r){
return (this.end-this.start)*r+this.start;
};
function _9(_a,_b,_c){
this.start=_a,this.end=_b;
this.units=_c;
};
_9.prototype.getValue=function(r){
return (this.end-this.start)*r+this.start+this.units;
};
function _d(_e,_f){
this.start=_e,this.end=_f;
this.temp=new _2();
};
_d.prototype.getValue=function(r){
return _2.blendColors(this.start,this.end,r,this.temp);
};
function _10(_11){
this.values=_11;
this.length=_11.length;
};
_10.prototype.getValue=function(r){
return this.values[Math.min(Math.floor(r*this.length),this.length-1)];
};
function _12(_13,def){
this.values=_13;
this.def=def?def:{};
};
_12.prototype.getValue=function(r){
var ret=_1.clone(this.def);
for(var i in this.values){
ret[i]=this.values[i].getValue(r);
}
return ret;
};
function _14(_15,_16){
this.stack=_15;
this.original=_16;
};
_14.prototype.getValue=function(r){
var ret=[];
_3.forEach(this.stack,function(t){
if(t instanceof m.Matrix2D){
ret.push(t);
return;
}
if(t.name=="original"&&this.original){
ret.push(this.original);
return;
}
if(!(t.name in m)){
return;
}
var f=m[t.name];
if(typeof f!="function"){
ret.push(f);
return;
}
var val=_3.map(t.start,function(v,i){
return (t.end[i]-v)*r+v;
}),_17=f.apply(m,val);
if(_17 instanceof m.Matrix2D){
ret.push(_17);
}
},this);
return ret;
};
var _18=new _2(0,0,0,0);
function _19(_1a,obj,_1b,def){
if(_1a.values){
return new _10(_1a.values);
}
var _1c,_1d,end;
if(_1a.start){
_1d=g.normalizeColor(_1a.start);
}else{
_1d=_1c=obj?(_1b?obj[_1b]:obj):def;
}
if(_1a.end){
end=g.normalizeColor(_1a.end);
}else{
if(!_1c){
_1c=obj?(_1b?obj[_1b]:obj):def;
}
end=_1c;
}
return new _d(_1d,end);
};
function _1e(_1f,obj,_20,def){
if(_1f.values){
return new _10(_1f.values);
}
var _21,_22,end;
if(_1f.start){
_22=_1f.start;
}else{
_22=_21=obj?obj[_20]:def;
}
if(_1f.end){
end=_1f.end;
}else{
if(typeof _21!="number"){
_21=obj?obj[_20]:def;
}
end=_21;
}
return new _6(_22,end);
};
_5.animateStroke=function(_23){
if(!_23.easing){
_23.easing=fx._defaultEasing;
}
var _24=new fx.Animation(_23),_25=_23.shape,_26;
_4.connect(_24,"beforeBegin",_24,function(){
_26=_25.getStroke();
var _27=_23.color,_28={},_29,_2a,end;
if(_27){
_28.color=_19(_27,_26,"color",_18);
}
_27=_23.style;
if(_27&&_27.values){
_28.style=new _10(_27.values);
}
_27=_23.width;
if(_27){
_28.width=_1e(_27,_26,"width",1);
}
_27=_23.cap;
if(_27&&_27.values){
_28.cap=new _10(_27.values);
}
_27=_23.join;
if(_27){
if(_27.values){
_28.join=new _10(_27.values);
}else{
_2a=_27.start?_27.start:(_26&&_26.join||0);
end=_27.end?_27.end:(_26&&_26.join||0);
if(typeof _2a=="number"&&typeof end=="number"){
_28.join=new _6(_2a,end);
}
}
}
this.curve=new _12(_28,_26);
});
_4.connect(_24,"onAnimate",_25,"setStroke");
return _24;
};
_5.animateFill=function(_2b){
if(!_2b.easing){
_2b.easing=fx._defaultEasing;
}
var _2c=new fx.Animation(_2b),_2d=_2b.shape,_2e;
_4.connect(_2c,"beforeBegin",_2c,function(){
_2e=_2d.getFill();
var _2f=_2b.color,_30={};
if(_2f){
this.curve=_19(_2f,_2e,"",_18);
}
});
_4.connect(_2c,"onAnimate",_2d,"setFill");
return _2c;
};
_5.animateFont=function(_31){
if(!_31.easing){
_31.easing=fx._defaultEasing;
}
var _32=new fx.Animation(_31),_33=_31.shape,_34;
_4.connect(_32,"beforeBegin",_32,function(){
_34=_33.getFont();
var _35=_31.style,_36={},_37,_38,end;
if(_35&&_35.values){
_36.style=new _10(_35.values);
}
_35=_31.variant;
if(_35&&_35.values){
_36.variant=new _10(_35.values);
}
_35=_31.weight;
if(_35&&_35.values){
_36.weight=new _10(_35.values);
}
_35=_31.family;
if(_35&&_35.values){
_36.family=new _10(_35.values);
}
_35=_31.size;
if(_35&&_35.units){
_38=parseFloat(_35.start?_35.start:(_33.font&&_33.font.size||"0"));
end=parseFloat(_35.end?_35.end:(_33.font&&_33.font.size||"0"));
_36.size=new _9(_38,end,_35.units);
}
this.curve=new _12(_36,_34);
});
_4.connect(_32,"onAnimate",_33,"setFont");
return _32;
};
_5.animateTransform=function(_39){
if(!_39.easing){
_39.easing=fx._defaultEasing;
}
var _3a=new fx.Animation(_39),_3b=_39.shape,_3c;
_4.connect(_3a,"beforeBegin",_3a,function(){
_3c=_3b.getTransform();
this.curve=new _14(_39.transform,_3c);
});
_4.connect(_3a,"onAnimate",_3b,"setTransform");
return _3a;
};
return _5;
});
