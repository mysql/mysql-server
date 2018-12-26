//>>built
define("dojox/gfx/utils",["dojo/_base/kernel","dojo/_base/lang","./_base","dojo/_base/html","dojo/_base/array","dojo/_base/window","dojo/_base/json","dojo/_base/Deferred","dojo/_base/sniff","require","dojo/_base/config"],function(_1,_2,g,_3,_4,_5,_6,_7,_8,_9,_a){
var gu=g.utils={};
_2.mixin(gu,{forEach:function(_b,f,o){
o=o||_5.global;
f.call(o,_b);
if(_b instanceof g.Surface||_b instanceof g.Group){
_4.forEach(_b.children,function(_c){
gu.forEach(_c,f,o);
});
}
},serialize:function(_d){
var t={},v,_e=_d instanceof g.Surface;
if(_e||_d instanceof g.Group){
t.children=_4.map(_d.children,gu.serialize);
if(_e){
return t.children;
}
}else{
t.shape=_d.getShape();
}
if(_d.getTransform){
v=_d.getTransform();
if(v){
t.transform=v;
}
}
if(_d.getStroke){
v=_d.getStroke();
if(v){
t.stroke=v;
}
}
if(_d.getFill){
v=_d.getFill();
if(v){
t.fill=v;
}
}
if(_d.getFont){
v=_d.getFont();
if(v){
t.font=v;
}
}
return t;
},toJson:function(_f,_10){
return _6.toJson(gu.serialize(_f),_10);
},deserialize:function(_11,_12){
if(_12 instanceof Array){
return _4.map(_12,_2.hitch(null,gu.deserialize,_11));
}
var _13=("shape" in _12)?_11.createShape(_12.shape):_11.createGroup();
if("transform" in _12){
_13.setTransform(_12.transform);
}
if("stroke" in _12){
_13.setStroke(_12.stroke);
}
if("fill" in _12){
_13.setFill(_12.fill);
}
if("font" in _12){
_13.setFont(_12.font);
}
if("children" in _12){
_4.forEach(_12.children,_2.hitch(null,gu.deserialize,_13));
}
return _13;
},fromJson:function(_14,_15){
return gu.deserialize(_14,_6.fromJson(_15));
},toSvg:function(_16){
var _17=new _7();
if(g.renderer==="svg"){
try{
var svg=gu._cleanSvg(gu._innerXML(_16.rawNode));
_17.callback(svg);
}
catch(e){
_17.errback(e);
}
}else{
if(!gu._initSvgSerializerDeferred){
gu._initSvgSerializer();
}
var _18=gu.toJson(_16);
var _19=function(){
try{
var _1a=_16.getDimensions();
var _1b=_1a.width;
var _1c=_1a.height;
var _1d=gu._gfxSvgProxy.document.createElement("div");
gu._gfxSvgProxy.document.body.appendChild(_1d);
_5.withDoc(gu._gfxSvgProxy.document,function(){
_3.style(_1d,"width",_1b);
_3.style(_1d,"height",_1c);
},this);
var ts=gu._gfxSvgProxy[dojox._scopeName].gfx.createSurface(_1d,_1b,_1c);
var _1e=function(_1f){
try{
gu._gfxSvgProxy[dojox._scopeName].gfx.utils.fromJson(_1f,_18);
var svg=gu._cleanSvg(_1d.innerHTML);
_1f.clear();
_1f.destroy();
gu._gfxSvgProxy.document.body.removeChild(_1d);
_17.callback(svg);
}
catch(e){
_17.errback(e);
}
};
ts.whenLoaded(null,_1e);
}
catch(ex){
_17.errback(ex);
}
};
if(gu._initSvgSerializerDeferred.fired>0){
_19();
}else{
gu._initSvgSerializerDeferred.addCallback(_19);
}
}
return _17;
},_gfxSvgProxy:null,_initSvgSerializerDeferred:null,_svgSerializerInitialized:function(){
gu._initSvgSerializerDeferred.callback(true);
},_initSvgSerializer:function(){
if(!gu._initSvgSerializerDeferred){
gu._initSvgSerializerDeferred=new _7();
var f=_5.doc.createElement("iframe");
_3.style(f,{display:"none",position:"absolute",width:"1em",height:"1em",top:"-10000px"});
var _20;
if(_8("ie")){
f.onreadystatechange=function(){
if(f.contentWindow.document.readyState=="complete"){
f.onreadystatechange=function(){
};
_20=setInterval(function(){
if(f.contentWindow[_1.scopeMap["dojo"][1]._scopeName]&&f.contentWindow[_1.scopeMap["dojox"][1]._scopeName].gfx&&f.contentWindow[_1.scopeMap["dojox"][1]._scopeName].gfx.utils){
clearInterval(_20);
f.contentWindow.parent[_1.scopeMap["dojox"][1]._scopeName].gfx.utils._gfxSvgProxy=f.contentWindow;
f.contentWindow.parent[_1.scopeMap["dojox"][1]._scopeName].gfx.utils._svgSerializerInitialized();
}
},50);
}
};
}else{
f.onload=function(){
f.onload=function(){
};
_20=setInterval(function(){
if(f.contentWindow[_1.scopeMap["dojo"][1]._scopeName]&&f.contentWindow[_1.scopeMap["dojox"][1]._scopeName].gfx&&f.contentWindow[_1.scopeMap["dojox"][1]._scopeName].gfx.utils){
clearInterval(_20);
f.contentWindow.parent[_1.scopeMap["dojox"][1]._scopeName].gfx.utils._gfxSvgProxy=f.contentWindow;
f.contentWindow.parent[_1.scopeMap["dojox"][1]._scopeName].gfx.utils._svgSerializerInitialized();
}
},50);
};
}
var uri=(_a["dojoxGfxSvgProxyFrameUrl"]||_9.toUrl("dojox/gfx/resources/gfxSvgProxyFrame.html"));
f.setAttribute("src",uri.toString());
_5.body().appendChild(f);
}
},_innerXML:function(_21){
if(_21.innerXML){
return _21.innerXML;
}else{
if(_21.xml){
return _21.xml;
}else{
if(typeof XMLSerializer!="undefined"){
return (new XMLSerializer()).serializeToString(_21);
}
}
}
return null;
},_cleanSvg:function(svg){
if(svg){
if(svg.indexOf("xmlns=\"http://www.w3.org/2000/svg\"")==-1){
svg=svg.substring(4,svg.length);
svg="<svg xmlns=\"http://www.w3.org/2000/svg\""+svg;
}
if(svg.indexOf("xmlns:xlink=\"http://www.w3.org/1999/xlink\"")==-1){
svg=svg.substring(4,svg.length);
svg="<svg xmlns:xlink=\"http://www.w3.org/1999/xlink\""+svg;
}
if(svg.indexOf("xlink:href")===-1){
svg=svg.replace(/href\s*=/g,"xlink:href=");
}
svg=svg.replace(/\bdojoGfx\w*\s*=\s*(['"])\w*\1/g,"");
svg=svg.replace(/\b__gfxObject__\s*=\s*(['"])\w*\1/g,"");
svg=svg.replace(/[=]([^"']+?)(\s|>)/g,"=\"$1\"$2");
}
return svg;
}});
return gu;
});
