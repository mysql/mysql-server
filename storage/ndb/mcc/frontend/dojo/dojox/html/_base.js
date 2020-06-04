//>>built
define("dojox/html/_base",["dojo/_base/declare","dojo/Deferred","dojo/dom-construct","dojo/html","dojo/_base/kernel","dojo/_base/lang","dojo/ready","dojo/_base/sniff","dojo/_base/url","dojo/_base/xhr","dojo/when","dojo/_base/window"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var _d=_5.getObject("dojox.html",true);
if(_8("ie")){
var _e=/(AlphaImageLoader\([^)]*?src=(['"]))(?![a-z]+:|\/)([^\r\n;}]+?)(\2[^)]*\)\s*[;}]?)/g;
}
var _f=/(?:(?:@import\s*(['"])(?![a-z]+:|\/)([^\r\n;{]+?)\1)|url\(\s*(['"]?)(?![a-z]+:|\/)([^\r\n;]+?)\3\s*\))([a-z, \s]*[;}]?)/g;
var _10=_d._adjustCssPaths=function(_11,_12){
if(!_12||!_11){
return;
}
if(_e){
_12=_12.replace(_e,function(_13,pre,_14,url,_15){
return pre+(new _9(_11,"./"+url).toString())+_15;
});
}
return _12.replace(_f,function(_16,_17,_18,_19,_1a,_1b){
if(_18){
return "@import \""+(new _9(_11,"./"+_18).toString())+"\""+_1b;
}else{
return "url("+(new _9(_11,"./"+_1a).toString())+")"+_1b;
}
});
};
var _1c=/(<[a-z][a-z0-9]*\s[^>]*)(?:(href|src)=(['"]?)([^>]*?)\3|style=(['"]?)([^>]*?)\5)([^>]*>)/gi;
var _1d=_d._adjustHtmlPaths=function(_1e,_1f){
var url=_1e||"./";
return _1f.replace(_1c,function(tag,_20,_21,_22,_23,_24,_25,end){
return _20+(_21?(_21+"="+_22+(new _9(url,_23).toString())+_22):("style="+_24+_10(url,_25)+_24))+end;
});
};
var _26=_d._snarfStyles=function(_27,_28,_29){
_29.attributes=[];
_28=_28.replace(/<[!][-][-](.|\s)*?[-][-]>/g,function(_2a){
return _2a.replace(/<(\/?)style\b/ig,"&lt;$1Style").replace(/<(\/?)link\b/ig,"&lt;$1Link").replace(/@import "/ig,"@ import \"");
});
return _28.replace(/(?:<style([^>]*)>([\s\S]*?)<\/style>|<link\s+(?=[^>]*rel=['"]?stylesheet)([^>]*?href=(['"])([^>]*?)\4[^>\/]*)\/?>)/gi,function(_2b,_2c,_2d,_2e,_2f,_30){
var i,_31=(_2c||_2e||"").replace(/^\s*([\s\S]*?)\s*$/i,"$1");
if(_2d){
i=_29.push(_27?_10(_27,_2d):_2d);
}else{
i=_29.push("@import \""+_30+"\";");
_31=_31.replace(/\s*(?:rel|href)=(['"])?[^\s]*\1\s*/gi,"");
}
if(_31){
_31=_31.split(/\s+/);
var _32={},tmp;
for(var j=0,e=_31.length;j<e;j++){
tmp=_31[j].split("=");
_32[tmp[0]]=tmp[1].replace(/^\s*['"]?([\s\S]*?)['"]?\s*$/,"$1");
}
_29.attributes[i-1]=_32;
}
return "";
});
};
var _33=_d._snarfScripts=function(_34,_35){
_35.code="";
_34=_34.replace(/<[!][-][-](.|\s)*?[-][-]>/g,function(_36){
return _36.replace(/<(\/?)script\b/ig,"&lt;$1Script");
});
function _37(src){
if(_35.downloadRemote){
src=src.replace(/&([a-z0-9#]+);/g,function(m,_38){
switch(_38){
case "amp":
return "&";
case "gt":
return ">";
case "lt":
return "<";
default:
return _38.charAt(0)=="#"?String.fromCharCode(_38.substring(1)):"&"+_38+";";
}
});
_a.get({url:src,sync:true,load:function(_39){
if(_35.code!==""){
_39="\n"+_39;
}
_35.code+=_39+";";
},error:_35.errBack});
}
};
return _34.replace(/<script\s*(?![^>]*type=['"]?(?:dojo\/|text\/html\b))[^>]*?(?:src=(['"]?)([^>]*?)\1[^>]*)?>([\s\S]*?)<\/script>/gi,function(_3a,_3b,src,_3c){
if(src){
_37(src);
}else{
if(_35.code!==""){
_3c="\n"+_3c;
}
_35.code+=_3c+";";
}
return "";
});
};
var _3d=_d.evalInGlobal=function(_3e,_3f){
_3f=_3f||_c.doc.body;
var n=_3f.ownerDocument.createElement("script");
n.type="text/javascript";
_3f.appendChild(n);
n.text=_3e;
};
_d._ContentSetter=_1(_4._ContentSetter,{adjustPaths:false,referencePath:".",renderStyles:false,executeScripts:false,scriptHasHooks:false,scriptHookReplacement:null,_renderStyles:function(_40){
this._styleNodes=[];
var st,att,_41,doc=this.node.ownerDocument;
var _42=doc.getElementsByTagName("head")[0];
for(var i=0,e=_40.length;i<e;i++){
_41=_40[i];
att=_40.attributes[i];
st=doc.createElement("style");
st.setAttribute("type","text/css");
for(var x in att){
st.setAttribute(x,att[x]);
}
this._styleNodes.push(st);
_42.appendChild(st);
if(st.styleSheet){
st.styleSheet.cssText=_41;
}else{
st.appendChild(doc.createTextNode(_41));
}
}
},empty:function(){
this.inherited("empty",arguments);
this._styles=[];
},onBegin:function(){
this.inherited("onBegin",arguments);
var _43=this.content,_44=this.node;
var _45=this._styles;
this._code=null;
if(_6.isString(_43)){
if(this.adjustPaths&&this.referencePath){
_43=_1d(this.referencePath,_43);
}
if(this.renderStyles||this.cleanContent){
_43=_26(this.referencePath,_43,_45);
}
if(this.executeScripts){
var _46=this;
var _47={downloadRemote:true,errBack:function(e){
_46._onError.call(_46,"Exec","Error downloading remote script in \""+_46.id+"\"",e);
}};
_43=_33(_43,_47);
this._code=_47.code;
}
}
this.content=_43;
},onEnd:function(){
var _48=this._code,_49=this._styles;
if(this._styleNodes&&this._styleNodes.length){
while(this._styleNodes.length){
_3.destroy(this._styleNodes.pop());
}
}
if(this.renderStyles&&_49&&_49.length){
this._renderStyles(_49);
}
var d=new _2();
var _4a=this.getInherited(arguments),_4b=arguments,_4c=_6.hitch(this,function(){
_4a.apply(this,_4b);
_b(this.parseDeferred,function(){
d.resolve();
});
});
if(this.executeScripts&&_48){
if(this.cleanContent){
_48=_48.replace(/(<!--|(?:\/\/)?-->|<!\[CDATA\[|\]\]>)/g,"");
}
if(this.scriptHasHooks){
_48=_48.replace(/_container_(?!\s*=[^=])/g,this.scriptHookReplacement);
}
try{
_3d(_48,this.node);
}
catch(e){
this._onError("Exec","Error eval script in "+this.id+", "+e.message,e);
}
_7(_4c);
}else{
_4c();
}
return d.promise;
},tearDown:function(){
this.inherited(arguments);
delete this._styles;
if(this._styleNodes&&this._styleNodes.length){
while(this._styleNodes.length){
_3.destroy(this._styleNodes.pop());
}
}
delete this._styleNodes;
_6.mixin(this,_d._ContentSetter.prototype);
}});
_d.set=function(_4d,_4e,_4f){
if(!_4f){
return _4._setNodeContent(_4d,_4e,true);
}else{
var op=new _d._ContentSetter(_6.mixin(_4f,{content:_4e,node:_4d}));
return op.set();
}
};
return _d;
});
