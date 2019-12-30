/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/common",["../_base/connect","../_base/kernel","../_base/lang","../dom"],function(_1,_2,_3,_4){
var _5=_3.getObject("dojo.dnd",true);
_5.getCopyKeyState=_1.isCopyKey;
_5._uniqueId=0;
_5.getUniqueId=function(){
var id;
do{
id=_2._scopeName+"Unique"+(++_5._uniqueId);
}while(_4.byId(id));
return id;
};
_5._empty={};
_5.isFormElement=function(e){
var t=e.target;
if(t.nodeType==3){
t=t.parentNode;
}
return " button textarea input select option ".indexOf(" "+t.tagName.toLowerCase()+" ")>=0;
};
return _5;
});
