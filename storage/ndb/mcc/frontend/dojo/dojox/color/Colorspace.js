//>>built
define("dojox/color/Colorspace",["dojo/_base/kernel","../main","dojo/_base/lang","./_base","dojox/math/matrix"],function(_1,_2,_3,_4,_5){
_2.color.Colorspace=new (function(){
var _6=this;
var _7={"2":{"E":{x:1/3,y:1/3,t:5400},"D50":{x:0.34567,y:0.3585,t:5000},"D55":{x:0.33242,y:0.34743,t:5500},"D65":{x:0.31271,y:0.32902,t:6500},"D75":{x:0.29902,y:0.31485,t:7500},"A":{x:0.44757,y:0.40745,t:2856},"B":{x:0.34842,y:0.35161,t:4874},"C":{x:0.31006,y:0.31616,t:6774},"9300":{x:0.2848,y:0.2932,t:9300},"F2":{x:0.37207,y:0.37512,t:4200},"F7":{x:0.31285,y:0.32918,t:6500},"F11":{x:0.38054,y:0.37691,t:4000}},"10":{"E":{x:1/3,y:1/3,t:5400},"D50":{x:0.34773,y:0.35952,t:5000},"D55":{x:0.33411,y:0.34877,t:5500},"D65":{x:0.31382,y:0.331,t:6500},"D75":{x:0.29968,y:0.3174,t:7500},"A":{x:0.45117,y:0.40594,t:2856},"B":{x:0.3498,y:0.3527,t:4874},"C":{x:0.31039,y:0.31905,t:6774},"F2":{x:0.37928,y:0.36723,t:4200},"F7":{x:0.31565,y:0.32951,t:6500},"F11":{x:0.38543,y:0.3711,t:4000}}};
var _8={"Adobe RGB 98":[2.2,"D65",0.64,0.33,0.297361,0.21,0.71,0.627355,0.15,0.06,0.075285],"Apple RGB":[1.8,"D65",0.625,0.34,0.244634,0.28,0.595,0.672034,0.155,0.07,0.083332],"Best RGB":[2.2,"D50",0.7347,0.2653,0.228457,0.215,0.775,0.737352,0.13,0.035,0.034191],"Beta RGB":[2.2,"D50",0.6888,0.3112,0.303273,0.1986,0.7551,0.663786,0.1265,0.0352,0.032941],"Bruce RGB":[2.2,"D65",0.64,0.33,0.240995,0.28,0.65,0.683554,0.15,0.06,0.075452],"CIE RGB":[2.2,"E",0.735,0.265,0.176204,0.274,0.717,0.812985,0.167,0.009,0.010811],"ColorMatch RGB":[1.8,"D50",0.63,0.34,0.274884,0.295,0.605,0.658132,0.15,0.075,0.066985],"DON RGB 4":[2.2,"D50",0.696,0.3,0.27835,0.215,0.765,0.68797,0.13,0.035,0.03368],"ECI RGB":[1.8,"D50",0.67,0.33,0.32025,0.21,0.71,0.602071,0.14,0.08,0.077679],"EktaSpace PS5":[2.2,"D50",0.695,0.305,0.260629,0.26,0.7,0.734946,0.11,0.005,0.004425],"NTSC RGB":[2.2,"C",0.67,0.33,0.298839,0.21,0.71,0.586811,0.14,0.08,0.11435],"PAL/SECAM RGB":[2.2,"D65",0.64,0.33,0.222021,0.29,0.6,0.706645,0.15,0.06,0.071334],"Pro Photo RGB":[1.8,"D50",0.7347,0.2653,0.28804,0.1596,0.8404,0.711874,0.0366,0.0001,0.000086],"SMPTE/C RGB":[2.2,"D65",0.63,0.34,0.212395,0.31,0.595,0.701049,0.155,0.07,0.086556],"sRGB":[2.2,"D65",0.64,0.33,0.212656,0.3,0.6,0.715158,0.15,0.06,0.072186],"Wide Gamut RGB":[2.2,"D50",0.735,0.265,0.258187,0.115,0.826,0.724938,0.157,0.018,0.016875]};
var _9={"XYZ scaling":{ma:[[1,0,0],[0,1,0],[0,0,1]],mai:[[1,0,0],[0,1,0],[0,0,1]]},"Bradford":{ma:[[0.8951,-0.7502,0.0389],[0.2664,1.7135,-0.0685],[-0.1614,0.0367,1.0296]],mai:[[0.986993,0.432305,-0.008529],[-0.147054,0.51836,0.040043],[0.159963,0.049291,0.968487]]},"Von Kries":{ma:[[0.40024,-0.2263,0],[0.7076,1.16532,0],[-0.08081,0.0457,0.91822]],mai:[[1.859936,0.361191,0],[-1.129382,0.638812,0],[0.219897,-0.000006,1.089064]]}};
var _a={"XYZ":{"xyY":function(_b,_c){
_c=_1.mixin({whitepoint:"D65",observer:"10",useApproximation:true},_c||{});
var wp=_6.whitepoint(_c.whitepoint,_c.observer);
var _d=_b.X+_b.Y+_b.Z;
if(_d==0){
var x=wp.x,y=wp.y;
}else{
var x=_b.X/_d,y=_b.Y/_d;
}
return {x:x,y:y,Y:_b.Y};
},"Lab":function(_e,_f){
_f=_1.mixin({whitepoint:"D65",observer:"10",useApproximation:true},_f||{});
var _10=_6.kappa(_f.useApproximation),_11=_6.epsilon(_f.useApproximation);
var wp=_6.whitepoint(_f.whitepoint,_f.observer);
var xr=_e.X/wp.x,yr=_e.Y/wp.y,zr=_e.z/wp.z;
var fx=(xr>_11)?Math.pow(xr,1/3):(_10*xr+16)/116;
var fy=(yr>_11)?Math.pow(yr,1/3):(_10*yr+16)/116;
var fz=(zr>_11)?Math.pow(zr,1/3):(_10*zr+16)/116;
var L=116*fy-16,a=500*(fx-fy),b=200*(fy-fz);
return {L:L,a:a,b:b};
},"Luv":function(xyz,_12){
_12=_1.mixin({whitepoint:"D65",observer:"10",useApproximation:true},_12||{});
var _13=_6.kappa(_12.useApproximation),_14=_6.epsilon(_12.useApproximation);
var wp=_6.whitepoint(_12.whitepoint,_12.observer);
var ud=(4*xyz.X)/(xyz.X+15*xyz.Y+3*xyz.Z);
var vd=(9*xyz.Y)/(xyz.X+15*xyz.Y+3*xyz.Z);
var udr=(4*wp.x)/(wp.x+15*wp.y+3*wp.z);
var vdr=(9*wp.y)/(wp.x+15*wp.y+3*wp.z);
var yr=xyz.Y/wp.y;
var L=(yr>_14)?116*Math.pow(yr,1/3)-16:_13*yr;
var u=13*L*(ud-udr);
var v=13*L*(vd-vdr);
return {L:L,u:u,v:v};
}},"xyY":{"XYZ":function(xyY){
if(xyY.y==0){
var X=0,Y=0,Z=0;
}else{
var X=(xyY.x*xyY.Y)/xyY.y;
var Y=xyY.Y;
var Z=((1-xyY.x-xyY.y)*xyY.Y)/xyY.y;
}
return {X:X,Y:Y,Z:Z};
}},"Lab":{"XYZ":function(lab,_15){
_15=_1.mixin({whitepoint:"D65",observer:"10",useApproximation:true},_15||{});
var b=_15.useApproximation,_16=_6.kappa(b),_17=_6.epsilon(b);
var wp=_6.whitepoint(_15.whitepoint,_15.observer);
var yr=(lab.L>(_16*_17))?Math.pow((lab.L+16)/116,3):lab.L/_16;
var fy=(yr>_17)?(lab.L+16)/116:(_16*yr+16)/116;
var fx=(lab.a/500)+fy;
var fz=fy-(lab.b/200);
var _18=Math.pow(fx,3),_19=Math.pow(fz,3);
var xr=(_18>_17)?_18:(116*fx-16)/_16;
var zr=(_19>_17)?_19:(116*fz-16)/_16;
return {X:xr*wp.x,Y:yr*wp.y,Z:zr*wp.z};
},"LCHab":function(lab){
var L=lab.L,C=Math.pow(lab.a*lab.a+lab.b*lab.b,0.5),H=Math.atan(lab.b,lab.a)*(180/Math.PI);
if(H<0){
H+=360;
}
if(H<360){
H-=360;
}
return {L:L,C:C,H:H};
}},"LCHab":{"Lab":function(lch){
var _1a=lch.H*(Math.PI/180),L=lch.L,a=lch.C/Math.pow(Math.pow(Math.tan(_1a),2)+1,0.5);
if(90<lchH&&lch.H<270){
a=-a;
}
var b=Math.pow(Math.pow(lch.C,2)-Math.pow(a,2),0.5);
if(lch.H>180){
b=-b;
}
return {L:L,a:a,b:b};
}},"Luv":{"XYZ":function(Luv,_1b){
_1b=_1.mixin({whitepoint:"D65",observer:"10",useApproximation:true},_1b||{});
var b=_1b.useApproximation,_1c=_6.kappa(b),_1d=_6.epsilon(b);
var wp=_6.whitepoint(_1b.whitepoint,_1b.observer);
var uz=(4*wp.x)/(wp.x+15*wp.y+3*wp.z);
var vz=(9*wp.y)/(wp.x+15*wp.y+3*wp.z);
var Y=(Luv.L>_1c*_1d)?Math.pow((Luv.L+16)/116,3):Luv.L/_1c;
var a=(1/3)*(((52*Luv.L)/(Luv.u+13*Luv.L*uz))-1);
var b=-5*Y,c=-(1/3),d=Y*(((39*Luv.L)/(Luv.v+13*Luv.L*vz))-5);
var X=(d-b)/(a-c),Z=X*a+b;
return {X:X,Y:Y,Z:Z};
},"LCHuv":function(Luv){
var L=Luv.L,C=Math.pow(Luv.u*Luv.u+Luv.v*Luv*v,0.5),H=Math.atan(Luv.v,Luv.u)*(180/Math.PI);
if(H<0){
H+=360;
}
if(H>360){
H-=360;
}
return {L:L,C:C,H:H};
}},"LCHuv":{"Luv":function(LCH){
var _1e=LCH.H*(Math.PI/180);
var L=LCH.L,u=LCH.C/Math.pow(Math.pow(Math.tan(_1e),2)+1,0.5);
var v=Math.pow(LCH.C*LCH.C-u*u,0.5);
if(90<LCH.H&&LCH.H>270){
u*=-1;
}
if(LCH.H>180){
v*=-1;
}
return {L:L,u:u,v:v};
}}};
var _1f={"CMY":{"CMYK":function(obj,_20){
return _4.fromCmy(obj).toCmyk();
},"HSL":function(obj,_21){
return _4.fromCmy(obj).toHsl();
},"HSV":function(obj,_22){
return _4.fromCmy(obj).toHsv();
},"Lab":function(obj,_23){
return _a["XYZ"]["Lab"](_4.fromCmy(obj).toXYZ(_23));
},"LCHab":function(obj,_24){
return _a["Lab"]["LCHab"](_1f["CMY"]["Lab"](obj));
},"LCHuv":function(obj,_25){
return _a["LCHuv"]["Luv"](_a["Luv"]["XYZ"](_4.fromCmy(obj).toXYZ(_25)));
},"Luv":function(obj,_26){
return _a["Luv"]["XYZ"](_4.fromCmy(obj).toXYZ(_26));
},"RGB":function(obj,_27){
return _4.fromCmy(obj);
},"XYZ":function(obj,_28){
return _4.fromCmy(obj).toXYZ(_28);
},"xyY":function(obj,_29){
return _a["XYZ"]["xyY"](_4.fromCmy(obj).toXYZ(_29));
}},"CMYK":{"CMY":function(obj,_2a){
return _4.fromCmyk(obj).toCmy();
},"HSL":function(obj,_2b){
return _4.fromCmyk(obj).toHsl();
},"HSV":function(obj,_2c){
return _4.fromCmyk(obj).toHsv();
},"Lab":function(obj,_2d){
return _a["XYZ"]["Lab"](_4.fromCmyk(obj).toXYZ(_2d));
},"LCHab":function(obj,_2e){
return _a["Lab"]["LCHab"](_1f["CMYK"]["Lab"](obj));
},"LCHuv":function(obj,_2f){
return _a["LCHuv"]["Luv"](_a["Luv"]["XYZ"](_4.fromCmyk(obj).toXYZ(_2f)));
},"Luv":function(obj,_30){
return _a["Luv"]["XYZ"](_4.fromCmyk(obj).toXYZ(_30));
},"RGB":function(obj,_31){
return _4.fromCmyk(obj);
},"XYZ":function(obj,_32){
return _4.fromCmyk(obj).toXYZ(_32);
},"xyY":function(obj,_33){
return _a["XYZ"]["xyY"](_4.fromCmyk(obj).toXYZ(_33));
}},"HSL":{"CMY":function(obj,_34){
return _4.fromHsl(obj).toCmy();
},"CMYK":function(obj,_35){
return _4.fromHsl(obj).toCmyk();
},"HSV":function(obj,_36){
return _4.fromHsl(obj).toHsv();
},"Lab":function(obj,_37){
return _a["XYZ"]["Lab"](_4.fromHsl(obj).toXYZ(_37));
},"LCHab":function(obj,_38){
return _a["Lab"]["LCHab"](_1f["CMYK"]["Lab"](obj));
},"LCHuv":function(obj,_39){
return _a["LCHuv"]["Luv"](_a["Luv"]["XYZ"](_4.fromHsl(obj).toXYZ(_39)));
},"Luv":function(obj,_3a){
return _a["Luv"]["XYZ"](_4.fromHsl(obj).toXYZ(_3a));
},"RGB":function(obj,_3b){
return _4.fromHsl(obj);
},"XYZ":function(obj,_3c){
return _4.fromHsl(obj).toXYZ(_3c);
},"xyY":function(obj,_3d){
return _a["XYZ"]["xyY"](_4.fromHsl(obj).toXYZ(_3d));
}},"HSV":{"CMY":function(obj,_3e){
return _4.fromHsv(obj).toCmy();
},"CMYK":function(obj,_3f){
return _4.fromHsv(obj).toCmyk();
},"HSL":function(obj,_40){
return _4.fromHsv(obj).toHsl();
},"Lab":function(obj,_41){
return _a["XYZ"]["Lab"](_4.fromHsv(obj).toXYZ(_41));
},"LCHab":function(obj,_42){
return _a["Lab"]["LCHab"](_1f["CMYK"]["Lab"](obj));
},"LCHuv":function(obj,_43){
return _a["LCHuv"]["Luv"](_a["Luv"]["XYZ"](_4.fromHsv(obj).toXYZ(_43)));
},"Luv":function(obj,_44){
return _a["Luv"]["XYZ"](_4.fromHsv(obj).toXYZ(_44));
},"RGB":function(obj,_45){
return _4.fromHsv(obj);
},"XYZ":function(obj,_46){
return _4.fromHsv(obj).toXYZ(_46);
},"xyY":function(obj,_47){
return _a["XYZ"]["xyY"](_4.fromHsv(obj).toXYZ(_47));
}},"Lab":{"CMY":function(obj,_48){
return _4.fromXYZ(_a["Lab"]["XYZ"](obj,_48)).toCmy();
},"CMYK":function(obj,_49){
return _4.fromXYZ(_a["Lab"]["XYZ"](obj,_49)).toCmyk();
},"HSL":function(obj,_4a){
return _4.fromXYZ(_a["Lab"]["XYZ"](obj,_4a)).toHsl();
},"HSV":function(obj,_4b){
return _4.fromXYZ(_a["Lab"]["XYZ"](obj,_4b)).toHsv();
},"LCHab":function(obj,_4c){
return _a["Lab"]["LCHab"](obj,_4c);
},"LCHuv":function(obj,_4d){
return _a["Luv"]["LCHuv"](_a["Lab"]["XYZ"](obj,_4d),_4d);
},"Luv":function(obj,_4e){
return _a["XYZ"]["Luv"](_a["Lab"]["XYZ"](obj,_4e),_4e);
},"RGB":function(obj,_4f){
return _4.fromXYZ(_a["Lab"]["XYZ"](obj,_4f));
},"XYZ":function(obj,_50){
return _a["Lab"]["XYZ"](obj,_50);
},"xyY":function(obj,_51){
return _a["XYZ"]["xyY"](_a["Lab"]["XYZ"](obj,_51),_51);
}},"LCHab":{"CMY":function(obj,_52){
return _4.fromXYZ(_a["Lab"]["XYZ"](_a["LCHab"]["Lab"](obj),_52),_52).toCmy();
},"CMYK":function(obj,_53){
return _4.fromXYZ(_a["Lab"]["XYZ"](_a["LCHab"]["Lab"](obj),_53),_53).toCmyk();
},"HSL":function(obj,_54){
return _4.fromXYZ(_a["Lab"]["XYZ"](_a["LCHab"]["Lab"](obj),_54),_54).toHsl();
},"HSV":function(obj,_55){
return _4.fromXYZ(_a["Lab"]["XYZ"](_a["LCHab"]["Lab"](obj),_55),_55).toHsv();
},"Lab":function(obj,_56){
return _a["Lab"]["LCHab"](obj,_56);
},"LCHuv":function(obj,_57){
return _a["Luv"]["LCHuv"](_a["XYZ"]["Luv"](_a["Lab"]["XYZ"](_a["LCHab"]["Lab"](obj),_57),_57),_57);
},"Luv":function(obj,_58){
return _a["XYZ"]["Luv"](_a["Lab"]["XYZ"](_a["LCHab"]["Lab"](obj),_58),_58);
},"RGB":function(obj,_59){
return _4.fromXYZ(_a["Lab"]["XYZ"](_a["LCHab"]["Lab"](obj),_59),_59);
},"XYZ":function(obj,_5a){
return _a["Lab"]["XYZ"](_a["LCHab"]["Lab"](obj,_5a),_5a);
},"xyY":function(obj,_5b){
return _a["XYZ"]["xyY"](_a["Lab"]["XYZ"](_a["LCHab"]["Lab"](obj),_5b),_5b);
}},"LCHuv":{"CMY":function(obj,_5c){
return _4.fromXYZ(_a["Luv"]["XYZ"](_a["LCHuv"]["Luv"](obj),_5c),_5c).toCmy();
},"CMYK":function(obj,_5d){
return _4.fromXYZ(_a["Luv"]["XYZ"](_a["LCHuv"]["Luv"](obj),_5d),_5d).toCmyk();
},"HSL":function(obj,_5e){
return _4.fromXYZ(_a["Luv"]["XYZ"](_a["LCHuv"]["Luv"](obj),_5e),_5e).toHsl();
},"HSV":function(obj,_5f){
return _4.fromXYZ(_a["Luv"]["XYZ"](_a["LCHuv"]["Luv"](obj),_5f),_5f).toHsv();
},"Lab":function(obj,_60){
return _a["XYZ"]["Lab"](_a["Luv"]["XYZ"](_a["LCHuv"]["Luv"](obj),_60),_60);
},"LCHab":function(obj,_61){
return _a["Lab"]["LCHab"](_a["XYZ"]["Lab"](_a["Luv"]["XYZ"](_a["LCHuv"]["Luv"](obj),_61),_61),_61);
},"Luv":function(obj,_62){
return _a["LCHuv"]["Luv"](obj,_62);
},"RGB":function(obj,_63){
return _4.fromXYZ(_a["Luv"]["XYZ"](_a["LCHuv"]["Luv"](obj),_63),_63);
},"XYZ":function(obj,_64){
return _a["Luv"]["XYZ"](_a["LCHuv"]["Luv"](obj),_64);
},"xyY":function(obj,_65){
return _a["XYZ"]["xyY"](_a["Luv"]["XYZ"](_a["LCHuv"]["Luv"](obj),_65),_65);
}},"Luv":{"CMY":function(obj,_66){
return _4.fromXYZ(_a["Luv"]["XYZ"](obj,_66),_66).toCmy();
},"CMYK":function(obj,_67){
return _4.fromXYZ(_a["Luv"]["XYZ"](obj,_67),_67).toCmyk();
},"HSL":function(obj,_68){
return _4.fromXYZ(_a["Luv"]["XYZ"](obj,_68),_68).toHsl();
},"HSV":function(obj,_69){
return _4.fromXYZ(_a["Luv"]["XYZ"](obj,_69),_69).toHsv();
},"Lab":function(obj,_6a){
return _a["XYZ"]["Lab"](_a["Luv"]["XYZ"](obj,_6a),_6a);
},"LCHab":function(obj,_6b){
return _a["Lab"]["LCHab"](_a["XYZ"]["Lab"](_a["Luv"]["XYZ"](obj,_6b),_6b),_6b);
},"LCHuv":function(obj,_6c){
return _a["Luv"]["LCHuv"](obj,_6c);
},"RGB":function(obj,_6d){
return _4.fromXYZ(_a["Luv"]["XYZ"](obj,_6d),_6d);
},"XYZ":function(obj,_6e){
return _a["Luv"]["XYZ"](obj,_6e);
},"xyY":function(obj,_6f){
return _a["XYZ"]["xyY"](_a["Luv"]["XYZ"](obj,_6f),_6f);
}},"RGB":{"CMY":function(obj,_70){
return obj.toCmy();
},"CMYK":function(obj,_71){
return obj.toCmyk();
},"HSL":function(obj,_72){
return obj.toHsl();
},"HSV":function(obj,_73){
return obj.toHsv();
},"Lab":function(obj,_74){
return _a["XYZ"]["Lab"](obj.toXYZ(_74),_74);
},"LCHab":function(obj,_75){
return _a["LCHab"]["Lab"](_a["XYZ"]["Lab"](obj.toXYZ(_75),_75),_75);
},"LCHuv":function(obj,_76){
return _a["LCHuv"]["Luv"](_a["XYZ"]["Luv"](obj.toXYZ(_76),_76),_76);
},"Luv":function(obj,_77){
return _a["XYZ"]["Luv"](obj.toXYZ(_77),_77);
},"XYZ":function(obj,_78){
return obj.toXYZ(_78);
},"xyY":function(obj,_79){
return _a["XYZ"]["xyY"](obj.toXYZ(_79),_79);
}},"XYZ":{"CMY":function(obj,_7a){
return _4.fromXYZ(obj,_7a).toCmy();
},"CMYK":function(obj,_7b){
return _4.fromXYZ(obj,_7b).toCmyk();
},"HSL":function(obj,_7c){
return _4.fromXYZ(obj,_7c).toHsl();
},"HSV":function(obj,_7d){
return _4.fromXYZ(obj,_7d).toHsv();
},"Lab":function(obj,_7e){
return _a["XYZ"]["Lab"](obj,_7e);
},"LCHab":function(obj,_7f){
return _a["Lab"]["LCHab"](_a["XYZ"]["Lab"](obj,_7f),_7f);
},"LCHuv":function(obj,_80){
return _a["Luv"]["LCHuv"](_a["XYZ"]["Luv"](obj,_80),_80);
},"Luv":function(obj,_81){
return _a["XYZ"]["Luv"](obj,_81);
},"RGB":function(obj,_82){
return _4.fromXYZ(obj,_82);
},"xyY":function(obj,_83){
return _a["XYZ"]["xyY"](_4.fromXYZ(obj,_83),_83);
}},"xyY":{"CMY":function(obj,_84){
return _4.fromXYZ(_a["xyY"]["XYZ"](obj,_84),_84).toCmy();
},"CMYK":function(obj,_85){
return _4.fromXYZ(_a["xyY"]["XYZ"](obj,_85),_85).toCmyk();
},"HSL":function(obj,_86){
return _4.fromXYZ(_a["xyY"]["XYZ"](obj,_86),_86).toHsl();
},"HSV":function(obj,_87){
return _4.fromXYZ(_a["xyY"]["XYZ"](obj,_87),_87).toHsv();
},"Lab":function(obj,_88){
return _a["Lab"]["XYZ"](_a["xyY"]["XYZ"](obj,_88),_88);
},"LCHab":function(obj,_89){
return _a["LCHab"]["Lab"](_a["Lab"]["XYZ"](_a["xyY"]["XYZ"](obj,_89),_89),_89);
},"LCHuv":function(obj,_8a){
return _a["LCHuv"]["Luv"](_a["Luv"]["XYZ"](_a["xyY"]["XYZ"](obj,_8a),_8a),_8a);
},"Luv":function(obj,_8b){
return _a["Luv"]["XYZ"](_a["xyY"]["XYZ"](obj,_8b),_8b);
},"RGB":function(obj,_8c){
return _4.fromXYZ(_a["xyY"]["XYZ"](obj,_8c),_8c);
},"XYZ":function(obj,_8d){
return _a["xyY"]["XYZ"](obj,_8d);
}}};
this.whitepoint=function(_8e,_8f){
_8f=_8f||"10";
var x=0,y=0,t=0;
if(_7[_8f]&&_7[_8f][_8e]){
x=_7[_8f][_8e].x;
y=_7[_8f][_8e].y;
t=_7[_8f][_8e].t;
}else{
console.warn("dojox.color.Colorspace::whitepoint: either the observer or the whitepoint name was not found. ",_8f,_8e);
}
var wp={x:x,y:y,z:(1-x-y),t:t,Y:1};
return this.convert(wp,"xyY","XYZ");
};
this.tempToWhitepoint=function(t){
if(t<4000){
console.warn("dojox.color.Colorspace::tempToWhitepoint: can't find a white point for temperatures less than 4000K. (Passed ",t,").");
return {x:0,y:0};
}
if(t>25000){
console.warn("dojox.color.Colorspace::tempToWhitepoint: can't find a white point for temperatures greater than 25000K. (Passed ",t,").");
return {x:0,y:0};
}
var t1=t,t2=t*t,t3=t2*t;
var _90=Math.pow(10,9),_91=Math.pow(10,6),_92=Math.pow(10,3);
if(t<=7000){
var x=(-4.607*_90/t3)+(2.9678*_91/t2)+(0.09911*_92/t)+0.2444063;
}else{
var x=(-2.0064*_90/t3)+(1.9018*_91/t2)+(0.24748*_92/t)+0.23704;
}
var y=-3*x*x+2.87*x-0.275;
return {x:x,y:y};
};
this.primaries=function(_93){
_93=_1.mixin({profile:"sRGB",whitepoint:"D65",observer:"10",adaptor:"Bradford"},_93||{});
var m=[];
if(_8[_93.profile]){
m=_8[_93.profile].slice(0);
}else{
console.warn("dojox.color.Colorspace::primaries: the passed profile was not found.  ","Available profiles include: ",_8,".  The profile passed was ",_93.profile);
}
var _94={name:_93.profile,gamma:m[0],whitepoint:m[1],xr:m[2],yr:m[3],Yr:m[4],xg:m[5],yg:m[6],Yg:m[7],xb:m[8],yb:m[9],Yb:m[10]};
if(_93.whitepoint!=_94.whitepoint){
var r=this.convert(this.adapt({color:this.convert({x:xr,y:yr,Y:Yr},"xyY","XYZ"),adaptor:_93.adaptor,source:_94.whitepoint,destination:_93.whitepoint}),"XYZ","xyY");
var g=this.convert(this.adapt({color:this.convert({x:xg,y:yg,Y:Yg},"xyY","XYZ"),adaptor:_93.adaptor,source:_94.whitepoint,destination:_93.whitepoint}),"XYZ","xyY");
var b=this.convert(this.adapt({color:this.convert({x:xb,y:yb,Y:Yb},"xyY","XYZ"),adaptor:_93.adaptor,source:_94.whitepoint,destination:_93.whitepoint}),"XYZ","xyY");
_94=_1.mixin(_94,{xr:r.x,yr:r.y,Yr:r.Y,xg:g.x,yg:g.y,Yg:g.Y,xb:b.x,yb:b.y,Yb:b.Y,whitepoint:_93.whitepoint});
}
return _1.mixin(_94,{zr:1-_94.xr-_94.yr,zg:1-_94.xg-_94.yg,zb:1-_94.xb-_94.yb});
};
this.adapt=function(_95){
if(!_95.color||!_95.source){
console.error("dojox.color.Colorspace::adapt: color and source arguments are required. ",_95);
}
_95=_1.mixin({adaptor:"Bradford",destination:"D65"},_95);
var swp=this.whitepoint(_95.source);
var dwp=this.whitepoint(_95.destination);
if(_9[_95.adaptor]){
var ma=_9[_95.adaptor].ma;
var mai=_9[_95.adaptor].mai;
}else{
console.warn("dojox.color.Colorspace::adapt: the passed adaptor '",_95.adaptor,"' was not found.");
}
var _96=_5.multiply([[swp.x,swp.y,swp.z]],ma);
var _97=_5.multiply([[dwp.x,dwp.y,dwp.z]],ma);
var _98=[[_97[0][0]/_96[0][0],0,0],[0,_97[0][1]/_96[0][1],0],[0,0,_97[0][2]/_96[0][2]]];
var m=_5.multiply(_5.multiply(ma,_98),mai);
var r=_5.multiply([[_95.color.X,_95.color.Y,_95.color.Z]],m)[0];
return {X:r[0],Y:r[1],Z:r[2]};
};
this.matrix=function(to,_99){
var p=_99,wp=this.whitepoint(p.whitepoint);
var Xr=p.xr/p.yr,Yr=1,Zr=(1-p.xr-p.yr)/p.yr;
var Xg=p.xg/p.yg,Yg=1,Zg=(1-p.xg-p.yg)/p.yg;
var Xb=p.xb/p.yb,Yb=1,Zb=(1-p.xb-p.yb)/p.yb;
var m1=[[Xr,Yr,Zr],[Xg,Yg,Zg],[Xb,Yb,Zb]];
var m2=[[wp.X,wp.Y,wp.Z]];
var sm=_5.multiply(m2,_5.inverse(m1));
var Sr=sm[0][0],Sg=sm[0][1],Sb=sm[0][2];
var _9a=[[Sr*Xr,Sr*Yr,Sr*Zr],[Sg*Xg,Sg*Yg,Sg*Zg],[Sb*Xb,Sb*Yb,Sb*Zb]];
if(to=="RGB"){
return _5.inverse(_9a);
}
return _9a;
};
this.epsilon=function(_9b){
return (_9b||typeof (_9b)=="undefined")?0.008856:216/24289;
};
this.kappa=function(_9c){
return (_9c||typeof (_9c)=="undefined")?903.3:24389/27;
};
this.convert=function(_9d,_9e,to,_9f){
if(_1f[_9e]&&_1f[_9e][to]){
return _1f[_9e][to](_9d,_9f);
}
console.warn("dojox.color.Colorspace::convert: Can't convert ",_9d," from ",_9e," to ",to,".");
};
})();
_1.mixin(_2.color,{fromXYZ:function(xyz,_a0){
_a0=_a0||{};
var p=_2.color.Colorspace.primaries(_a0);
var m=_2.color.Colorspace.matrix("RGB",p);
var rgb=_2.math.matrix.multiply([[xyz.X,xyz.Y,xyz.Z]],m);
var r=rgb[0][0],g=rgb[0][1],b=rgb[0][2];
if(p.profile=="sRGB"){
var R=(r>0.0031308)?(1.055*Math.pow(r,1/2.4))-0.055:12.92*r;
var G=(g>0.0031308)?(1.055*Math.pow(g,1/2.4))-0.055:12.92*g;
var B=(b>0.0031308)?(1.055*Math.pow(b,1/2.4))-0.055:12.92*b;
}else{
var R=Math.pow(r,1/p.gamma),G=Math.pow(g,1/p.gamma),B=Math.pow(b,1/p.gamma);
}
return new _2.color.Color({r:Math.floor(R*255),g:Math.floor(G*255),b:Math.floor(B*255)});
}});
_1.extend(_2.color.Color,{toXYZ:function(_a1){
_a1=_a1||{};
var p=_2.color.Colorspace.primaries(_a1);
var m=_2.color.Colorspace.matrix("XYZ",p);
var _a2=this.r/255,_a3=this.g/255,_a4=this.b/255;
if(p.profile=="sRGB"){
var r=(_a2>0.04045)?Math.pow(((_a2+0.055)/1.055),2.4):_a2/12.92;
var g=(_a3>0.04045)?Math.pow(((_a3+0.055)/1.055),2.4):_a3/12.92;
var b=(_a4>0.04045)?Math.pow(((_a4+0.055)/1.055),2.4):_a4/12.92;
}else{
var r=Math.pow(_a2,p.gamma),g=Math.pow(_a3,p.gamma),b=Math.pow(_a4,p.gamma);
}
var xyz=_2.math.matrix([[r,g,b]],m);
return {X:xyz[0][0],Y:xyz[0][1],Z:xyz[0][2]};
}});
return _2.color.Colorspace;
});
