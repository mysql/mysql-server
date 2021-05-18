//>>built
define("dojox/gfx/_gfxBidiSupport",["./_base","dojo/_base/lang","dojo/_base/sniff","dojo/dom","dojo/_base/html","dojo/_base/array","./utils","./shape","./path","dojox/string/BidiEngine"],function(g,_1,_2,_3,_4,_5,_6,_7,_8,_9){
_1.getObject("dojox.gfx._gfxBidiSupport",true);
switch(g.renderer){
case "vml":
g.isVml=true;
break;
case "svg":
g.isSvg=true;
if(g.svg.useSvgWeb){
g.isSvgWeb=true;
}
break;
case "silverlight":
g.isSilverlight=true;
break;
case "canvas":
case "canvasWithEvents":
g.isCanvas=true;
break;
}
var _a={LRM:"‎",LRE:"‪",PDF:"‬",RLM:"‏",RLE:"‫"};
var _b=new _9();
_1.extend(g.shape.Surface,{textDir:"",setTextDir:function(_c){
_d(this,_c);
},getTextDir:function(){
return this.textDir;
}});
_1.extend(g.Group,{textDir:"",setTextDir:function(_e){
_d(this,_e);
},getTextDir:function(){
return this.textDir;
}});
_1.extend(g.Text,{textDir:"",formatText:function(_f,_10){
if(_10&&_f&&_f.length>1){
var _11="ltr",_12=_10;
if(_12=="auto"){
if(g.isVml){
return _f;
}
_12=_b.checkContextual(_f);
}
if(g.isVml){
_11=_b.checkContextual(_f);
if(_12!=_11){
if(_12=="rtl"){
return !_b.hasBidiChar(_f)?_b.bidiTransform(_f,"IRNNN","ILNNN"):_a.RLM+_a.RLM+_f;
}else{
return _a.LRM+_f;
}
}
return _f;
}
if(g.isSvgWeb){
if(_12=="rtl"){
return _b.bidiTransform(_f,"IRNNN","ILNNN");
}
return _f;
}
if(g.isSilverlight){
return (_12=="rtl")?_b.bidiTransform(_f,"IRNNN","VLYNN"):_b.bidiTransform(_f,"ILNNN","VLYNN");
}
if(g.isCanvas){
return (_12=="rtl")?_a.RLE+_f+_a.PDF:_a.LRE+_f+_a.PDF;
}
if(g.isSvg){
if(_2("ff")<4){
return (_12=="rtl")?_b.bidiTransform(_f,"IRYNN","VLNNN"):_b.bidiTransform(_f,"ILYNN","VLNNN");
}else{
return _a.LRM+(_12=="rtl"?_a.RLE:_a.LRE)+_f+_a.PDF;
}
}
}
return _f;
},bidiPreprocess:function(_13){
return _13;
}});
_1.extend(g.TextPath,{textDir:"",formatText:function(_14,_15){
if(_15&&_14&&_14.length>1){
var _16="ltr",_17=_15;
if(_17=="auto"){
if(g.isVml){
return _14;
}
_17=_b.checkContextual(_14);
}
if(g.isVml){
_16=_b.checkContextual(_14);
if(_17!=_16){
if(_17=="rtl"){
return !_b.hasBidiChar(_14)?_b.bidiTransform(_14,"IRNNN","ILNNN"):_a.RLM+_a.RLM+_14;
}else{
return _a.LRM+_14;
}
}
return _14;
}
if(g.isSvgWeb){
if(_17=="rtl"){
return _b.bidiTransform(_14,"IRNNN","ILNNN");
}
return _14;
}
if(g.isSvg){
if(_2("opera")||_2("ff")>=4){
_14=_a.LRM+(_17=="rtl"?_a.RLE:_a.LRE)+_14+_a.PDF;
}else{
_14=(_17=="rtl")?_b.bidiTransform(_14,"IRYNN","VLNNN"):_b.bidiTransform(_14,"ILYNN","VLNNN");
}
}
}
return _14;
},bidiPreprocess:function(_18){
if(_18&&(typeof _18=="string")){
this.origText=_18;
_18=this.formatText(_18,this.textDir);
}
return _18;
}});
var _19=function(_1a,_1b,_1c,_1d){
var old=_1a.prototype[_1b];
_1a.prototype[_1b]=function(){
var _1e;
if(_1c){
_1e=_1c.apply(this,arguments);
}
var r=old.call(this,_1e);
if(_1d){
r=_1d.call(this,r,arguments);
}
return r;
};
};
var _1f=function(_20){
if(_20){
if(_20.textDir){
_20.textDir=_21(_20.textDir);
}
if(_20.text&&(_20.text instanceof Array)){
_20.text=_20.text.join(",");
}
}
if(_20&&(_20.text!=undefined||_20.textDir)&&(this.textDir!=_20.textDir||_20.text!=this.origText)){
this.origText=(_20.text!=undefined)?_20.text:this.origText;
if(_20.textDir){
this.textDir=_20.textDir;
}
_20.text=this.formatText(this.origText,this.textDir);
}
return this.bidiPreprocess(_20);
};
_19(g.Text,"setShape",_1f,null);
_19(g.TextPath,"setText",_1f,null);
var _22=function(_23){
var obj=_1.clone(_23);
if(obj&&this.origText){
obj.text=this.origText;
}
return obj;
};
_19(g.Text,"getShape",null,_22);
_19(g.TextPath,"getText",null,_22);
var _24=function(_25,_26){
var _27;
if(_26&&_26[0]){
_27=_21(_26[0]);
}
_25.setTextDir(_27?_27:this.textDir);
return _25;
};
_19(g.Surface,"createGroup",null,_24);
_19(g.Group,"createGroup",null,_24);
var _28=function(_29){
if(_29){
var _2a=_29.textDir?_21(_29.textDir):this.textDir;
if(_2a){
_29.textDir=_2a;
}
}
return _29;
};
_19(g.Surface,"createText",_28,null);
_19(g.Surface,"createTextPath",_28,null);
_19(g.Group,"createText",_28,null);
_19(g.Group,"createTextPath",_28,null);
g.createSurface=function(_2b,_2c,_2d,_2e){
var s=g[g.renderer].createSurface(_2b,_2c,_2d);
var _2f=_21(_2e);
if(g.isSvgWeb){
s.textDir=_2f?_2f:_4.style(_3.byId(_2b),"direction");
return s;
}
if(g.isVml||g.isSvg||g.isCanvas){
s.textDir=_2f?_2f:_4.style(s.rawNode,"direction");
}
if(g.isSilverlight){
s.textDir=_2f?_2f:_4.style(s._nodes[1],"direction");
}
return s;
};
function _d(obj,_30){
var _31=_21(_30);
if(_31){
g.utils.forEach(obj,function(e){
if(e instanceof g.Surface||e instanceof g.Group){
e.textDir=_31;
}
if(e instanceof g.Text){
e.setShape({textDir:_31});
}
if(e instanceof g.TextPath){
e.setText({textDir:_31});
}
},obj);
}
return obj;
};
function _21(_32){
var _33=["ltr","rtl","auto"];
if(_32){
_32=_32.toLowerCase();
if(_5.indexOf(_33,_32)<0){
return null;
}
}
return _32;
};
return g;
});
