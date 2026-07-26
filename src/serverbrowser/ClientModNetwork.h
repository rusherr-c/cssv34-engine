#pragma once
// EVERYTHING WAS CAPTURED USING WIRESHARK
// SOME DATA MAY BE WRONG
// Fuck Reg1oxeN

// ip address
const char* cm_netip = "212.41.8.114:27070";

// 37 bytes, 475 bytes = 0x00
const unsigned char cm_cl_connectiondropped[] = 
"\x24\x21\00\12\12Connection dropped\x18\x8b\x27\x25\xc2\x0e\x5e\x73\x2d\x07\x9b\xc3\x75";


// header that is send on any request to clientmod network
// (except cl_connectiondropped)
const unsigned char cm_cl_header[] = { 0x81, 0xC2, 0x0E, 0x5E, 0x73 };

//// some random payload from wireshark:
/// --------------------------------
/// 81c20e5e7398000f0a0d0a0b084110d9
/// 0118002000284466c52d6815288ad69b
/// e7ecd3600795892870459f985588
/// 


// ##############################################
// Ecryption methods extracted from ClientModNetwork.dll 
// disassembly
// 
//	AES for x86, CRYPTOGAMS by <appro@openssl.org>
//	Vector Permutation AES for x86 / SSSE3, Mike Hamburg(Stanford University)
//	AES for Intel AES - NI, CRYPTOGAMS by <appro@openssl.org>
//	SHA1 block transform for x86, CRYPTOGAMS by <appro@openssl.org>
//	SHA256 block transform for x86, CRYPTOGAMS by <appro@openssl.org>
//	GHASH for x86, CRYPTOGAMS by <appro@openssl.org>
//	Montgomery Multiplication for x86, CRYPTOGAMS by <appro@openssl.org>
//ECP_NISZ256 for x86 / SSE2, CRYPTOGAMS by <appro@openssl.org>
//	RC4 for x86, CRYPTOGAMS by <appro@openssl.org>
//	Camellia for x86 by <appro@openssl.org>
//	ChaCha20 for x86, CRYPTOGAMS by <appro@openssl.org>
//	GF(2 ^ m) Multiplication for x86, CRYPTOGAMS by <appro@openssl.org>
//	Poly1305 for x86, CRYPTOGAMS by <appro@openssl.org>