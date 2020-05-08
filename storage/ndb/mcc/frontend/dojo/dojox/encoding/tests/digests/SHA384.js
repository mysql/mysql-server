define(['doh', '../../digests/_base', '../../digests/SHA384', "../../digests/_sha-64"], 
		function(doh, ded, SHA384, sha64){
	var message="abc";

	var vector = [
		0xcb00753f, 0x45a35e8b, 0xb5a03d69, 0x9ac65007, 
		0x272c32ab, 0x0eded163, 0x1a8b605a, 0x43ff5bed, 
		0x8086072b, 0xa1e7cc23, 0x58baeca1, 0x34c825a7
	];

	var base64="ywB1P0WjXou1oD1pmsZQBycsMqsO3tFjGotgWkP/W+2AhgcroefMI1i67KE0yCWn";
	var hex="cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7";
	var hmacKey="Jefe";
	var hmacData="what do ya want for nothing?";
	var hmacHex="af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e8e2240ca5e69e2c78b3239ecfab21649";

	console.log("Vector:", vector.map(function(item){
		return ((item >> 16) & 0xffff).toString(16) + (item & 0xffff).toString(16);
	}));
	console.log(sha64.toHex(vector));
	console.log(sha64.toBase64(vector));
	console.log(sha64.toWord(message));

	var test = SHA384(message, ded.outputTypes.Raw);
	console.log("Message: ", test.map(function(item){
		return ((item >> 16) & 0xffff).toString(16) + (item & 0xffff).toString(16);
	}));
	console.log(sha64.toHex(test));
	console.log(sha64.toBase64(test));

	doh.register("dojox.encoding.tests.digests.SHA384", [
		function testBase64Compute(t){
			t.assertEqual(base64, SHA384(message));
		},
		function testHexCompute(t){
			t.assertEqual(hex, SHA384(message, ded.outputTypes.Hex));
		},
		function testHmacCompute(t){
			t.assertEqual(hmacHex, SHA384.hmac(hmacData, hmacKey, ded.outputTypes.Hex));
		}
	]);
});
