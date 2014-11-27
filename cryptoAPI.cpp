#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <string.h>

unsigned char IV[17] = "AAAAAAAAAAAAAAA";
unsigned char KEY[33] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

int symmetric_encrypt(unsigned char* ptxt, unsigned char* ctxt)
{
	EVP_CIPHER_CTX *ctx;
	int ptxtLen = 15;
	int evpLen;
	int ctxtLen;
	if(!(ctx = EVP_CIPHER_CTX_new()))
		abort();
	if(EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, KEY, IV) != 1)
		abort();
	if(EVP_EncryptUpdate(ctx, ctxt, &evpLen, ptxt, ptxtLen) != 1)
		abort();
	ctxtLen = evpLen;
	if(EVP_EncryptFinal_ex(ctx, ctxt + evpLen, &evpLen) != 1)
		abort();
	ctxtLen += evpLen;
	EVP_CIPHER_CTX_free(ctx);
	return ctxtLen;
}

int symmetric_decrypt(unsigned char* ctxt, unsigned char* ptxt)
{
	EVP_CIPHER_CTX *ctx;
	int ctxtLen = 16;
	int evpLen;
	int ptxtLen;
	if(!(ctx = EVP_CIPHER_CTX_new()))
		abort();
	if(EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, KEY, IV) != 1)
		abort();
	if(EVP_DecryptUpdate(ctx, ptxt, &evpLen, ctxt, ctxtLen) != 1)
		abort();
	ptxtLen = evpLen;
	if(EVP_DecryptFinal_ex(ctx, ptxt + evpLen, &evpLen) != 1)
		abort();
	ptxtLen += evpLen;
	EVP_CIPHER_CTX_free(ctx);
	return ptxtLen;
}
