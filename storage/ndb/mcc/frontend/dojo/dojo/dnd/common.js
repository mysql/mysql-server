/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/common",["../main"],function(_1){
_1.getObject("dnd",true,_1);
_1.dnd.getCopyKeyState=_1.isCopyKey;
_1.dnd._uniqueId=0;
_1.dnd.getUniqueId=function(){
var id;
do{
id=_1._scopeName+"Unique"+(++_1.dnd._uniqueId);
}while(_1.byId(id));
return id;
};
_1.dnd._empty={};
_1.dnd.isFormElement=function(e){
var t=e.target;
if(t.nodeType==3){
t=t.parentNode;
}
return " button textarea input select option ".indexOf(" "+t.tagName.toLowerCase()+" ")>=0;
};
return _1.dnd;
});
