//>>built
define("dojox/data/AtomReadStore",["dojo","dojox","dojo/data/util/filter","dojo/data/util/simpleFetch","dojo/date/stamp"],function(_1,_2){
_1.experimental("dojox.data.AtomReadStore");
_1.declare("dojox.data.AtomReadStore",null,{constructor:function(_3){
if(_3){
this.url=_3.url;
this.rewriteUrl=_3.rewriteUrl;
this.label=_3.label||this.label;
this.sendQuery=(_3.sendQuery||_3.sendquery||this.sendQuery);
this.unescapeHTML=_3.unescapeHTML;
if("urlPreventCache" in _3){
this.urlPreventCache=_3.urlPreventCache?true:false;
}
}
if(!this.url){
throw new Error("AtomReadStore: a URL must be specified when creating the data store");
}
},url:"",label:"title",sendQuery:false,unescapeHTML:false,urlPreventCache:false,getValue:function(_4,_5,_6){
this._assertIsItem(_4);
this._assertIsAttribute(_5);
this._initItem(_4);
_5=_5.toLowerCase();
if(!_4._attribs[_5]&&!_4._parsed){
this._parseItem(_4);
_4._parsed=true;
}
var _7=_4._attribs[_5];
if(!_7&&_5=="summary"){
var _8=this.getValue(_4,"content");
var _9=new RegExp("/(<([^>]+)>)/g","i");
var _a=_8.text.replace(_9,"");
_7={text:_a.substring(0,Math.min(400,_a.length)),type:"text"};
_4._attribs[_5]=_7;
}
if(_7&&this.unescapeHTML){
if((_5=="content"||_5=="summary"||_5=="subtitle")&&!_4["_"+_5+"Escaped"]){
_7.text=this._unescapeHTML(_7.text);
_4["_"+_5+"Escaped"]=true;
}
}
return _7?_1.isArray(_7)?_7[0]:_7:_6;
},getValues:function(_b,_c){
this._assertIsItem(_b);
this._assertIsAttribute(_c);
this._initItem(_b);
_c=_c.toLowerCase();
if(!_b._attribs[_c]){
this._parseItem(_b);
}
var _d=_b._attribs[_c];
return _d?((_d.length!==undefined&&typeof (_d)!=="string")?_d:[_d]):undefined;
},getAttributes:function(_e){
this._assertIsItem(_e);
if(!_e._attribs){
this._initItem(_e);
this._parseItem(_e);
}
var _f=[];
for(var x in _e._attribs){
_f.push(x);
}
return _f;
},hasAttribute:function(_10,_11){
return (this.getValue(_10,_11)!==undefined);
},containsValue:function(_12,_13,_14){
var _15=this.getValues(_12,_13);
for(var i=0;i<_15.length;i++){
if((typeof _14==="string")){
if(_15[i].toString&&_15[i].toString()===_14){
return true;
}
}else{
if(_15[i]===_14){
return true;
}
}
}
return false;
},isItem:function(_16){
if(_16&&_16.element&&_16.store&&_16.store===this){
return true;
}
return false;
},isItemLoaded:function(_17){
return this.isItem(_17);
},loadItem:function(_18){
},getFeatures:function(){
var _19={"dojo.data.api.Read":true};
return _19;
},getLabel:function(_1a){
if((this.label!=="")&&this.isItem(_1a)){
var _1b=this.getValue(_1a,this.label);
if(_1b&&_1b.text){
return _1b.text;
}else{
if(_1b){
return _1b.toString();
}else{
return undefined;
}
}
}
return undefined;
},getLabelAttributes:function(_1c){
if(this.label!==""){
return [this.label];
}
return null;
},getFeedValue:function(_1d,_1e){
var _1f=this.getFeedValues(_1d,_1e);
if(_1.isArray(_1f)){
return _1f[0];
}
return _1f;
},getFeedValues:function(_20,_21){
if(!this.doc){
return _21;
}
if(!this._feedMetaData){
this._feedMetaData={element:this.doc.getElementsByTagName("feed")[0],store:this,_attribs:{}};
this._parseItem(this._feedMetaData);
}
return this._feedMetaData._attribs[_20]||_21;
},_initItem:function(_22){
if(!_22._attribs){
_22._attribs={};
}
},_fetchItems:function(_23,_24,_25){
var url=this._getFetchUrl(_23);
if(!url){
_25(new Error("No URL specified."));
return;
}
var _26=(!this.sendQuery?_23:null);
var _27=this;
var _28=function(_29){
_27.doc=_29;
var _2a=_27._getItems(_29,_26);
var _2b=_23.query;
if(_2b){
if(_2b.id){
_2a=_1.filter(_2a,function(_2c){
return (_27.getValue(_2c,"id")==_2b.id);
});
}else{
if(_2b.category){
_2a=_1.filter(_2a,function(_2d){
var _2e=_27.getValues(_2d,"category");
if(!_2e){
return false;
}
return _1.some(_2e,"return item.term=='"+_2b.category+"'");
});
}
}
}
if(_2a&&_2a.length>0){
_24(_2a,_23);
}else{
_24([],_23);
}
};
if(this.doc){
_28(this.doc);
}else{
var _2f={url:url,handleAs:"xml",preventCache:this.urlPreventCache};
var _30=_1.xhrGet(_2f);
_30.addCallback(_28);
_30.addErrback(function(_31){
_25(_31,_23);
});
}
},_getFetchUrl:function(_32){
if(!this.sendQuery){
return this.url;
}
var _33=_32.query;
if(!_33){
return this.url;
}
if(_1.isString(_33)){
return this.url+_33;
}
var _34="";
for(var _35 in _33){
var _36=_33[_35];
if(_36){
if(_34){
_34+="&";
}
_34+=(_35+"="+_36);
}
}
if(!_34){
return this.url;
}
var _37=this.url;
if(_37.indexOf("?")<0){
_37+="?";
}else{
_37+="&";
}
return _37+_34;
},_getItems:function(_38,_39){
if(this._items){
return this._items;
}
var _3a=[];
var _3b=[];
if(_38.childNodes.length<1){
this._items=_3a;
return _3a;
}
var _3c=_1.filter(_38.childNodes,"return item.tagName && item.tagName.toLowerCase() == 'feed'");
var _3d=_39.query;
if(!_3c||_3c.length!=1){
return _3a;
}
_3b=_1.filter(_3c[0].childNodes,"return item.tagName && item.tagName.toLowerCase() == 'entry'");
if(_39.onBegin){
_39.onBegin(_3b.length,this.sendQuery?_39:{});
}
for(var i=0;i<_3b.length;i++){
var _3e=_3b[i];
if(_3e.nodeType!=1){
continue;
}
_3a.push(this._getItem(_3e));
}
this._items=_3a;
return _3a;
},close:function(_3f){
},_getItem:function(_40){
return {element:_40,store:this};
},_parseItem:function(_41){
var _42=_41._attribs;
var _43=this;
var _44,_45;
function _46(_47){
var txt=_47.textContent||_47.innerHTML||_47.innerXML;
if(!txt&&_47.childNodes[0]){
var _48=_47.childNodes[0];
if(_48&&(_48.nodeType==3||_48.nodeType==4)){
txt=_47.childNodes[0].nodeValue;
}
}
return txt;
};
function _49(_4a){
return {text:_46(_4a),type:_4a.getAttribute("type")};
};
_1.forEach(_41.element.childNodes,function(_4b){
var _4c=_4b.tagName?_4b.tagName.toLowerCase():"";
switch(_4c){
case "title":
_42[_4c]={text:_46(_4b),type:_4b.getAttribute("type")};
break;
case "subtitle":
case "summary":
case "content":
_42[_4c]=_49(_4b);
break;
case "author":
var _4d,_4e;
_1.forEach(_4b.childNodes,function(_4f){
if(!_4f.tagName){
return;
}
switch(_4f.tagName.toLowerCase()){
case "name":
_4d=_4f;
break;
case "uri":
_4e=_4f;
break;
}
});
var _50={};
if(_4d&&_4d.length==1){
_50.name=_46(_4d[0]);
}
if(_4e&&_4e.length==1){
_50.uri=_46(_4e[0]);
}
_42[_4c]=_50;
break;
case "id":
_42[_4c]=_46(_4b);
break;
case "updated":
_42[_4c]=_1.date.stamp.fromISOString(_46(_4b));
break;
case "published":
_42[_4c]=_1.date.stamp.fromISOString(_46(_4b));
break;
case "category":
if(!_42[_4c]){
_42[_4c]=[];
}
_42[_4c].push({scheme:_4b.getAttribute("scheme"),term:_4b.getAttribute("term")});
break;
case "link":
if(!_42[_4c]){
_42[_4c]=[];
}
var _51={rel:_4b.getAttribute("rel"),href:_4b.getAttribute("href"),type:_4b.getAttribute("type")};
_42[_4c].push(_51);
if(_51.rel=="alternate"){
_42["alternate"]=_51;
}
break;
default:
break;
}
});
},_unescapeHTML:function(_52){
_52=_52.replace(/&#8217;/m,"'").replace(/&#8243;/m,"\"").replace(/&#60;/m,">").replace(/&#62;/m,"<").replace(/&#38;/m,"&");
return _52;
},_assertIsItem:function(_53){
if(!this.isItem(_53)){
throw new Error("dojox.data.AtomReadStore: Invalid item argument.");
}
},_assertIsAttribute:function(_54){
if(typeof _54!=="string"){
throw new Error("dojox.data.AtomReadStore: Invalid attribute argument.");
}
}});
_1.extend(_2.data.AtomReadStore,_1.data.util.simpleFetch);
return _2.data.AtomReadStore;
});
