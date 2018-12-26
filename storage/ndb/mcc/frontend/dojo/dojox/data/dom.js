//>>built
define("dojox/data/dom",["dojo/_base/kernel","dojo/_base/lang","dojox/xml/parser"],function(_1,_2,_3){
dojo.deprecated("dojox.data.dom","Use dojox.xml.parser instead.","2.0");
var _4=_2.getObject("dojox.data.dom",true);
_4.createDocument=function(_5,_6){
dojo.deprecated("dojox.data.dom.createDocument()","Use dojox.xml.parser.parse() instead.","2.0");
try{
return _3.parse(_5,_6);
}
catch(e){
return null;
}
};
_4.textContent=function(_7,_8){
dojo.deprecated("dojox.data.dom.textContent()","Use dojox.xml.parser.textContent() instead.","2.0");
if(arguments.length>1){
return _3.textContent(_7,_8);
}else{
return _3.textContent(_7);
}
};
_4.replaceChildren=function(_9,_a){
dojo.deprecated("dojox.data.dom.replaceChildren()","Use dojox.xml.parser.replaceChildren() instead.","2.0");
_3.replaceChildren(_9,_a);
};
_4.removeChildren=function(_b){
dojo.deprecated("dojox.data.dom.removeChildren()","Use dojox.xml.parser.removeChildren() instead.","2.0");
return dojox.xml.parser.removeChildren(_b);
};
_4.innerXML=function(_c){
dojo.deprecated("dojox.data.dom.innerXML()","Use dojox.xml.parser.innerXML() instead.","2.0");
return _3.innerXML(_c);
};
return _4;
});
