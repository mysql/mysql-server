//>>built
define("dojox/gauges/GlossyHorizontalGauge",["dojo/_base/declare","dojo/_base/connect","dojo/_base/lang","dojo/_base/Color","dojox/gfx","./BarGauge","./BarCircleIndicator","./GlossyHorizontalGaugeMarker"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _1("dojox.gauges.GlossyHorizontalGauge",[_6],{_defaultIndicator:_7,color:"black",markerColor:"black",majorTicksInterval:10,_majorTicksLength:10,majorTicksColor:"#c4c4c4",minorTicksInterval:5,_minorTicksLength:6,minorTicksColor:"#c4c4c4",value:0,noChange:false,title:"",font:"normal normal normal 10pt serif",scalePrecision:0,_font:null,_margin:2,_minBorderWidth:2,_maxBorderWidth:6,_tickLabelOffset:5,_designHeight:100,constructor:function(){
this.min=0;
this.max=100;
},startup:function(){
this.inherited(arguments);
if(this._gaugeStarted){
return;
}
this._gaugeStarted=true;
var _9=this.height/this._designHeight;
this._minorTicksLength=this._minorTicksLength*_9;
this._majorTicksLength=this._majorTicksLength*_9;
var _a=this._font;
this._computeDataRectangle();
var th=_5.normalizedLength(_a.size);
var _b=th+this._tickLabelOffset+Math.max(this._majorTicksLength,this._minorTicksLength);
var _c=Math.max(0,(this.height-_b)/2);
this.addRange({low:this.min?this.min:0,high:this.max?this.max:100,color:[0,0,0,0]});
this.setMajorTicks({fixedPrecision:true,precision:this.scalePrecision,font:_a,labelPlacement:"inside",offset:_c-this._majorTicksLength/2,interval:this.majorTicksInterval,length:this._majorTicksLength,color:this.majorTicksColor});
this.setMinorTicks({labelPlacement:"inside",offset:_c-this._minorTicksLength/2,interval:this.minorTicksInterval,length:this._minorTicksLength,color:this.minorTicksColor});
this._needle=new _8({hideValue:true,title:this.title,noChange:this.noChange,offset:_c,color:this.markerColor,value:this.value});
this.addIndicator(this._needle);
_2.connect(this._needle,"valueChanged",_3.hitch(this,function(){
this.value=this._needle.value;
this.onValueChanged();
}));
},_layoutGauge:function(){
if(!this._gaugeStarted){
return;
}
var _d=this._font;
this._computeDataRectangle();
var th=_5.normalizedLength(_d.size);
var _e=th+this._tickLabelOffset+Math.max(this._majorTicksLength,this._minorTicksLength);
var _f=Math.max(0,(this.height-_e)/2);
this._setMajorTicksProperty({fixedPrecision:true,precision:this.scalePrecision,font:_d,offset:_f-this._majorTicksLength/2,interval:this.majorTicksInterval,length:this._majorTicksLength});
this._setMinorTicksProperty({offset:_f-this._minorTicksLength/2,interval:this.minorTicksInterval,length:this._minorTicksLength});
this.removeIndicator(this._needle);
this._needle.offset=_f;
this.addIndicator(this._needle);
},_formatNumber:function(val){
var _10=this._getNumberModule();
if(_10){
return _10.format(val,{places:this.scalePrecision});
}else{
return val.toFixed(this.scalePrecision);
}
},_computeDataRectangle:function(){
if(!this._gaugeStarted){
return;
}
var _11=this._font;
var _12=this._getTextWidth(this._formatNumber(this.min),_11)/2;
var _13=this._getTextWidth(this._formatNumber(this.max),_11)/2;
var _14=Math.max(_12,_13);
var _15=this._getBorderWidth()+Math.max(this._majorTicksLength,this._majorTicksLength)/2+_14;
this.dataHeight=this.height;
this.dataY=0;
this.dataX=_15+this._margin;
this.dataWidth=Math.max(0,this.width-2*this.dataX);
},_getTextWidth:function(s,_16){
return _5._base._getTextBox(s,{font:_5.makeFontString(_5.makeParameters(_5.defaultFont,_16))}).w||0;
},_getBorderWidth:function(){
return Math.max(this._minBorderWidth,Math.min(this._maxBorderWidth,this._maxBorderWidth*this.height/this._designHeight));
},drawBackground:function(_17){
if(this._gaugeBackground){
return;
}
var _18=_4.blendColors(new _4(this.color),new _4("white"),0.4);
this._gaugeBackground=_17.createGroup();
var _19=this._getBorderWidth();
var _1a=this._margin;
var w=this.width;
var h=this.height;
var _1b=Math.min(h/4,23);
this._gaugeBackground.createRect({x:_1a,y:_1a,width:Math.max(0,w-2*_1a),height:Math.max(0,h-2*_1a),r:_1b}).setFill(this.color);
var _1c=_1a+_19;
var _1d=w-_19-_1a;
var top=_1a+_19;
var w2=w-2*_19-2*_1a;
var h2=h-2*_19-2*_1a;
if(w2<=0||h2<=0){
return;
}
_1b=Math.min(_1b,w2/2);
_1b=Math.min(_1b,h2/2);
this._gaugeBackground.createRect({x:_1c,y:top,width:w2,height:h2,r:_1b}).setFill({type:"linear",x1:_1c,y1:0,x2:_1c,y2:h-_19-_1a,colors:[{offset:0,color:_18},{offset:0.2,color:this.color},{offset:0.8,color:this.color},{offset:1,color:_18}]});
var f=4*(Math.sqrt(2)-1)/3*_1b;
this._gaugeBackground.createPath({path:"M"+_1c+" "+(top+_1b)+"C"+_1c+" "+(top+_1b-f)+" "+(_1c+_1b-f)+" "+top+" "+(_1c+_1b)+" "+top+"L"+(_1d-_1b)+" "+top+"C"+(_1d-_1b+f)+" "+top+" "+_1d+" "+(top+_1b-f)+" "+_1d+" "+(top+_1b)+"L"+_1d+" "+(top+h/2)+"L"+_1c+" "+(top+h/3)+"Z"}).setFill({type:"linear",x1:_1c,y1:top,x2:_1c,y2:top+this.height/2,colors:[{offset:0,color:_18},{offset:1,color:_4.blendColors(new _4(this.color),new _4("white"),0.2)}]});
},onValueChanged:function(){
},_setColorAttr:function(_1e){
this.color=_1e?_1e:"black";
if(this._gaugeBackground&&this._gaugeBackground.parent){
this._gaugeBackground.parent.remove(this._gaugeBackground);
}
this._gaugeBackground=null;
this.draw();
},_setMarkerColorAttr:function(_1f){
this.markerColor=_1f;
if(this._needle){
this.removeIndicator(this._needle);
this._needle.color=_1f;
this._needle.shape=null;
this.addIndicator(this._needle);
}
},_setMajorTicksIntervalAttr:function(_20){
this.majorTicksInterval=_20;
this._setMajorTicksProperty({"interval":this.majorTicksInterval});
},setMajorTicksLength:function(_21){
this._majorTicksLength=_21;
this._layoutGauge();
return this;
},getMajorTicksLength:function(){
return this._majorTicksLength;
},_setMajorTicksColorAttr:function(_22){
this.majorTicksColor=_22;
this._setMajorTicksProperty({"color":this.majorTicksColor});
},_setMajorTicksProperty:function(_23){
if(this.majorTicks==null){
return;
}
_3.mixin(this.majorTicks,_23);
this.setMajorTicks(this.majorTicks);
},_setMinorTicksIntervalAttr:function(_24){
this.minorTicksInterval=_24;
this._setMinorTicksProperty({"interval":this.minorTicksInterval});
},setMinorTicksLength:function(_25){
this._minorTicksLength=_25;
this._layoutGauge();
return this;
},getMinorTicksLength:function(){
return this._minorTicksLength;
},_setMinorTicksColorAttr:function(_26){
this.minorTicksColor=_26;
this._setMinorTicksProperty({"color":this.minorTicksColor});
},_setMinorTicksProperty:function(_27){
if(this.minorTicks==null){
return;
}
_3.mixin(this.minorTicks,_27);
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
},_setValueAttr:function(_28){
_28=Math.min(this.max,_28);
_28=Math.max(this.min,_28);
this.value=_28;
if(this._needle){
var _29=this._needle.noChange;
this._needle.noChange=false;
this._needle.update(_28);
this._needle.noChange=_29;
}
},_setScalePrecisionAttr:function(_2a){
this.scalePrecision=_2a;
this._layoutGauge();
},_setNoChangeAttr:function(_2b){
this.noChange=_2b;
if(this._needle){
this._needle.noChange=this.noChange;
}
},_setTitleAttr:function(_2c){
this.title=_2c;
if(this._needle){
this._needle.title=this.title;
}
},_setFontAttr:function(_2d){
this.font=_2d;
this._font=_5.splitFontString(_2d);
this._layoutGauge();
}});
});
