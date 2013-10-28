//>>built
define("dojox/gfx/VectorText",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/loader","dojo/_base/xhr","./_base","dojox/xml/DomParser","dojox/html/metrics","./matrix"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=function(_b){
var _c;
_5.get({url:_b,sync:true,load:function(_d){
_c=_d;
}});
return _c;
};
_1.getObject("dojox.gfx.VectorText",true);
_1.mixin(_6,{vectorFontFitting:{NONE:0,FLOW:1,FIT:2},defaultVectorText:{type:"vectortext",x:0,y:0,width:null,height:null,text:"",align:"start",decoration:"none",fitting:0,leading:1.5},defaultVectorFont:{type:"vectorfont",size:"10pt",family:null},_vectorFontCache:{},_svgFontCache:{},getVectorFont:function(_e){
if(_6._vectorFontCache[_e]){
return _6._vectorFontCache[_e];
}
return new _6.VectorFont(_e);
}});
return _2("dojox.gfx.VectorFont",null,{_entityRe:/&(quot|apos|lt|gt|amp|#x[^;]+|#\d+);/g,_decodeEntitySequence:function(_f){
if(!_f.match(this._entityRe)){
return;
}
var _10={amp:"&",apos:"'",quot:"\"",lt:"<",gt:">"};
var r,tmp="";
while((r=this._entityRe.exec(_f))!==null){
if(r[1].charAt(1)=="x"){
tmp+=String.fromCharCode(parseInt(r[1].slice(2),16));
}else{
if(!isNaN(parseInt(r[1].slice(1),10))){
tmp+=String.fromCharCode(parseInt(r[1].slice(1),10));
}else{
tmp+=_10[r[1]]||"";
}
}
}
return tmp;
},_parse:function(svg,url){
var doc=_6._svgFontCache[url]||_7.parse(svg);
var f=doc.documentElement.byName("font")[0],_11=doc.documentElement.byName("font-face")[0];
var _12=parseFloat(_11.getAttribute("units-per-em")||1000,10);
var _13={x:parseFloat(f.getAttribute("horiz-adv-x"),10),y:parseFloat(f.getAttribute("vert-adv-y")||0,10)};
if(!_13.y){
_13.y=_12;
}
var _14={horiz:{x:parseFloat(f.getAttribute("horiz-origin-x")||0,10),y:parseFloat(f.getAttribute("horiz-origin-y")||0,10)},vert:{x:parseFloat(f.getAttribute("vert-origin-x")||0,10),y:parseFloat(f.getAttribute("vert-origin-y")||0,10)}};
var _15=_11.getAttribute("font-family"),_16=_11.getAttribute("font-style")||"all",_17=_11.getAttribute("font-variant")||"normal",_18=_11.getAttribute("font-weight")||"all",_19=_11.getAttribute("font-stretch")||"normal",_1a=_11.getAttribute("unicode-range")||"U+0-10FFFF",_1b=_11.getAttribute("panose-1")||"0 0 0 0 0 0 0 0 0 0",_1c=_11.getAttribute("cap-height"),_1d=parseFloat(_11.getAttribute("ascent")||(_12-_14.vert.y),10),_1e=parseFloat(_11.getAttribute("descent")||_14.vert.y,10),_1f={};
var _20=_15;
if(_11.byName("font-face-name")[0]){
_20=_11.byName("font-face-name")[0].getAttribute("name");
}
if(_6._vectorFontCache[_20]){
return;
}
_3.forEach(["alphabetic","ideographic","mathematical","hanging"],function(_21){
var a=_11.getAttribute(_21);
if(a!==null){
_1f[_21]=parseFloat(a,10);
}
});
var _22=parseFloat(doc.documentElement.byName("missing-glyph")[0].getAttribute("horiz-adv-x")||_13.x,10);
var _23={},_24={},g=doc.documentElement.byName("glyph");
_3.forEach(g,function(_25){
var _26=_25.getAttribute("unicode"),_20=_25.getAttribute("glyph-name"),_27=parseFloat(_25.getAttribute("horiz-adv-x")||_13.x,10),_28=_25.getAttribute("d");
if(_26.match(this._entityRe)){
_26=this._decodeEntitySequence(_26);
}
var o={code:_26,name:_20,xAdvance:_27,path:_28};
_23[_26]=o;
_24[_20]=o;
},this);
var _29=doc.documentElement.byName("hkern");
_3.forEach(_29,function(_2a,i){
var k=-parseInt(_2a.getAttribute("k"),10);
var u1=_2a.getAttribute("u1"),g1=_2a.getAttribute("g1"),u2=_2a.getAttribute("u2"),g2=_2a.getAttribute("g2"),gl;
if(u1){
u1=this._decodeEntitySequence(u1);
if(_23[u1]){
gl=_23[u1];
}
}else{
if(_24[g1]){
gl=_24[g1];
}
}
if(gl){
if(!gl.kern){
gl.kern={};
}
if(u2){
u2=this._decodeEntitySequence(u2);
gl.kern[u2]={x:k};
}else{
if(_24[g2]){
gl.kern[_24[g2].code]={x:k};
}
}
}
},this);
_1.mixin(this,{family:_15,name:_20,style:_16,variant:_17,weight:_18,stretch:_19,range:_1a,viewbox:{width:_12,height:_12},origin:_14,advance:_1.mixin(_13,{missing:{x:_22,y:_22}}),ascent:_1d,descent:_1e,baseline:_1f,glyphs:_23});
_6._vectorFontCache[_20]=this;
_6._vectorFontCache[url]=this;
if(_20!=_15&&!_6._vectorFontCache[_15]){
_6._vectorFontCache[_15]=this;
}
if(!_6._svgFontCache[url]){
_6._svgFontCache[url]=doc;
}
},_clean:function(){
var _2b=this.name,_2c=this.family;
_3.forEach(["family","name","style","variant","weight","stretch","range","viewbox","origin","advance","ascent","descent","baseline","glyphs"],function(_2d){
try{
delete this[_2d];
}
catch(e){
}
},this);
if(_6._vectorFontCache[_2b]){
delete _6._vectorFontCache[_2b];
}
if(_6._vectorFontCache[_2c]){
delete _6._vectorFontCache[_2c];
}
return this;
},constructor:function(url){
this._defaultLeading=1.5;
if(url!==undefined){
this.load(url);
}
},load:function(url){
this.onLoadBegin(url.toString());
this._parse(_6._svgFontCache[url.toString()]||_a(url.toString()),url.toString());
this.onLoad(this);
return this;
},initialized:function(){
return (this.glyphs!==null);
},_round:function(n){
return Math.round(1000*n)/1000;
},_leading:function(_2e){
return this.viewbox.height*(_2e||this._defaultLeading);
},_normalize:function(str){
return str.replace(/\s+/g,String.fromCharCode(32));
},_getWidth:function(_2f){
var w=0,_30=0,_31=null;
_3.forEach(_2f,function(_32,i){
_30=_32.xAdvance;
if(_2f[i]&&_32.kern&&_32.kern[_2f[i].code]){
_30+=_32.kern[_2f[i].code].x;
}
w+=_30;
_31=_32;
});
if(_31&&_31.code==" "){
w-=_31.xAdvance;
}
return this._round(w);
},_getLongestLine:function(_33){
var _34=0,idx=0;
_3.forEach(_33,function(_35,i){
var max=Math.max(_34,this._getWidth(_35));
if(max>_34){
_34=max;
idx=i;
}
},this);
return {width:_34,index:idx,line:_33[idx]};
},_trim:function(_36){
var fn=function(arr){
if(!arr.length){
return;
}
if(arr[arr.length-1].code==" "){
arr.splice(arr.length-1,1);
}
if(!arr.length){
return;
}
if(arr[0].code==" "){
arr.splice(0,1);
}
};
if(_1.isArray(_36[0])){
_3.forEach(_36,fn);
}else{
fn(_36);
}
return _36;
},_split:function(_37,_38){
var w=this._getWidth(_37),_39=Math.floor(w/_38),_3a=[],cw=0,c=[],_3b=false;
for(var i=0,l=_37.length;i<l;i++){
if(_37[i].code==" "){
_3b=true;
}
cw+=_37[i].xAdvance;
if(i+1<l&&_37[i].kern&&_37[i].kern[_37[i+1].code]){
cw+=_37[i].kern[_37[i+1].code].x;
}
if(cw>=_39){
var chr=_37[i];
while(_3b&&chr.code!=" "&&i>=0){
chr=c.pop();
i--;
}
_3a.push(c);
c=[];
cw=0;
_3b=false;
}
c.push(_37[i]);
}
if(c.length){
_3a.push(c);
}
return this._trim(_3a);
},_getSizeFactor:function(_3c){
_3c+="";
var _3d=_8.getCachedFontMeasurements(),_3e=this.viewbox.height,f=_3d["1em"],_3f=parseFloat(_3c,10);
if(_3c.indexOf("em")>-1){
return this._round((_3d["1em"]*_3f)/_3e);
}else{
if(_3c.indexOf("ex")>-1){
return this._round((_3d["1ex"]*_3f)/_3e);
}else{
if(_3c.indexOf("pt")>-1){
return this._round(((_3d["12pt"]/12)*_3f)/_3e);
}else{
if(_3c.indexOf("px")>-1){
return this._round(((_3d["16px"]/16)*_3f)/_3e);
}else{
if(_3c.indexOf("%")>-1){
return this._round((_3d["1em"]*(_3f/100))/_3e);
}else{
f=_3d[_3c]||_3d.medium;
return this._round(f/_3e);
}
}
}
}
}
},_getFitFactor:function(_40,w,h,l){
if(!h){
return this._round(w/this._getWidth(_40));
}else{
var _41=this._getLongestLine(_40).width,_42=(_40.length*(this.viewbox.height*l))-((this.viewbox.height*l)-this.viewbox.height);
return this._round(Math.min(w/_41,h/_42));
}
},_getBestFit:function(_43,w,h,_44){
var _45=32,_46=0,_47=_45;
while(_45>0){
var f=this._getFitFactor(this._split(_43,_45),w,h,_44);
if(f>_46){
_46=f;
_47=_45;
}
_45--;
}
return {scale:_46,lines:this._split(_43,_47)};
},_getBestFlow:function(_48,w,_49){
var _4a=[],cw=0,c=[],_4b=false;
for(var i=0,l=_48.length;i<l;i++){
if(_48[i].code==" "){
_4b=true;
}
var tw=_48[i].xAdvance;
if(i+1<l&&_48[i].kern&&_48[i].kern[_48[i+1].code]){
tw+=_48[i].kern[_48[i+1].code].x;
}
cw+=_49*tw;
if(cw>=w){
var chr=_48[i];
while(_4b&&chr.code!=" "&&i>=0){
chr=c.pop();
i--;
}
_4a.push(c);
c=[];
cw=0;
_4b=false;
}
c.push(_48[i]);
}
if(c.length){
_4a.push(c);
}
return this._trim(_4a);
},getWidth:function(_4c,_4d){
return this._getWidth(_3.map(this._normalize(_4c).split(""),function(chr){
return this.glyphs[chr]||{xAdvance:this.advance.missing.x};
},this))*(_4d||1);
},getLineHeight:function(_4e){
return this.viewbox.height*(_4e||1);
},getCenterline:function(_4f){
return (_4f||1)*(this.viewbox.height/2);
},getBaseline:function(_50){
return (_50||1)*(this.viewbox.height+this.descent);
},draw:function(_51,_52,_53,_54,_55){
if(!this.initialized()){
throw new Error("dojox.gfx.VectorFont.draw(): we have not been initialized yet.");
}
var g=_51.createGroup();
if(_52.x||_52.y){
_51.applyTransform({dx:_52.x||0,dy:_52.y||0});
}
var _56=_3.map(this._normalize(_52.text).split(""),function(chr){
return this.glyphs[chr]||{path:null,xAdvance:this.advance.missing.x};
},this);
var _57=_53.size,_58=_52.fitting,_59=_52.width,_5a=_52.height,_5b=_52.align,_5c=_52.leading||this._defaultLeading;
if(_58){
if((_58==_6.vectorFontFitting.FLOW&&!_59)||(_58==_6.vectorFontFitting.FIT&&(!_59||!_5a))){
_58=_6.vectorFontFitting.NONE;
}
}
var _5d,_5e;
switch(_58){
case _6.vectorFontFitting.FIT:
var o=this._getBestFit(_56,_59,_5a,_5c);
_5e=o.scale;
_5d=o.lines;
break;
case _6.vectorFontFitting.FLOW:
_5e=this._getSizeFactor(_57);
_5d=this._getBestFlow(_56,_59,_5e);
break;
default:
_5e=this._getSizeFactor(_57);
_5d=[_56];
}
_5d=_3.filter(_5d,function(_5f){
return _5f.length>0;
});
var cy=0,_60=this._getLongestLine(_5d).width;
for(var i=0,l=_5d.length;i<l;i++){
var cx=0,_61=_5d[i],_62=this._getWidth(_61),lg=g.createGroup();
for(var j=0;j<_61.length;j++){
var _63=_61[j];
if(_63.path!==null){
var p=lg.createPath(_63.path).setFill(_54);
if(_55){
p.setStroke(_55);
}
p.setTransform([_9.flipY,_9.translate(cx,-this.viewbox.height-this.descent)]);
}
cx+=_63.xAdvance;
if(j+1<_61.length&&_63.kern&&_63.kern[_61[j+1].code]){
cx+=_63.kern[_61[j+1].code].x;
}
}
var dx=0;
if(_5b=="middle"){
dx=_60/2-_62/2;
}else{
if(_5b=="end"){
dx=_60-_62;
}
}
lg.setTransform({dx:dx,dy:cy});
cy+=this.viewbox.height*_5c;
}
g.setTransform(_9.scale(_5e));
return g;
},onLoadBegin:function(url){
},onLoad:function(_64){
}});
});
