//>>built
require({cache:{"url:dijit/templates/ProgressBar.html":"<div class=\"dijitProgressBar dijitProgressBarEmpty\" role=\"progressbar\"\n\t><div  data-dojo-attach-point=\"internalProgress\" class=\"dijitProgressBarFull\"\n\t\t><div class=\"dijitProgressBarTile\" role=\"presentation\"></div\n\t\t><span style=\"visibility:hidden\">&#160;</span\n\t></div\n\t><div data-dojo-attach-point=\"labelNode\" class=\"dijitProgressBarLabel\" id=\"${id}_label\"></div\n\t><img data-dojo-attach-point=\"indeterminateHighContrastImage\" class=\"dijitProgressBarIndeterminateHighContrastImage\" alt=\"\"\n/></div>\n"}});
define("dijit/ProgressBar",["require","dojo/_base/declare","dojo/dom-class","dojo/_base/lang","dojo/number","./_Widget","./_TemplatedMixin","dojo/text!./templates/ProgressBar.html"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dijit.ProgressBar",[_6,_7],{progress:"0",value:"",maximum:100,places:0,indeterminate:false,label:"",name:"",templateString:_8,_indeterminateHighContrastImagePath:_1.toUrl("./themes/a11y/indeterminate_progress.gif"),postMixInProperties:function(){
this.inherited(arguments);
if(!("value" in this.params)){
this.value=this.indeterminate?Infinity:this.progress;
}
},buildRendering:function(){
this.inherited(arguments);
this.indeterminateHighContrastImage.setAttribute("src",this._indeterminateHighContrastImagePath.toString());
this.update();
},update:function(_9){
_4.mixin(this,_9||{});
var _a=this.internalProgress,ap=this.domNode;
var _b=1;
if(this.indeterminate){
ap.removeAttribute("aria-valuenow");
ap.removeAttribute("aria-valuemin");
ap.removeAttribute("aria-valuemax");
}else{
if(String(this.progress).indexOf("%")!=-1){
_b=Math.min(parseFloat(this.progress)/100,1);
this.progress=_b*this.maximum;
}else{
this.progress=Math.min(this.progress,this.maximum);
_b=this.maximum?this.progress/this.maximum:0;
}
ap.setAttribute("aria-describedby",this.labelNode.id);
ap.setAttribute("aria-valuenow",this.progress);
ap.setAttribute("aria-valuemin",0);
ap.setAttribute("aria-valuemax",this.maximum);
}
this.labelNode.innerHTML=this.report(_b);
_3.toggle(this.domNode,"dijitProgressBarIndeterminate",this.indeterminate);
_a.style.width=(_b*100)+"%";
this.onChange();
},_setValueAttr:function(v){
this._set("value",v);
if(v==Infinity){
this.update({indeterminate:true});
}else{
this.update({indeterminate:false,progress:v});
}
},_setLabelAttr:function(_c){
this._set("label",_c);
this.update();
},_setIndeterminateAttr:function(_d){
this.indeterminate=_d;
this.update();
},report:function(_e){
return this.label?this.label:(this.indeterminate?"&#160;":_5.format(_e,{type:"percent",places:this.places,locale:this.lang}));
},onChange:function(){
}});
});
