//>>built
define("dojox/uuid/Uuid",["dojo/_base/lang","./_base"],function(_1,_2){
dojox.uuid.Uuid=function(_3){
this._uuidString=dojox.uuid.NIL_UUID;
if(_3){
dojox.uuid.assert(_1.isString(_3));
this._uuidString=_3.toLowerCase();
dojox.uuid.assert(this.isValid());
}else{
var _4=dojox.uuid.Uuid.getGenerator();
if(_4){
this._uuidString=_4();
dojox.uuid.assert(this.isValid());
}
}
};
dojox.uuid.Uuid.compare=function(_5,_6){
var _7=_5.toString();
var _8=_6.toString();
if(_7>_8){
return 1;
}
if(_7<_8){
return -1;
}
return 0;
};
dojox.uuid.Uuid.setGenerator=function(_9){
dojox.uuid.assert(!_9||_1.isFunction(_9));
dojox.uuid.Uuid._ourGenerator=_9;
};
dojox.uuid.Uuid.getGenerator=function(){
return dojox.uuid.Uuid._ourGenerator;
};
dojox.uuid.Uuid.prototype.toString=function(){
return this._uuidString;
};
dojox.uuid.Uuid.prototype.compare=function(_a){
return dojox.uuid.Uuid.compare(this,_a);
};
dojox.uuid.Uuid.prototype.isEqual=function(_b){
return (this.compare(_b)==0);
};
dojox.uuid.Uuid.prototype.isValid=function(){
return dojox.uuid.isValid(this);
};
dojox.uuid.Uuid.prototype.getVariant=function(){
return dojox.uuid.getVariant(this);
};
dojox.uuid.Uuid.prototype.getVersion=function(){
if(!this._versionNumber){
this._versionNumber=dojox.uuid.getVersion(this);
}
return this._versionNumber;
};
dojox.uuid.Uuid.prototype.getNode=function(){
if(!this._nodeString){
this._nodeString=dojox.uuid.getNode(this);
}
return this._nodeString;
};
dojox.uuid.Uuid.prototype.getTimestamp=function(_c){
if(!_c){
_c=null;
}
switch(_c){
case "string":
case String:
return this.getTimestamp(Date).toUTCString();
break;
case "hex":
if(!this._timestampAsHexString){
this._timestampAsHexString=dojox.uuid.getTimestamp(this,"hex");
}
return this._timestampAsHexString;
break;
case null:
case "date":
case Date:
if(!this._timestampAsDate){
this._timestampAsDate=dojox.uuid.getTimestamp(this,Date);
}
return this._timestampAsDate;
break;
default:
dojox.uuid.assert(false,"The getTimestamp() method dojox.uuid.Uuid was passed a bogus returnType: "+_c);
break;
}
};
return dojox.uuid.Uuid;
});
