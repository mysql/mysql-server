//>>built
define("dojox/gauges/GlossyHorizontalGauge",["dojo/_base/declare","dojo/_base/connect","dojo/_base/lang","dojo/_base/Color","dojox/gfx","./BarGauge","./BarCircleIndicator","./GlossyHorizontalGaugeMarker"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9;
return _1("dojox.gauges.GlossyHorizontalGauge",[_6],{_defaultIndicator:_7,color:"black",markerColor:"black",majorTicksInterval:10,_majorTicksLength:10,majorTicksColor:"#c4c4c4",minorTicksInterval:5,_minorTicksLength:6,minorTicksColor:"#c4c4c4",value:0,noChange:false,title:"",font:"normal normal normal 10pt serif",scalePrecision:0,_font:null,_margin:2,_minBorderWidth:2,_maxBorderWidth:6,_tickLabelOffset:5,_designHeight:100,constructor:function(){
this.min=0;
this.max=100;
},startup:function(){
this.inherited(arguments);
if(this._gaugeStarted){
return;
}
this._gaugeStarted=true;
var _a=this.height/this._designHeight;
this._minorTicksLength=this._minorTicksLength*_a;
this._majorTicksLength=this._majorTicksLength*_a;
var _b=this._font;
this._computeDataRectangle();
var th=_5.normalizedLength(_b.size);
var _c=th+this._tickLabelOffset+Math.max(this._majorTicksLength,this._minorTicksLength);
var _d=Math.max(0,(this.height-_c)/2);
this.addRange({low:this.min?this.min:0,high:this.max?this.max:100,color:[0,0,0,0]});
this.setMajorTicks({fixedPrecision:true,precision:this.scalePrecision,font:_b,labelPlacement:"inside",offset:_d-this._majorTicksLength/2,interval:this.majorTicksInterval,length:this._majorTicksLength,color:this.majorTicksColor});
this.setMinorTicks({labelPlacement:"inside",offset:_d-this._minorTicksLength/2,interval:this.minorTicksInterval,length:this._minorTicksLength,color:this.minorTicksColor});
this._needle=new _8({hideValue:true,title:this.title,noChange:this.noChange,offset:_d,color:this.markerColor,value:this.value});
this.addIndicator(this._needle);
_2.connect(this._needle,"valueChanged",_3.hitch(this,function(){
this.value=this._needle.value;
this.onValueChanged();
}));
},_layoutGauge:function(){
if(!this._gaugeStarted){
return;
}
var _e=this._font;
this._computeDataRectangle();
var th=_5.normalizedLength(_e.size);
var _f=th+this._tickLabelOffset+Math.max(this._majorTicksLength,this._minorTicksLength);
var _10=Math.max(0,(this.height-_f)/2);
this._setMajorTicksProperty({fixedPrecision:true,precision:this.scalePrecision,font:_e,offset:_10-this._majorTicksLength/2,interval:this.majorTicksInterval,length:this._majorTicksLength});
this._setMinorTicksProperty({offset:_10-this._minorTicksLength/2,interval:this.minorTicksInterval,length:this._minorTicksLength});
this.removeIndicator(this._needle);
this._needle.offset=_10;
this.addIndicator(this._needle);
},_formatNumber:function(val){
var _11=this._getNumberModule();
if(_11){
return _11.format(val,{places:this.scalePrecision});
}else{
return val.toFixed(this.scalePrecision);
}
},_computeDataRectangle:function(){
if(!this._gaugeStarted){
return;
}
var _12=this._font;
var _13=this._getTextWidth(this._formatNumber(this.min),_12)/2;
var _14=this._getTextWidth(this._formatNumber(this.max),_12)/2;
var _15=Math.max(_13,_14);
var _16=this._getBorderWidth()+Math.max(this._majorTicksLength,this._majorTicksLength)/2+_15;
this.dataHeight=this.height;
this.dataY=0;
this.dataX=_16+this._margin;
this.dataWidth=Math.max(0,this.width-2*this.dataX);
},_getTextWidth:function(s,_17){
return _5._base._getTextBox(s,{font:_5.makeFontString(_5.makeParameters(_5.defaultFont,_17))}).w||0;
},_getBorderWidth:function(){
return Math.max(this._minBorderWidth,Math.min(this._maxBorderWidth,this._maxBorderWidth*this.height/this._designHeight));
},drawBackground:function(_18){
if(this._gaugeBackground){
return;
}
var _19=_4.blendColors(new _4(this.color),new _4("white"),0.4);
this._gaugeBackground=_18.createGroup();
var _1a=this._getBorderWidth();
var _1b=this._margin;
var w=this.width;
var h=this.height;
var _1c=Math.min(h/4,23);
this._gaugeBackground.createRect({x:_1b,y:_1b,width:Math.max(0,w-2*_1b),height:Math.max(0,h-2*_1b),r:_1c}).setFill(this.color);
var _1d=_1b+_1a;
var _1e=w-_1a-_1b;
var top=_1b+_1a;
var w2=w-2*_1a-2*_1b;
var h2=h-2*_1a-2*_1b;
if(w2<=0||h2<=0){
return;
}
_1c=Math.min(_1c,w2/2);
_1c=Math.min(_1c,h2/2);
this._gaugeBackground.createRect({x:_1d,y:top,width:w2,height:h2,r:_1c}).setFill({type:"linear",x1:_1d,y1:0,x2:_1d,y2:h-_1a-_1b,colors:[{offset:0,color:_19},{offset:0.2,color:this.color},{offset:0.8,color:this.color},{offset:1,color:_19}]});
var f=4*(Math.sqrt(2)-1)/3*_1c;
this._gaugeBackground.createPath({path:"M"+_1d+" "+(top+_1c)+"C"+_1d+" "+(top+_1c-f)+" "+(_1d+_1c-f)+" "+top+" "+(_1d+_1c)+" "+top+"L"+(_1e-_1c)+" "+top+"C"+(_1e-_1c+f)+" "+top+" "+_1e+" "+(top+_1c-f)+" "+_1e+" "+(top+_1c)+"L"+_1e+" "+(top+h/2)+"L"+_1d+" "+(top+h/3)+"Z"}).setFill({type:"linear",x1:_1d,y1:top,x2:_1d,y2:top+this.height/2,colors:[{offset:0,color:_19},{offset:1,color:_4.blendColors(new _4(this.color),new _4("white"),0.2)}]});
},onValueChanged:function(){
},_setColorAttr:function(_1f){
this.color=_1f?_1f:"black";
if(this._gaugeBackground&&this._gaugeBackground.parent){
this._gaugeBackground.parent.remove(this._gaugeBackground);
}
this._gaugeBackground=null;
this.draw();
},_setMarkerColorAttr:function(_20){
this.markerColor=_20;
if(this._needle){
this.removeIndicator(this._needle);
this._needle.color=_20;
this._needle.shape=null;
this.addIndicator(this._needle);
}
},_setMajorTicksIntervalAttr:function(_21){
this.majorTicksInterval=_21;
this._setMajorTicksProperty({"interval":this.majorTicksInterval});
},setMajorTicksLength:function(_22){
this._majorTicksLength=_22;
this._layoutGauge();
return this;
},getMajorTicksLength:function(){
return this._majorTicksLength;
},_setMajorTicksColorAttr:function(_23){
this.majorTicksColor=_23;
this._setMajorTicksProperty({"color":this.majorTicksColor});
},_setMajorTicksProperty:function(_24){
if(this.majorTicks==null){
return;
}
_3.mixin(this.majorTicks,_24);
this.setMajorTicks(this.majorTicks);
},_setMinorTicksIntervalAttr:function(_25){
this.minorTicksInterval=_25;
this._setMinorTicksProperty({"interval":this.minorTicksInterval});
},setMinorTicksLength:function(_26){
this._minorTicksLength=_26;
this._layoutGauge();
return this;
},getMinorTicksLength:function(){
return this._minorTicksLength;
},_setMinorTicksColorAttr:function(_27){
this.minorTicksColor=_27;
this._setMinorTicksProperty({"color":this.minorTicksColor});
},_setMinorTicksProperty:function(_28){
if(this.minorTicks==null){
return;
}
_3.mixin(this.minorTicks,_28);
this.setMinorTicks(this.minorTicks);
},_setMinAttr:function(min){
this.min=min;
this._computeDataRectangle();
if(this.majorTicks!=null){
this.setMajorTicks(this.majorTicks);
}
if(this.minorTicks!=null){
this.setMinorTicks(this.minorTicks);
}
this.draw();
},_setMaxAttr:function(max){
this.max=max;
this._computeDataRectangle();
if(this.majorTicks!=null){
this.setMajorTicks(this.majorTicks);
}
if(this.minorTicks!=null){
this.setMinorTicks(this.minorTicks);
}
this.draw();
},_setValueAttr:function(_29){
_29=Math.min(this.max,_29);
_29=Math.max(this.min,_29);
this.value=_29;
if(this._needle){
var _2a=this._needle.noChange;
this._needle.noChange=false;
this._needle.update(_29);
this._needle.noChange=_2a;
}
},_setScalePrecisionAttr:function(_2b){
this.scalePrecision=_2b;
this._layoutGauge();
},_setNoChangeAttr:function(_2c){
this.noChange=_2c;
if(this._needle){
this._needle.noChange=this.noChange;
}
},_setTitleAttr:function(_2d){
this.title=_2d;
if(this._needle){
this._needle.title=this.title;
}
},_setFontAttr:function(_2e){
this.font=_2e;
this._font=_5.splitFontString(_2e);
this._layoutGauge();
}});
});
