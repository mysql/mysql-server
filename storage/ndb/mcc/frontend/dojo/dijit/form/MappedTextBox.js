//>>built
define("dijit/form/MappedTextBox",["dojo/_base/declare","dojo/dom-construct","./ValidationTextBox"],function(_1,_2,_3){
return _1("dijit.form.MappedTextBox",_3,{postMixInProperties:function(){
this.inherited(arguments);
this.nameAttrSetting="";
},_setNameAttr:null,serialize:function(_4){
return _4.toString?_4.toString():"";
},toString:function(){
var _5=this.filter(this.get("value"));
return _5!=null?(typeof _5=="string"?_5:this.serialize(_5,this.constraints)):"";
},validate:function(){
this.valueNode.value=this.toString();
return this.inherited(arguments);
},buildRendering:function(){
this.inherited(arguments);
this.valueNode=_2.place("<input type='hidden'"+(this.name?" name='"+this.name.replace(/'/g,"&quot;")+"'":"")+"/>",this.textbox,"after");
},reset:function(){
this.valueNode.value="";
this.inherited(arguments);
}});
});
