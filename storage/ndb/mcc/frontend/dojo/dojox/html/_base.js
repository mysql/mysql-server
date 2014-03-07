//>>built
define("dojox/html/_base",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/xhr","dojo/_base/window","dojo/_base/sniff","dojo/_base/url","dojo/dom-construct","dojo/html","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_1.getObject("dojox.html",true);
if(_5("ie")){
var _a=/(AlphaImageLoader\([^)]*?src=(['"]))(?![a-z]+:|\/)([^\r\n;}]+?)(\2[^)]*\)\s*[;}]?)/g;
}
var _b=/(?:(?:@import\s*(['"])(?![a-z]+:|\/)([^\r\n;{]+?)\1)|url\(\s*(['"]?)(?![a-z]+:|\/)([^\r\n;]+?)\3\s*\))([a-z, \s]*[;}]?)/g;
var _c=_9._adjustCssPaths=function(_d,_e){
if(!_e||!_d){
return;
}
if(_a){
_e=_e.replace(_a,function(_f,pre,_10,url,_11){
return pre+(new _6(_d,"./"+url).toString())+_11;
});
}
return _e.replace(_b,function(_12,_13,_14,_15,_16,_17){
if(_14){
return "@import \""+(new _6(_d,"./"+_14).toString())+"\""+_17;
}else{
return "url("+(new _6(_d,"./"+_16).toString())+")"+_17;
}
});
};
var _18=/(<[a-z][a-z0-9]*\s[^>]*)(?:(href|src)=(['"]?)([^>]*?)\3|style=(['"]?)([^>]*?)\5)([^>]*>)/gi;
var _19=_9._adjustHtmlPaths=function(_1a,_1b){
var url=_1a||"./";
return _1b.replace(_18,function(tag,_1c,_1d,_1e,_1f,_20,_21,end){
return _1c+(_1d?(_1d+"="+_1e+(new _6(url,_1f).toString())+_1e):("style="+_20+_c(url,_21)+_20))+end;
});
};
var _22=_9._snarfStyles=function(_23,_24,_25){
_25.attributes=[];
return _24.replace(/(?:<style([^>]*)>([\s\S]*?)<\/style>|<link\s+(?=[^>]*rel=['"]?stylesheet)([^>]*?href=(['"])([^>]*?)\4[^>\/]*)\/?>)/gi,function(_26,_27,_28,_29,_2a,_2b){
var i,_2c=(_27||_29||"").replace(/^\s*([\s\S]*?)\s*$/i,"$1");
if(_28){
i=_25.push(_23?_c(_23,_28):_28);
}else{
i=_25.push("@import \""+_2b+"\";");
_2c=_2c.replace(/\s*(?:rel|href)=(['"])?[^\s]*\1\s*/gi,"");
}
if(_2c){
_2c=_2c.split(/\s+/);
var _2d={},tmp;
for(var j=0,e=_2c.length;j<e;j++){
tmp=_2c[j].split("=");
_2d[tmp[0]]=tmp[1].replace(/^\s*['"]?([\s\S]*?)['"]?\s*$/,"$1");
}
_25.attributes[i-1]=_2d;
}
return "";
});
};
var _2e=_9._snarfScripts=function(_2f,_30){
_30.code="";
_2f=_2f.replace(/<[!][-][-](.|\s)*?[-][-]>/g,function(_31){
return _31.replace(/<(\/?)script\b/ig,"&lt;$1Script");
});
function _32(src){
if(_30.downloadRemote){
src=src.replace(/&([a-z0-9#]+);/g,function(m,_33){
switch(_33){
case "amp":
return "&";
case "gt":
return ">";
case "lt":
return "<";
default:
return _33.charAt(0)=="#"?String.fromCharCode(_33.substring(1)):"&"+_33+";";
}
});
_3.get({url:src,sync:true,load:function(_34){
_30.code+=_34+";";
},error:_30.errBack});
}
};
return _2f.replace(/<script\s*(?![^>]*type=['"]?(?:dojo\/|text\/html\b))(?:[^>]*?(?:src=(['"]?)([^>]*?)\1[^>]*)?)*>([\s\S]*?)<\/script>/gi,function(_35,_36,src,_37){
if(src){
_32(src);
}else{
_30.code+=_37;
}
return "";
});
};
var _38=_9.evalInGlobal=function(_39,_3a){
_3a=_3a||_4.doc.body;
var n=_3a.ownerDocument.createElement("script");
n.type="text/javascript";
_3a.appendChild(n);
n.text=_39;
};
_9._ContentSetter=_1.declare(_8._ContentSetter,{adjustPaths:false,referencePath:".",renderStyles:false,executeScripts:false,scriptHasHooks:false,scriptHookReplacement:null,_renderStyles:function(_3b){
this._styleNodes=[];
var st,att,_3c,doc=this.node.ownerDocument;
var _3d=doc.getElementsByTagName("head")[0];
for(var i=0,e=_3b.length;i<e;i++){
_3c=_3b[i];
att=_3b.attributes[i];
st=doc.createElement("style");
st.setAttribute("type","text/css");
for(var x in att){
st.setAttribute(x,att[x]);
}
this._styleNodes.push(st);
_3d.appendChild(st);
if(st.styleSheet){
st.styleSheet.cssText=_3c;
}else{
st.appendChild(doc.createTextNode(_3c));
}
}
},empty:function(){
this.inherited("empty",arguments);
this._styles=[];
},onBegin:function(){
this.inherited("onBegin",arguments);
var _3e=this.content,_3f=this.node;
var _40=this._styles;
if(_2.isString(_3e)){
if(this.adjustPaths&&this.referencePath){
_3e=_19(this.referencePath,_3e);
}
if(this.renderStyles||this.cleanContent){
_3e=_22(this.referencePath,_3e,_40);
}
if(this.executeScripts){
var _41=this;
var _42={downloadRemote:true,errBack:function(e){
_41._onError.call(_41,"Exec","Error downloading remote script in \""+_41.id+"\"",e);
}};
_3e=_2e(_3e,_42);
this._code=_42.code;
}
}
this.content=_3e;
},onEnd:function(){
var _43=this._code,_44=this._styles;
if(this._styleNodes&&this._styleNodes.length){
while(this._styleNodes.length){
_7.destroy(this._styleNodes.pop());
}
}
if(this.renderStyles&&_44&&_44.length){
this._renderStyles(_44);
}
if(this.executeScripts&&_43){
if(this.cleanContent){
_43=_43.replace(/(<!--|(?:\/\/)?-->|<!\[CDATA\[|\]\]>)/g,"");
}
if(this.scriptHasHooks){
_43=_43.replace(/_container_(?!\s*=[^=])/g,this.scriptHookReplacement);
}
try{
_38(_43,this.node);
}
catch(e){
this._onError("Exec","Error eval script in "+this.id+", "+e.message,e);
}
}
this.inherited("onEnd",arguments);
},tearDown:function(){
this.inherited(arguments);
delete this._styles;
if(this._styleNodes&&this._styleNodes.length){
while(this._styleNodes.length){
_7.destroy(this._styleNodes.pop());
}
}
delete this._styleNodes;
_1.mixin(this,_9._ContentSetter.prototype);
}});
_9.set=function(_45,_46,_47){
if(!_47){
return _8._setNodeContent(_45,_46,true);
}else{
var op=new _9._ContentSetter(_1.mixin(_47,{content:_46,node:_45}));
return op.set();
}
};
return _9;
});
