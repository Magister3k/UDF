#ifndef G723_1_DECODER_H
#define G723_1_DECODER_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
	#endif
	
	typedef struct G723DecoderContext G723DecoderContext;
	
	G723DecoderContext* __cdecl g723_create_context();
	void g723_destroy_context(G723DecoderContext* ctx);
	void g723_reset_decoder(G723DecoderContext* ctx);
	
	int __cdecl g723_decode_frame(G723DecoderContext* ctx, const unsigned char* input, double* output);
	
	#ifdef __cplusplus
}
#endif

#endif