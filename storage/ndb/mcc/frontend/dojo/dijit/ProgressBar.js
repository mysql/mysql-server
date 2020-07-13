//>>built
require({cache:{"url:dijit/templates/ProgressBar.html":"<div class=\"dijitProgressBar dijitProgressBarEmpty\" role=\"progressbar\"\n\t><div  data-dojo-attach-point=\"internalProgress\" class=\"dijitProgressBarFull\"\n\t\t><div class=\"dijitProgressBarTile\" role=\"presentation\"></div\n\t\t><span style=\"visibility:hidden\">&#160;</span\n\t></div\n\t><div data-dojo-attach-point=\"labelNode\" class=\"dijitProgressBarLabel\" id=\"${id}_label\"></div\n\t><span data-dojo-attach-point=\"indeterminateHighContrastImage\"\n\t\t   class=\"dijitInline dijitProgressBarIndeterminateHighContrastImage\"></span\n></div>\n"}});
define("dijit/ProgressBar",["require","dojo/_base/declare","dojo/dom-class","dojo/_base/lang","dojo/number","./_Widget","./_TemplatedMixin","dojo/text!./templates/ProgressBar.html"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dijit.ProgressBar",[_6,_7],{progress:"0",value:"",maximum:100,places:0,indeterminate:false,label:"",name:"",templateString:_8,_indeterminateHighContrastImagePath:_1.toUrl("./themes/a11y/indeterminate_progress.gif"),postMixInProperties:function(){
this.inherited(arguments);
if(!(this.params&&"value" in this.params)){
this.value=this.indeterminate?Infinity:this.progress;
}
},buildRendering:function(){
this.inherited(arguments);
this.indeterminateHighContrastImage.setAttribute("src",this._indeterminateHighContrastImagePath.toString());
this.update();
},_setDirAttr:function(_9){
var _a=_9.toLowerCase()=="rtl";
_3.toggle(this.domNode,"dijitProgressBarRtl",_a);
_3.toggle(this.domNode,"dijitProgressBarIndeterminateRtl",this.indeterminate&&_a);
this.inherited(arguments);
},update:function(_b){
_4.mixin(this,_b||{});
var _c=this.internalProgress,ap=this.domNode;
var _d=1;
if(this.indeterminate){
ap.removeAttribute("aria-valuenow");
}else{
if(String(this.progress).indexOf("%")!=-1){
_d=Math.min(parseFloat(this.progress)/100,1);
this.progress=_d*this.maximum;
}else{
this.progress=Math.min(this.progress,this.maximum);
_d=this.maximum?this.progress/this.maximum:0;
}
ap.setAttribute("aria-valuenow",this.progress);
}
ap.setAttribute("aria-labelledby",this.labelNode.id);
ap.setAttribute("aria-valuemin",0);
ap.setAttribute("aria-valuemax",this.maximum);
this.labelNode.innerHTML=this.report(_d);
_3.toggle(this.domNode,"dijitProgressBarIndeterminate",this.indeterminate);
_3.toggle(this.domNode,"dijitProgressBarIndeterminateRtl",this.indeterminate&&!this.isLeftToRight());
_c.style.width=(_d*100)+"%";
this.onChange();
},_setValueAttr:function(v){
this._set("value",v);
if(v==Infinity){
this.update({indeterminate:true});
}else{
this.update({indeterminate:false,progress:v});
}
},_setLabelAttr:function(_e){
this._set("label",_e);
this.update();
},_setIndeterminateAttr:function(_f){
this._set("indeterminate",_f);
this.update();
},report:function(_10){
return this.label?this.label:(this.indeterminate?"&#160;":_5.format(_10,{type:"percent",places:this.places,locale:this.lang}));
},onChange:function(){
}});
});
