//>>built
define("dijit/hccss",["require","dojo/_base/config","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/ready","dojo/_base/sniff","dojo/_base/window"],function(_1,_2,_3,_4,_5,_6,_7,_8){
if(_7("ie")||_7("mozilla")){
_6(90,function(){
var _9=_4.create("div",{id:"a11yTestNode",style:{cssText:"border: 1px solid;"+"border-color:red green;"+"position: absolute;"+"height: 5px;"+"top: -999px;"+"background-image: url(\""+(_2.blankGif||_1.toUrl("dojo/resources/blank.gif"))+"\");"}},_8.body());
var cs=_5.getComputedStyle(_9);
if(cs){
var _a=cs.backgroundImage;
var _b=(cs.borderTopColor==cs.borderRightColor)||(_a!=null&&(_a=="none"||_a=="url(invalid-url:)"));
if(_b){
_3.add(_8.body(),"dijit_a11y");
}
if(_7("ie")){
_9.outerHTML="";
}else{
_8.body().removeChild(_9);
}
}
});
}
});
