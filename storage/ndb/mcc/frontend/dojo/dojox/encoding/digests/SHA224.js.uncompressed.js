define("dojox/encoding/digests/SHA224", ["./_sha-32", "./_sha2"], function(sha32, sha2){
	//	The 224-bit implementation of SHA-2
	var hash = [
		0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939,
		0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4
	];

	return sha2(sha32, 224, 512, hash);
});
