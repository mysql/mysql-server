/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Avatar",["../main","./common"],function(_1){
_1.declare("dojo.dnd.Avatar",null,{constructor:function(_2){
this.manager=_2;
this.construct();
},construct:function(){
this.isA11y=_1.hasClass(_1.body(),"dijit_a11y");
var a=_1.create("table",{"class":"dojoDndAvatar",style:{position:"absolute",zIndex:"1999",margin:"0px"}}),_3=this.manager.source,_4,b=_1.create("tbody",null,a),tr=_1.create("tr",null,b),td=_1.create("td",null,tr),_5=this.isA11y?_1.create("span",{id:"a11yIcon",innerHTML:this.manager.copy?"+":"<"},td):null,_6=_1.create("span",{innerHTML:_3.generateText?this._generateText():""},td),k=Math.min(5,this.manager.nodes.length),i=0;
_1.attr(tr,{"class":"dojoDndAvatarHeader",style:{opacity:0.9}});
for(;i<k;++i){
if(_3.creator){
_4=_3._normalizedCreator(_3.getItem(this.manager.nodes[i].id).data,"avatar").node;
}else{
_4=this.manager.nodes[i].cloneNode(true);
if(_4.tagName.toLowerCase()=="tr"){
var _7=_1.create("table"),_8=_1.create("tbody",null,_7);
_8.appendChild(_4);
_4=_7;
}
}
_4.id="";
tr=_1.create("tr",null,b);
td=_1.create("td",null,tr);
td.appendChild(_4);
_1.attr(tr,{"class":"dojoDndAvatarItem",style:{opacity:(9-i)/10}});
}
this.node=a;
},destroy:function(){
_1.destroy(this.node);
this.node=false;
},update:function(){
_1[(this.manager.canDropFlag?"add":"remove")+"Class"](this.node,"dojoDndAvatarCanDrop");
if(this.isA11y){
var _9=_1.byId("a11yIcon");
var _a="+";
if(this.manager.canDropFlag&&!this.manager.copy){
_a="< ";
}else{
if(!this.manager.canDropFlag&&!this.manager.copy){
_a="o";
}else{
if(!this.manager.canDropFlag){
_a="x";
}
}
}
_9.innerHTML=_a;
}
_1.query(("tr.dojoDndAvatarHeader td span"+(this.isA11y?" span":"")),this.node).forEach(function(_b){
_b.innerHTML=this._generateText();
},this);
},_generateText:function(){
return this.manager.nodes.length.toString();
}});
return _1.dnd.Avatar;
});
