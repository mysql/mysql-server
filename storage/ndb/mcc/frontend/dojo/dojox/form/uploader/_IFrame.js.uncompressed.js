define("dojox/form/uploader/_IFrame", [
	"dojo/query",
	"dojo/dom-construct",
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/_base/array",
	"dojo/dom-form",
	"dojo/request/iframe"
],function(query, domConstruct, declare, lang, arrayUtil, domForm, request){
	

	return declare("dojox.form.uploader._IFrame", [], {
		// summary:
		//		A mixin for dojox/form/Uploader that adds Ajax upload capabilities via an iframe.
		//
		// description:
		//		Only supported by IE, due to the specific iFrame hack used.  Progress events are not
		//		supported.
		//		
		//
	
		postMixInProperties: function(){
			this.inherited(arguments);
			if(this.uploadType === "iframe"){
				this.uploadType = "iframe";
				this.upload = this.uploadIFrame;
			}
		},
	
		uploadIFrame: function(data){
			// summary:
			//		Internal. You could use this, but you should use upload() or submit();
			//		which can also handle the post data.
	
			var
				formObject = {},
				sendForm,
				form = this.getForm(),
				url = this.getUrl(),
				self = this;
			data = data || {};
			data.uploadType = this.uploadType;
			
			// create a temp form for which to send data
			//enctype can't be changed once a form element is created
			sendForm = domConstruct.place('<form enctype="multipart/form-data" method="post"></form>', this.domNode);
			arrayUtil.forEach(this._inputs, function(n, i){
				// don't send blank inputs
				if(n.value !== ''){
					sendForm.appendChild(n);
					formObject[n.name] = n.value;
				}
			}, this);
			
			
			// add any extra data as form inputs		
			if(data){
				//formObject = domForm.toObject(form);
				for(nm in data){
					if(formObject[nm] === undefined){
						domConstruct.create('input', {name:nm, value:data[nm], type:'hidden'}, sendForm);
					}
				}
			}
	
			
			request.post(url, {
				form: sendForm,
				handleAs: "json",
				content: data
			}).then(function(result){
				domConstruct.destroy(sendForm);
				if(result["ERROR"] || result["error"]){
					self.onError(result);
				}else{
					self.onComplete(result);
				}
			}, function(err){
				console.error('error parsing server result', err);
				domConstruct.destroy(sendForm); 
				self.onError(err);
			});
		}
	});
});
