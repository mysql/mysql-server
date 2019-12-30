//>>built
require({cache:{"url:dijit/form/templates/Spinner.html":"<div class=\"dijit dijitReset dijitInline dijitLeft\"\n\tid=\"widget_${id}\" role=\"presentation\"\n\t><div class=\"dijitReset dijitButtonNode dijitSpinnerButtonContainer\"\n\t\t><input class=\"dijitReset dijitInputField dijitSpinnerButtonInner\" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t\t/><div class=\"dijitReset dijitLeft dijitButtonNode dijitArrowButton dijitUpArrowButton\"\n\t\t\tdata-dojo-attach-point=\"upArrowNode\"\n\t\t\t><div class=\"dijitArrowButtonInner\"\n\t\t\t\t><input class=\"dijitReset dijitInputField\" value=\"&#9650; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t\t\t\t\t${_buttonInputDisabled}\n\t\t\t/></div\n\t\t></div\n\t\t><div class=\"dijitReset dijitLeft dijitButtonNode dijitArrowButton dijitDownArrowButton\"\n\t\t\tdata-dojo-attach-point=\"downArrowNode\"\n\t\t\t><div class=\"dijitArrowButtonInner\"\n\t\t\t\t><input class=\"dijitReset dijitInputField\" value=\"&#9660; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t\t\t\t\t${_buttonInputDisabled}\n\t\t\t/></div\n\t\t></div\n\t></div\n\t><div class='dijitReset dijitValidationContainer'\n\t\t><input class=\"dijitReset dijitInputField dijitValidationIcon dijitValidationInner\" value=\"&#935; \" type=\"text\" tabIndex=\"-1\" readonly=\"readonly\" role=\"presentation\"\n\t/></div\n\t><div class=\"dijitReset dijitInputField dijitInputContainer\"\n\t\t><input class='dijitReset dijitInputInner' data-dojo-attach-point=\"textbox,focusNode\" type=\"${type}\" data-dojo-attach-event=\"onkeypress:_onKeyPress\"\n\t\t\trole=\"spinbutton\" autocomplete=\"off\" ${!nameAttrSetting}\n\t/></div\n></div>\n"}});
define("dijit/form/_Spinner",["dojo/_base/declare","dojo/_base/event","dojo/keys","dojo/_base/lang","dojo/sniff","dojo/mouse","../typematic","./RangeBoundTextBox","dojo/text!./templates/Spinner.html","./_TextBoxMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _1("dijit.form._Spinner",_8,{defaultTimeout:500,minimumTimeout:10,timeoutChangeRate:0.9,smallDelta:1,largeDelta:10,templateString:_9,baseClass:"dijitTextBox dijitSpinner",cssStateNodes:{"upArrowNode":"dijitUpArrowButton","downArrowNode":"dijitDownArrowButton"},adjust:function(_b){
return _b;
},_arrowPressed:function(_c,_d,_e){
if(this.disabled||this.readOnly){
return;
}
this._setValueAttr(this.adjust(this.get("value"),_d*_e),false);
_a.selectInputText(this.textbox,this.textbox.value.length);
},_arrowReleased:function(){
this._wheelTimer=null;
},_typematicCallback:function(_f,_10,evt){
var inc=this.smallDelta;
if(_10==this.textbox){
var key=evt.charOrCode;
inc=(key==_3.PAGE_UP||key==_3.PAGE_DOWN)?this.largeDelta:this.smallDelta;
_10=(key==_3.UP_ARROW||key==_3.PAGE_UP)?this.upArrowNode:this.downArrowNode;
}
if(_f==-1){
this._arrowReleased(_10);
}else{
this._arrowPressed(_10,(_10==this.upArrowNode)?1:-1,inc);
}
},_wheelTimer:null,_mouseWheeled:function(evt){
_2.stop(evt);
var _11=evt.wheelDelta/120;
if(Math.floor(_11)!=_11){
_11=evt.wheelDelta>0?1:-1;
}
var _12=evt.detail?(evt.detail*-1):_11;
if(_12!==0){
var _13=this[(_12>0?"upArrowNode":"downArrowNode")];
this._arrowPressed(_13,_12,this.smallDelta);
if(this._wheelTimer){
this._wheelTimer.remove();
}
this._wheelTimer=this.defer(function(){
this._arrowReleased(_13);
},50);
}
},_setConstraintsAttr:function(_14){
this.inherited(arguments);
if(this.focusNode){
if(this.constraints.min!==undefined){
this.focusNode.setAttribute("aria-valuemin",this.constraints.min);
}else{
this.focusNode.removeAttribute("aria-valuemin");
}
if(this.constraints.max!==undefined){
this.focusNode.setAttribute("aria-valuemax",this.constraints.max);
}else{
this.focusNode.removeAttribute("aria-valuemax");
}
}
},_setValueAttr:function(_15,_16){
this.focusNode.setAttribute("aria-valuenow",_15);
this.inherited(arguments);
},postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,_6.wheel,"_mouseWheeled");
this.own(_7.addListener(this.upArrowNode,this.textbox,{charOrCode:_3.UP_ARROW,ctrlKey:false,altKey:false,shiftKey:false,metaKey:false},this,"_typematicCallback",this.timeoutChangeRate,this.defaultTimeout,this.minimumTimeout),_7.addListener(this.downArrowNode,this.textbox,{charOrCode:_3.DOWN_ARROW,ctrlKey:false,altKey:false,shiftKey:false,metaKey:false},this,"_typematicCallback",this.timeoutChangeRate,this.defaultTimeout,this.minimumTimeout),_7.addListener(this.upArrowNode,this.textbox,{charOrCode:_3.PAGE_UP,ctrlKey:false,altKey:false,shiftKey:false,metaKey:false},this,"_typematicCallback",this.timeoutChangeRate,this.defaultTimeout,this.minimumTimeout),_7.addListener(this.downArrowNode,this.textbox,{charOrCode:_3.PAGE_DOWN,ctrlKey:false,altKey:false,shiftKey:false,metaKey:false},this,"_typematicCallback",this.timeoutChangeRate,this.defaultTimeout,this.minimumTimeout));
}});
});
