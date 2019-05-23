//>>built
define("dojox/xml/widgetParser",["dojo/_base/lang","dojo/_base/window","dojo/_base/sniff","dojo/query","dojo/parser","dojox/xml/parser"],function(_1,_2,_3,_4,_5,_6){
var _7=lang.getObject("dojox.xml",true);
xXml.widgetParser=new function(){
var d=_1;
this.parseNode=function(_8){
var _9=[];
d.query("script[type='text/xml']",_8).forEach(function(_a){
_9.push.apply(_9,this._processScript(_a));
},this).orphan();
return d.parser.instantiate(_9);
};
this._processScript=function(_b){
var _c=_b.src?d._getText(_b.src):_b.innerHTML||_b.firstChild.nodeValue;
var _d=this.toHTML(dojox.xml.parser.parse(_c).firstChild);
var _e=d.query("[dojoType]",_d);
_4(">",_d).place(_b,"before");
_b.parentNode.removeChild(_b);
return _e;
};
this.toHTML=function(_f){
var _10;
var _11=_f.nodeName;
var dd=_2.doc;
var _12=_f.nodeType;
if(_12>=3){
return dd.createTextNode((_12==3||_12==4)?_f.nodeValue:"");
}
var _13=_f.localName||_11.split(":").pop();
var _14=_f.namespaceURI||(_f.getNamespaceUri?_f.getNamespaceUri():"");
if(_14=="html"){
_10=dd.createElement(_13);
}else{
var _15=_14+"."+_13;
_10=_10||dd.createElement((_15=="dijit.form.ComboBox")?"select":"div");
_10.setAttribute("dojoType",_15);
}
d.forEach(_f.attributes,function(_16){
var _17=_16.name||_16.nodeName;
var _18=_16.value||_16.nodeValue;
if(_17.indexOf("xmlns")!=0){
if(_3("ie")&&_17=="style"){
_10.style.setAttribute("cssText",_18);
}else{
_10.setAttribute(_17,_18);
}
}
});
d.forEach(_f.childNodes,function(cn){
var _19=this.toHTML(cn);
if(_13=="script"){
_10.text+=_19.nodeValue;
}else{
_10.appendChild(_19);
}
},this);
return _10;
};
}();
return _7.widgetParser;
});
