/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/gears",["./_base/kernel","./_base/lang","./_base/sniff"],function(_1,_2,_3){
_2.getObject("gears",true,_1);
_1.gears._gearsObject=function(){
var _4;
var _5=_2.getObject("google.gears");
if(_5){
return _5;
}
if(typeof GearsFactory!="undefined"){
_4=new GearsFactory();
}else{
if(_3("ie")){
try{
_4=new ActiveXObject("Gears.Factory");
}
catch(e){
}
}else{
if(navigator.mimeTypes["application/x-googlegears"]){
_4=document.createElement("object");
_4.setAttribute("type","application/x-googlegears");
_4.setAttribute("width",0);
_4.setAttribute("height",0);
_4.style.display="none";
document.documentElement.appendChild(_4);
}
}
}
if(!_4){
return null;
}
_2.setObject("google.gears.factory",_4);
return _2.getObject("google.gears");
};
_1.gears.available=(!!_1.gears._gearsObject())||0;
return _1.gears;
});
