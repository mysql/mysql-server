//>>built
define("dojox/data/OpenSearchStore",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/xhr","dojo/_base/array","dojo/_base/window","dojo/query","dojo/data/util/simpleFetch","dojox/xml/parser"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
_1.experimental("dojox.data.OpenSearchStore");
var _a=_3("dojox.data.OpenSearchStore",null,{constructor:function(_b){
if(_b){
this.label=_b.label;
this.url=_b.url;
this.itemPath=_b.itemPath;
if("urlPreventCache" in _b){
this.urlPreventCache=_b.urlPreventCache?true:false;
}
}
var _c=_4.get({url:this.url,handleAs:"xml",sync:true,preventCache:this.urlPreventCache});
_c.addCallback(this,"_processOsdd");
_c.addErrback(function(){
throw new Error("Unable to load OpenSearch Description document from ".args.url);
});
},url:"",itemPath:"",_storeRef:"_S",urlElement:null,iframeElement:null,urlPreventCache:true,ATOM_CONTENT_TYPE:3,ATOM_CONTENT_TYPE_STRING:"atom",RSS_CONTENT_TYPE:2,RSS_CONTENT_TYPE_STRING:"rss",XML_CONTENT_TYPE:1,XML_CONTENT_TYPE_STRING:"xml",_assertIsItem:function(_d){
if(!this.isItem(_d)){
throw new Error("dojox.data.OpenSearchStore: a function was passed an item argument that was not an item");
}
},_assertIsAttribute:function(_e){
if(typeof _e!=="string"){
throw new Error("dojox.data.OpenSearchStore: a function was passed an attribute argument that was not an attribute name string");
}
},getFeatures:function(){
return {"dojo.data.api.Read":true};
},getValue:function(_f,_10,_11){
var _12=this.getValues(_f,_10);
if(_12){
return _12[0];
}
return _11;
},getAttributes:function(_13){
return ["content"];
},hasAttribute:function(_14,_15){
if(this.getValue(_14,_15)){
return true;
}
return false;
},isItemLoaded:function(_16){
return this.isItem(_16);
},loadItem:function(_17){
},getLabel:function(_18){
return undefined;
},getLabelAttributes:function(_19){
return null;
},containsValue:function(_1a,_1b,_1c){
var _1d=this.getValues(_1a,_1b);
for(var i=0;i<_1d.length;i++){
if(_1d[i]===_1c){
return true;
}
}
return false;
},getValues:function(_1e,_1f){
this._assertIsItem(_1e);
this._assertIsAttribute(_1f);
var _20=this.processItem(_1e,_1f);
if(_20){
return [_20];
}
return undefined;
},isItem:function(_21){
if(_21&&_21[this._storeRef]===this){
return true;
}
return false;
},close:function(_22){
},process:function(_23){
return this["_processOSD"+this.contentType](_23);
},processItem:function(_24,_25){
return this["_processItem"+this.contentType](_24.node,_25);
},_createSearchUrl:function(_26){
var _27=this.urlElement.attributes.getNamedItem("template").nodeValue;
var _28=this.urlElement.attributes;
var _29=_27.indexOf("{searchTerms}");
_27=_27.substring(0,_29)+_26.query.searchTerms+_27.substring(_29+13);
_5.forEach([{"name":"count","test":_26.count,"def":"10"},{"name":"startIndex","test":_26.start,"def":this.urlElement.attributes.getNamedItem("indexOffset")?this.urlElement.attributes.getNamedItem("indexOffset").nodeValue:0},{"name":"startPage","test":_26.startPage,"def":this.urlElement.attributes.getNamedItem("pageOffset")?this.urlElement.attributes.getNamedItem("pageOffset").nodeValue:0},{"name":"language","test":_26.language,"def":"*"},{"name":"inputEncoding","test":_26.inputEncoding,"def":"UTF-8"},{"name":"outputEncoding","test":_26.outputEncoding,"def":"UTF-8"}],function(_2a){
_27=_27.replace("{"+_2a.name+"}",_2a.test||_2a.def);
_27=_27.replace("{"+_2a.name+"?}",_2a.test||_2a.def);
});
return _27;
},_fetchItems:function(_2b,_2c,_2d){
if(!_2b.query){
_2b.query={};
}
var _2e=this;
var url=this._createSearchUrl(_2b);
var _2f={url:url,preventCache:this.urlPreventCache};
var xhr=_4.get(_2f);
xhr.addErrback(function(_30){
_2d(_30,_2b);
});
xhr.addCallback(function(_31){
var _32=[];
if(_31){
_32=_2e.process(_31);
for(var i=0;i<_32.length;i++){
_32[i]={node:_32[i]};
_32[i][_2e._storeRef]=_2e;
}
}
_2c(_32,_2b);
});
},_processOSDxml:function(_33){
var div=_6.doc.createElement("div");
div.innerHTML=_33;
return _7(this.itemPath,div);
},_processItemxml:function(_34,_35){
if(_35==="content"){
return _34.innerHTML;
}
return undefined;
},_processOSDatom:function(_36){
return this._processOSDfeed(_36,"entry");
},_processItematom:function(_37,_38){
return this._processItemfeed(_37,_38,"content");
},_processOSDrss:function(_39){
return this._processOSDfeed(_39,"item");
},_processItemrss:function(_3a,_3b){
return this._processItemfeed(_3a,_3b,"description");
},_processOSDfeed:function(_3c,_3d){
_3c=dojox.xml.parser.parse(_3c);
var _3e=[];
var _3f=_3c.getElementsByTagName(_3d);
for(var i=0;i<_3f.length;i++){
_3e.push(_3f.item(i));
}
return _3e;
},_processItemfeed:function(_40,_41,_42){
if(_41==="content"){
var _43=_40.getElementsByTagName(_42).item(0);
return this._getNodeXml(_43,true);
}
return undefined;
},_getNodeXml:function(_44,_45){
var i;
switch(_44.nodeType){
case 1:
var xml=[];
if(!_45){
xml.push("<"+_44.tagName);
var _46;
for(i=0;i<_44.attributes.length;i++){
_46=_44.attributes.item(i);
xml.push(" "+_46.nodeName+"=\""+_46.nodeValue+"\"");
}
xml.push(">");
}
for(i=0;i<_44.childNodes.length;i++){
xml.push(this._getNodeXml(_44.childNodes.item(i)));
}
if(!_45){
xml.push("</"+_44.tagName+">\n");
}
return xml.join("");
case 3:
case 4:
return _44.nodeValue;
}
return undefined;
},_processOsdd:function(doc){
var _47=doc.getElementsByTagName("Url");
var _48=[];
var _49;
var i;
for(i=0;i<_47.length;i++){
_49=_47[i].attributes.getNamedItem("type").nodeValue;
switch(_49){
case "application/rss+xml":
_48[i]=this.RSS_CONTENT_TYPE;
break;
case "application/atom+xml":
_48[i]=this.ATOM_CONTENT_TYPE;
break;
default:
_48[i]=this.XML_CONTENT_TYPE;
break;
}
}
var _4a=0;
var _4b=_48[0];
for(i=1;i<_47.length;i++){
if(_48[i]>_4b){
_4a=i;
_4b=_48[i];
}
}
var _4c=_47[_4a].nodeName.toLowerCase();
if(_4c=="url"){
var _4d=_47[_4a].attributes;
this.urlElement=_47[_4a];
switch(_48[_4a]){
case this.ATOM_CONTENT_TYPE:
this.contentType=this.ATOM_CONTENT_TYPE_STRING;
break;
case this.RSS_CONTENT_TYPE:
this.contentType=this.RSS_CONTENT_TYPE_STRING;
break;
case this.XML_CONTENT_TYPE:
this.contentType=this.XML_CONTENT_TYPE_STRING;
break;
}
}
}});
return _2.extend(_a,_8);
});
