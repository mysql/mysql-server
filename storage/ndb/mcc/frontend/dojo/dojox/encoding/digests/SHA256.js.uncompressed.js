define("dojox/encoding/digests/SHA256", ["./_sha-32", "./_sha2"], function(sha32, sha2){
	//	The 256-bit implementation of SHA-2
	var hash = [
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	];

	return sha2(sha32, 256, 512, hash);
});
