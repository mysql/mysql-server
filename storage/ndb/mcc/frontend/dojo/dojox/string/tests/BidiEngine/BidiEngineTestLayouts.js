define(["doh/runner", "dojox/string/BidiEngine"], function(doh, BidiEngine) {
	
	var unilisrc = [
		"11"
	];

	var bdEngine;
	var errorMessage = "dojox/string/BidiEngine: the bidi layout string is wrong!";
	
	doh.register('BidiEngine Parameters Test', [
		{
			name:'1. test empty',
			setUp: function() {
				bdEngine = new BidiEngine();
			},
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i){	
					doh.is('', bdEngine.bidiTransform(''), "empty string.");
				},this);
			}
		},
		{
			name:'2. empty format string.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i){	
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, '', ''), "default bidi layouts");
				},this);
			}
		},
		{	
			name:'3. empty output format',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'ILYNN'), "output format empty");
					try{
						bdEngine.set("outputFormat", "");
						throw new Error("Didn't threw error!!");
					}catch(e) {
					 doh.is(errorMessage, e.message, "should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'4. empty input format.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, '', 'ILYNN'), "input format empty");
					try{
						bdEngine.set("inputFormat", "");
						throw new Error("Didn't threw error!!");
					}catch(e){
						doh.is(errorMessage, e.message, "should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'5. wrong layouts.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					try{
						doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'V', 'I'), "wrong bidi layouts");
						throw new Error("Didn't threw error!!");
					}catch(e) {
						doh.is(errorMessage, e.message, "should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'6. Test first letter.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					try{
						doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'KLYNN', 'ILNNN'), "wrong first letter (input)");
						throw new Error("Didn't threw error!!");
					}catch(e){
						doh.is(errorMessage, e.message, "should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'7. Test first letter.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					try{
						doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VLYNN', 'KLNNN'), "wrong first letter (output)");
						throw new Error("Didn't threw error!!");
					}catch(e) {
						doh.is(errorMessage, e.message, "should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'8. Test second letter.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					try{
						doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VKYNN', 'ILNNN'), "wrong second letter (input)");
						throw new Error("Didn't threw error!!");
					}catch(e) {
						doh.is(errorMessage, e.message, "should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'9. Test second letter.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					try{
						doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VRYNN', 'IKNNN'), "wrong second letter (output)");
						throw new Error("Didn't threw error!!");
					}catch(e) {
						doh.is(errorMessage, e.message, "should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'10. Test third letter.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					try{
						doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VRSNN', 'IRNNN'), "wrong third letter (input)");
						throw new Error("Didn't threw error!!");
					}catch(e) {
						doh.is(errorMessage, e.message, "should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'11. Test third letter.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					try{
						doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VRYNN', 'IRLNN'), "wrong third letter (output)");
						throw new Error("Didn't threw error!!");
					}catch(e) {
						doh.is(errorMessage, e.message,"should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'12. Test fourth letter.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					try{
						doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VRSNN', 'IRNNN'), "wrong forth letter (input)");
						throw new Error("Didn't threw error!!");
					}catch(e) {
						doh.is(errorMessage, e.message, "should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'13. Test fourth letter.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					try{
						doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VRYNN', 'IRSNN'), "wrong forth letter (output)");
						throw new Error("Didn't threw error!!");
					}catch(e) {
					 doh.is(errorMessage, e.message,"should throw wrong format message!");
					}
				},this);
			}
		},
		{	
			name:'14. Test fifth letter.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					try{
						doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VRYNA', 'IRCNN'), "wrong fifth letter (input)");
						throw new Error("Didn't threw error!!");
					}catch(e) {
						doh.is(errorMessage, e.message, "should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'15. Test fifth letter.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					try{
						doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VRYNN', 'ICNNA'), "wrong fifth letter (output)");
						throw new Error("Didn't threw error!!");
					}catch(e) {
						doh.is(errorMessage, e.message, "should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'16. Too much letters.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					try{
						doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VRYNNN', 'IDYNN'), "too much letters (input)");
						throw new Error("Didn't threw error!!");
					}catch(e) {
						doh.is(errorMessage, e.message, "should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'16. Too much letters.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i) {
					try{
						doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VRYNN', 'ICYNNN'), "too much letters (output)");
						throw new Error("Didn't threw error!!");
					}catch(e) {
						doh.is(errorMessage, e.message, "should throw wrong format message!");
					}
				},this);
			}
		},
		{
			name:'17. Good formats.',
			runTest:function() {
				dojo.forEach(unilisrc, function(el, i){
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'ILNNN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VLNNN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'IRNNN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VRNNN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'ICNNN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'IDNNN', 'ILNNN'),"good formats");

					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'ILYNN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VLYNN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'IRYNN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VRYNN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'ICYNN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'IDYNN', 'ILNNN'),"good formats");
					
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'ILYSN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VLYSN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'IRYSN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VRYSN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'ICYSN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'IDYSN', 'ILNNN'),"good formats");

					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'ILNSN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VLNSN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'IRNSN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'VRNSN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'ICNSN', 'ILNNN'),"good formats");
					doh.is(unilisrc[i], bdEngine.bidiTransform(el, 'IDNSN', 'ILNNN'),"good formats");
				},this);
			}
		}
	]);
	
});