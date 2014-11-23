#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <string.h>

int symmetricEncrypt(unsigned char* ptxt, int ptxtLen, unsigned char* key, unsigned char* IV, unsigned char* ctxt)
{
	EVP_CIPHER_CTX *ctx;
	int evpLen;
	int ctxtLen;
	if(!(ctx = EVP_CIPHER_CTX_new()))
		abort();
	if(EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, IV) != 1)
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

int symmetricDecrypt(unsigned char* ctxt, int ctxtLen, unsigned char* key, unsigned char* IV, unsigned char* ptxt)
{
	EVP_CIPHER_CTX *ctx;
	int evpLen;
	int ptxtLen;
	if(!(ctx = EVP_CIPHER_CTX_new()))
		abort();
	if(EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, IV) != 1)
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
