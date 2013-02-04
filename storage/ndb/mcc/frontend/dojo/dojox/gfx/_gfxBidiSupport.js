//>>built
define("dojox/gfx/_gfxBidiSupport",["./_base","dojo/_base/lang","dojo/_base/sniff","dojo/dom","dojo/_base/html","dojo/_base/array","./utils","./shape","dojox/string/BidiEngine"],function(g,_1,_2,_3,_4,_5,_6,_7,_8){
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
g.isCanvas=true;
break;
}
var _9={LRM:"‎",LRE:"‪",PDF:"‬",RLM:"‏",RLE:"‫"};
var _a=new _8();
_1.extend(g.shape.Surface,{textDir:"",setTextDir:function(_b){
_c(this,_b);
},getTextDir:function(){
return this.textDir;
}});
_1.extend(g.Group,{textDir:"",setTextDir:function(_d){
_c(this,_d);
},getTextDir:function(){
return this.textDir;
}});
_1.extend(g.Text,{textDir:"",formatText:function(_e,_f){
if(_f&&_e&&_e.length>1){
var _10="ltr",_11=_f;
if(_11=="auto"){
if(g.isVml){
return _e;
}
_11=_a.checkContextual(_e);
}
if(g.isVml){
_10=_a.checkContextual(_e);
if(_11!=_10){
if(_11=="rtl"){
return !_a.hasBidiChar(_e)?_a.bidiTransform(_e,"IRNNN","ILNNN"):_9.RLM+_9.RLM+_e;
}else{
return _9.LRM+_e;
}
}
return _e;
}
if(g.isSvgWeb){
if(_11=="rtl"){
return _a.bidiTransform(_e,"IRNNN","ILNNN");
}
return _e;
}
if(g.isSilverlight){
return (_11=="rtl")?_a.bidiTransform(_e,"IRNNN","VLYNN"):_a.bidiTransform(_e,"ILNNN","VLYNN");
}
if(g.isCanvas){
return (_11=="rtl")?_9.RLE+_e+_9.PDF:_9.LRE+_e+_9.PDF;
}
if(g.isSvg){
if(_2("ff")){
return (_11=="rtl")?_a.bidiTransform(_e,"IRYNN","VLNNN"):_a.bidiTransform(_e,"ILYNN","VLNNN");
}
if(_2("chrome")||_2("safari")||_2("opera")){
return _9.LRM+(_11=="rtl"?_9.RLE:_9.LRE)+_e+_9.PDF;
}
}
}
return _e;
},bidiPreprocess:function(_12){
return _12;
}});
_1.extend(g.TextPath,{textDir:"",formatText:function(_13,_14){
if(_14&&_13&&_13.length>1){
var _15="ltr",_16=_14;
if(_16=="auto"){
if(g.isVml){
return _13;
}
_16=_a.checkContextual(_13);
}
if(g.isVml){
_15=_a.checkContextual(_13);
if(_16!=_15){
if(_16=="rtl"){
return !_a.hasBidiChar(_13)?_a.bidiTransform(_13,"IRNNN","ILNNN"):_9.RLM+_9.RLM+_13;
}else{
return _9.LRM+_13;
}
}
return _13;
}
if(g.isSvgWeb){
if(_16=="rtl"){
return _a.bidiTransform(_13,"IRNNN","ILNNN");
}
return _13;
}
if(g.isSvg){
if(_2("opera")){
_13=_9.LRM+(_16=="rtl"?_9.RLE:_9.LRE)+_13+_9.PDF;
}else{
_13=(_16=="rtl")?_a.bidiTransform(_13,"IRYNN","VLNNN"):_a.bidiTransform(_13,"ILYNN","VLNNN");
}
}
}
return _13;
},bidiPreprocess:function(_17){
if(_17&&(typeof _17=="string")){
this.origText=_17;
_17=this.formatText(_17,this.textDir);
}
return _17;
}});
var _18=function(_19,_1a,_1b,_1c){
var old=_19.prototype[_1a];
_19.prototype[_1a]=function(){
var _1d;
if(_1b){
_1d=_1b.apply(this,arguments);
}
var r=old.call(this,_1d);
if(_1c){
r=_1c.call(this,r,arguments);
}
return r;
};
};
var _1e=function(_1f){
if(_1f){
if(_1f.textDir){
_1f.textDir=_20(_1f.textDir);
}
if(_1f.text&&(_1f.text instanceof Array)){
_1f.text=_1f.text.join(",");
}
}
if(_1f&&(_1f.text!=undefined||_1f.textDir)&&(this.textDir!=_1f.textDir||_1f.text!=this.origText)){
this.origText=(_1f.text!=undefined)?_1f.text:this.origText;
if(_1f.textDir){
this.textDir=_1f.textDir;
}
_1f.text=this.formatText(this.origText,this.textDir);
}
return this.bidiPreprocess(_1f);
};
_18(g.Text,"setShape",_1e,null);
_18(g.TextPath,"setText",_1e,null);
var _21=function(_22){
var obj=_1.clone(_22);
if(obj&&this.origText){
obj.text=this.origText;
}
return obj;
};
_18(g.Text,"getShape",null,_21);
_18(g.TextPath,"getText",null,_21);
var _23=function(_24,_25){
var _26;
if(_25&&_25[0]){
_26=_20(_25[0]);
}
_24.setTextDir(_26?_26:this.textDir);
return _24;
};
_18(g.Surface,"createGroup",null,_23);
_18(g.Group,"createGroup",null,_23);
var _27=function(_28){
if(_28){
var _29=_28.textDir?_20(_28.textDir):this.textDir;
if(_29){
_28.textDir=_29;
}
}
return _28;
};
_18(g.Surface,"createText",_27,null);
_18(g.Surface,"createTextPath",_27,null);
_18(g.Group,"createText",_27,null);
_18(g.Group,"createTextPath",_27,null);
g.createSurface=function(_2a,_2b,_2c,_2d){
var s=g[g.renderer].createSurface(_2a,_2b,_2c);
var _2e=_20(_2d);
if(g.isSvgWeb){
s.textDir=_2e?_2e:_4.style(_3.byId(_2a),"direction");
return s;
}
if(g.isVml||g.isSvg||g.isCanvas){
s.textDir=_2e?_2e:_4.style(s.rawNode,"direction");
}
if(g.isSilverlight){
s.textDir=_2e?_2e:_4.style(s._nodes[1],"direction");
}
return s;
};
function _c(obj,_2f){
var _30=_20(_2f);
if(_30){
g.utils.forEach(obj,function(e){
if(e instanceof g.Surface||e instanceof g.Group){
e.textDir=_30;
}
if(e instanceof g.Text){
e.setShape({textDir:_30});
}
if(e instanceof g.TextPath){
e.setText({textDir:_30});
}
},obj);
}
return obj;
};
function _20(_31){
var _32=["ltr","rtl","auto"];
if(_31){
_31=_31.toLowerCase();
if(_5.indexOf(_32,_31)<0){
return null;
}
}
return _31;
};
return g;
});
