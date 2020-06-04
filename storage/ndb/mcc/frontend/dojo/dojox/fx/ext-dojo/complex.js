//>>built
define("dojox/fx/ext-dojo/complex",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/connect","dojo/_base/Color","dojo/_base/fx","dojo/fx"],function(_1,_2,_3,_4,_5,_6,_7,_8){
_2.getObject("dojox.fx.ext-dojo.complex",true);
var da=_7.animateProperty;
_1.animateProperty=_7.animateProperty=function(_9){
var _a=da(_9);
_5.connect(_a,"beforeBegin",function(){
_a.curve.getValue=function(r){
var _b={};
for(var p in this._properties){
var _c=this._properties[p],_d=_c.start;
if(_d instanceof _1.Color){
_b[p]=_1.blendColors(_d,_c.end,r,_c.tempColor).toCss();
}else{
if(_d instanceof dojox.fx._Complex){
_b[p]=_d.getValue(r);
}else{
if(!_1.isArray(_d)){
_b[p]=((_c.end-_d)*r)+_d+(p!="opacity"?_c.units||"px":0);
}
}
}
}
return _b;
};
var pm={};
for(var p in this.properties){
var o=this.properties[p];
if(typeof (o.start)=="string"&&/\(/.test(o.start)){
this.curve._properties[p].start=new dojox.fx._Complex(o);
}
}
});
return _a;
};
return _4("dojox.fx._Complex",null,{PROP:/\([\w|,|+|\-|#|\.|\s]*\)/g,constructor:function(_e){
var _f=_e.start.match(this.PROP);
var end=_e.end.match(this.PROP);
var _10=_3.map(_f,this.getProps,this);
var _11=_3.map(end,this.getProps,this);
this._properties={};
this.strProp=_e.start;
_3.forEach(_10,function(_12,i){
_3.forEach(_12,function(p,j){
this.strProp=this.strProp.replace(p,"PROP_"+i+""+j);
this._properties["PROP_"+i+""+j]=this.makePropObject(p,_11[i][j]);
},this);
},this);
},getValue:function(r){
var str=this.strProp,u;
for(var nm in this._properties){
var v,o=this._properties[nm];
if(o.units=="isColor"){
v=_6.blendColors(o.beg,o.end,r).toCss(false);
u="";
}else{
v=((o.end-o.beg)*r)+o.beg;
u=o.units;
}
str=str.replace(nm,v+u);
}
return str;
},makePropObject:function(beg,end){
var b=this.getNumAndUnits(beg);
var e=this.getNumAndUnits(end);
return {beg:b.num,end:e.num,units:b.units};
},getProps:function(str){
str=str.substring(1,str.length-1);
var s;
if(/,/.test(str)){
str=str.replace(/\s/g,"");
s=str.split(",");
}else{
str=str.replace(/\s{2,}/g," ");
s=str.split(" ");
}
return s;
},getNumAndUnits:function(_13){
if(!_13){
return {};
}
if(/#/.test(_13)){
return {num:new _6(_13),units:"isColor"};
}
var o={num:parseFloat(/-*[\d\.\d|\d]{1,}/.exec(_13).join(""))};
o.units=/[a-z]{1,}/.exec(_13);
o.units=o.units&&o.units.length?o.units.join(""):"";
return o;
}});
});
