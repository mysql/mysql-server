//>>built
define("dijit/form/HorizontalRuleLabels",["dojo/_base/declare","dojo/number","dojo/query","./HorizontalRule"],function(_1,_2,_3,_4){
return _1("dijit.form.HorizontalRuleLabels",_4,{templateString:"<div class=\"dijitRuleContainer dijitRuleContainerH dijitRuleLabelsContainer dijitRuleLabelsContainerH\"></div>",labelStyle:"",labels:[],numericMargin:0,minimum:0,maximum:1,constraints:{pattern:"#%"},_positionPrefix:"<div class=\"dijitRuleLabelContainer dijitRuleLabelContainerH\" style=\"left:",_labelPrefix:"\"><div class=\"dijitRuleLabel dijitRuleLabelH\">",_suffix:"</div></div>",_calcPosition:function(_5){
return _5;
},_genHTML:function(_6,_7){
return this._positionPrefix+this._calcPosition(_6)+this._positionSuffix+this.labelStyle+this._labelPrefix+this.labels[_7]+this._suffix;
},getLabels:function(){
var _8=this.labels;
if(!_8.length){
_8=_3("> li",this.srcNodeRef).map(function(_9){
return String(_9.innerHTML);
});
}
this.srcNodeRef.innerHTML="";
if(!_8.length&&this.count>1){
var _a=this.minimum;
var _b=(this.maximum-_a)/(this.count-1);
for(var i=0;i<this.count;i++){
_8.push((i<this.numericMargin||i>=(this.count-this.numericMargin))?"":_2.format(_a,this.constraints));
_a+=_b;
}
}
return _8;
},postMixInProperties:function(){
this.inherited(arguments);
this.labels=this.getLabels();
this.count=this.labels.length;
}});
});
