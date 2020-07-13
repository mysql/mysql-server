//>>built
define("dojox/data/AtomReadStore",["dojo","dojox","dojo/data/util/filter","dojo/data/util/simpleFetch","dojo/date/stamp"],function(_1,_2){
_1.experimental("dojox.data.AtomReadStore");
var _3=_1.declare("dojox.data.AtomReadStore",null,{constructor:function(_4){
if(_4){
this.url=_4.url;
this.rewriteUrl=_4.rewriteUrl;
this.label=_4.label||this.label;
this.sendQuery=(_4.sendQuery||_4.sendquery||this.sendQuery);
this.unescapeHTML=_4.unescapeHTML;
if("urlPreventCache" in _4){
this.urlPreventCache=_4.urlPreventCache?true:false;
}
}
if(!this.url){
throw new Error("AtomReadStore: a URL must be specified when creating the data store");
}
},url:"",label:"title",sendQuery:false,unescapeHTML:false,urlPreventCache:false,getValue:function(_5,_6,_7){
this._assertIsItem(_5);
this._assertIsAttribute(_6);
this._initItem(_5);
_6=_6.toLowerCase();
if(!_5._attribs[_6]&&!_5._parsed){
this._parseItem(_5);
_5._parsed=true;
}
var _8=_5._attribs[_6];
if(!_8&&_6=="summary"){
var _9=this.getValue(_5,"content");
var _a=new RegExp("/(<([^>]+)>)/g","i");
var _b=_9.text.replace(_a,"");
_8={text:_b.substring(0,Math.min(400,_b.length)),type:"text"};
_5._attribs[_6]=_8;
}
if(_8&&this.unescapeHTML){
if((_6=="content"||_6=="summary"||_6=="subtitle")&&!_5["_"+_6+"Escaped"]){
_8.text=this._unescapeHTML(_8.text);
_5["_"+_6+"Escaped"]=true;
}
}
return _8?_1.isArray(_8)?_8[0]:_8:_7;
},getValues:function(_c,_d){
this._assertIsItem(_c);
this._assertIsAttribute(_d);
this._initItem(_c);
_d=_d.toLowerCase();
if(!_c._attribs[_d]){
this._parseItem(_c);
}
var _e=_c._attribs[_d];
return _e?((_e.length!==undefined&&typeof (_e)!=="string")?_e:[_e]):undefined;
},getAttributes:function(_f){
this._assertIsItem(_f);
if(!_f._attribs){
this._initItem(_f);
this._parseItem(_f);
}
var _10=[];
for(var x in _f._attribs){
_10.push(x);
}
return _10;
},hasAttribute:function(_11,_12){
return (this.getValue(_11,_12)!==undefined);
},containsValue:function(_13,_14,_15){
var _16=this.getValues(_13,_14);
for(var i=0;i<_16.length;i++){
if((typeof _15==="string")){
if(_16[i].toString&&_16[i].toString()===_15){
return true;
}
}else{
if(_16[i]===_15){
return true;
}
}
}
return false;
},isItem:function(_17){
if(_17&&_17.element&&_17.store&&_17.store===this){
return true;
}
return false;
},isItemLoaded:function(_18){
return this.isItem(_18);
},loadItem:function(_19){
},getFeatures:function(){
var _1a={"dojo.data.api.Read":true};
return _1a;
},getLabel:function(_1b){
if((this.label!=="")&&this.isItem(_1b)){
var _1c=this.getValue(_1b,this.label);
if(_1c&&_1c.text){
return _1c.text;
}else{
if(_1c){
return _1c.toString();
}else{
return undefined;
}
}
}
return undefined;
},getLabelAttributes:function(_1d){
if(this.label!==""){
return [this.label];
}
return null;
},getFeedValue:function(_1e,_1f){
var _20=this.getFeedValues(_1e,_1f);
if(_1.isArray(_20)){
return _20[0];
}
return _20;
},getFeedValues:function(_21,_22){
if(!this.doc){
return _22;
}
if(!this._feedMetaData){
this._feedMetaData={element:this.doc.getElementsByTagName("feed")[0],store:this,_attribs:{}};
this._parseItem(this._feedMetaData);
}
return this._feedMetaData._attribs[_21]||_22;
},_initItem:function(_23){
if(!_23._attribs){
_23._attribs={};
}
},_fetchItems:function(_24,_25,_26){
var url=this._getFetchUrl(_24);
if(!url){
_26(new Error("No URL specified."));
return;
}
var _27=(!this.sendQuery?_24:null);
var _28=this;
var _29=function(_2a){
_28.doc=_2a;
var _2b=_28._getItems(_2a,_27);
var _2c=_24.query;
if(_2c){
if(_2c.id){
_2b=_1.filter(_2b,function(_2d){
return (_28.getValue(_2d,"id")==_2c.id);
});
}else{
if(_2c.category){
_2b=_1.filter(_2b,function(_2e){
var _2f=_28.getValues(_2e,"category");
if(!_2f){
return false;
}
return _1.some(_2f,"return item.term=='"+_2c.category+"'");
});
}
}
}
if(_2b&&_2b.length>0){
_25(_2b,_24);
}else{
_25([],_24);
}
};
if(this.doc){
_29(this.doc);
}else{
var _30={url:url,handleAs:"xml",preventCache:this.urlPreventCache};
var _31=_1.xhrGet(_30);
_31.addCallback(_29);
_31.addErrback(function(_32){
_26(_32,_24);
});
}
},_getFetchUrl:function(_33){
if(!this.sendQuery){
return this.url;
}
var _34=_33.query;
if(!_34){
return this.url;
}
if(_1.isString(_34)){
return this.url+_34;
}
var _35="";
for(var _36 in _34){
var _37=_34[_36];
if(_37){
if(_35){
_35+="&";
}
_35+=(_36+"="+_37);
}
}
if(!_35){
return this.url;
}
var _38=this.url;
if(_38.indexOf("?")<0){
_38+="?";
}else{
_38+="&";
}
return _38+_35;
},_getItems:function(_39,_3a){
if(this._items){
return this._items;
}
var _3b=[];
var _3c=[];
if(_39.childNodes.length<1){
this._items=_3b;
return _3b;
}
var _3d=_1.filter(_39.childNodes,"return item.tagName && item.tagName.toLowerCase() == 'feed'");
var _3e=_3a.query;
if(!_3d||_3d.length!=1){
return _3b;
}
_3c=_1.filter(_3d[0].childNodes,"return item.tagName && item.tagName.toLowerCase() == 'entry'");
if(_3a.onBegin){
_3a.onBegin(_3c.length,this.sendQuery?_3a:{});
}
for(var i=0;i<_3c.length;i++){
var _3f=_3c[i];
if(_3f.nodeType!=1){
continue;
}
_3b.push(this._getItem(_3f));
}
this._items=_3b;
return _3b;
},close:function(_40){
},_getItem:function(_41){
return {element:_41,store:this};
},_parseItem:function(_42){
var _43=_42._attribs;
var _44=this;
var _45,_46;
function _47(_48){
var txt=_48.textContent||_48.innerHTML||_48.innerXML;
if(!txt&&_48.childNodes[0]){
var _49=_48.childNodes[0];
if(_49&&(_49.nodeType==3||_49.nodeType==4)){
txt=_48.childNodes[0].nodeValue;
}
}
return txt;
};
function _4a(_4b){
return {text:_47(_4b),type:_4b.getAttribute("type")};
};
_1.forEach(_42.element.childNodes,function(_4c){
var _4d=_4c.tagName?_4c.tagName.toLowerCase():"";
switch(_4d){
case "title":
_43[_4d]={text:_47(_4c),type:_4c.getAttribute("type")};
break;
case "subtitle":
case "summary":
case "content":
_43[_4d]=_4a(_4c);
break;
case "author":
var _4e,_4f;
_1.forEach(_4c.childNodes,function(_50){
if(!_50.tagName){
return;
}
switch(_50.tagName.toLowerCase()){
case "name":
_4e=_50;
break;
case "uri":
_4f=_50;
break;
}
});
var _51={};
if(_4e&&_4e.length==1){
_51.name=_47(_4e[0]);
}
if(_4f&&_4f.length==1){
_51.uri=_47(_4f[0]);
}
_43[_4d]=_51;
break;
case "id":
_43[_4d]=_47(_4c);
break;
case "updated":
_43[_4d]=_1.date.stamp.fromISOString(_47(_4c));
break;
case "published":
_43[_4d]=_1.date.stamp.fromISOString(_47(_4c));
break;
case "category":
if(!_43[_4d]){
_43[_4d]=[];
}
_43[_4d].push({scheme:_4c.getAttribute("scheme"),term:_4c.getAttribute("term")});
break;
case "link":
if(!_43[_4d]){
_43[_4d]=[];
}
var _52={rel:_4c.getAttribute("rel"),href:_4c.getAttribute("href"),type:_4c.getAttribute("type")};
_43[_4d].push(_52);
if(_52.rel=="alternate"){
_43["alternate"]=_52;
}
break;
default:
break;
}
});
},_unescapeHTML:function(_53){
_53=_53.replace(/&#8217;/m,"'").replace(/&#8243;/m,"\"").replace(/&#60;/m,">").replace(/&#62;/m,"<").replace(/&#38;/m,"&");
return _53;
},_assertIsItem:function(_54){
if(!this.isItem(_54)){
throw new Error("dojox.data.AtomReadStore: Invalid item argument.");
}
},_assertIsAttribute:function(_55){
if(typeof _55!=="string"){
throw new Error("dojox.data.AtomReadStore: Invalid attribute argument.");
}
}});
_1.extend(_3,_1.data.util.simpleFetch);
return _3;
});
