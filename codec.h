#ifndef __CODEC_H__
#define __CODEC_H__

typedef struct codec
{
   const char *ident;
   
} codec_t;

extern codec_t codec_aw5808;
extern const codec_t *codecs[];
#endif