#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
//from decoding_encoding.c
#include "../../hidapi.c"


#ifndef PIX_FMT_RGB24
#define PIX_FMT_RGB24 AV_PIX_FMT_RGB24
#endif

//#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define avcodec_alloc_frame av_frame_alloc
//#endif

hid_device * hd;


#include <string.h>
#include <stdlib.h>

#define INBUF_SIZE 4096

static int request_w, request_h;
int video_decode( const char *filename, int reqw, int reqh);

void setup_video_decode()
{
    avcodec_register_all();
    av_register_all();
	printf( "REGISTERING\n" );
}

void clearframe()
{
	int sec;

	uint8_t rdata[300];
	memset( rdata, 0, sizeof( rdata ) );
	uint8_t * rdataptr = rdata;

	for( sec = 0; sec < 16; sec++ )
	{
		int ab;
		for( ab = 0; ab < 2; ab++ )
		{
			memset( rdata, 0, sizeof( rdata ) );
			rdataptr = rdata;
			*(rdataptr++) = 0xad; // Report ID.
			*(rdataptr++) = (sec) | (ab?0x10:0);
			int i;
			for( i = 0; i < 128; i++ )
			{
				*(rdataptr++) = 0;
			}
			int trysend = 130;
			int r = hid_send_feature_report( hd, rdata, trysend );
		}
	}
}

void got_video_frame( unsigned char * rgbbuffer, int linesize, int width, int height, int frame )
{
	printf( "%d %d\n", width, linesize );

	int sec;

	uint8_t rdata[300];
	memset( rdata, 0, sizeof( rdata ) );
	uint8_t * rdataptr = rdata;

	int seco = (128 - height)/2/8;

	for( sec = 0; sec < (height/8); sec++ )
	{
		int ab;
		for( ab = 0; ab < 2; ab++ )
		{
			memset( rdata, 0, sizeof( rdata ) );
			rdataptr = rdata;
			*(rdataptr++) = 0xad; // Report ID.
			*(rdataptr++) = (sec + seco) | (ab?0x10:0);
			int i;
			for( i = 0; i < 128; i++ )
			{
				uint8_t * line = rgbbuffer + (sec * 8 * linesize ) + (i/2 + ab*64) * 3;
				uint8_t ts = 0;
				int y;
				int rmask = 1<<(!(i&1));
				for( y = 0; y < 8; y++ )
				{
					int grey4 = (255-line[linesize * y]) / 64;
					ts |= ((grey4 & rmask)?1:0) << y;
				}
				*(rdataptr++) = ts;
			}
			int trysend = 130;
			int r = hid_send_feature_report( hd, rdata, trysend );
		}
	}

}

int main()
{
	uint8_t rdata[300];
	
	#define VID 0x303a
	#define PID 0x4004
	hid_init();
	hd = hid_open( VID, PID, L"usbsandbox000"); // third parameter is "serial"
	
	printf( "Hid device: %p\n", hd );
	if( !hd ) return -5;

	memset( rdata, 0, sizeof( rdata ) );
	uint8_t * rdataptr = rdata;
	*(rdataptr++) = 0xad; // Report ID.
	*(rdataptr++) = 0xff; // Report ID.


	*(rdataptr++) = 0x53; // LCD Bias
//	Write(COMMAND, 0x52); // Set LCD Bias=1/8 V0 (Experimentally found)

	*(rdataptr++) = 0x81; // Set Reference Voltage "Set Electronic Volume Register"
	*(rdataptr++) = 0x32; // Midway up.

	*(rdataptr++) = 0x00;
	*(rdataptr++) = 0x7B;
	*(rdataptr++) = 0x10; // Grayscale
	*(rdataptr++) = 0x00;

	*(rdataptr++) = 0xff; // teminate

	int r = hid_send_feature_report( hd, rdata, 130 );
	printf( "%d\n", r );

	clearframe();

	setup_video_decode();

	video_decode( "badapple-sm8628149.mp4", 128, 96 );
}

static int decode_write_frame( AVCodecContext *avctx,
                              AVFrame *frame, int *frame_count, AVPacket *pkt, int last, AVFrame* encoderRescaledFrame)
{
    int len, got_frame;
    char buf[1024];

    len = avcodec_decode_video2(avctx, frame, &got_frame, pkt);
    if (len < 0) {
        fprintf(stderr, "Error while decoding frame %d\n", *frame_count);
        return len;
    }

	if( request_w == 0 && request_h == 0 ) { request_w = avctx->width; request_h = avctx->height; }

    if (got_frame) {
//		uint8_t myframedata[avctx->width * avctx->height * 3];
		struct SwsContext *img_convert_ctx = sws_getContext(avctx->width, avctx->height, frame->format,
			request_w, request_h, PIX_FMT_RGB24, SWS_FAST_BILINEAR , NULL, NULL, NULL); 
		sws_scale(img_convert_ctx, ( const uint8_t * const * )frame->data, frame->linesize, 0, avctx->height,
			encoderRescaledFrame->data, encoderRescaledFrame->linesize);

		got_video_frame( encoderRescaledFrame->data[0], encoderRescaledFrame->linesize[0],
			request_w, request_h, *frame_count);

//		got_video_frame( frame->data[0], frame->linesize[0],
//			avctx->width, avctx->height, frame);

        (*frame_count)++;

		//avcodec_free_frame(&encoderRescaledFrame);
    }
    if (pkt->data) {
        pkt->size -= len;
        pkt->data += len;
    }
    return 0;
}


int video_decode( const char *filename, int reqw, int reqh)
{
	request_w = reqw;
	request_h = reqh;

	AVFrame* encoderRescaledFrame;
	AVFormatContext *fmt_ctx = 0;
	AVCodecContext *dec_ctx = 0;
	int video_stream_index;
    int frame_count = 0;
    AVFrame *frame;
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    AVPacket avpkt;
    int ret;
	int i;
    AVCodec *dec;

    av_init_packet(&avpkt);

    /* set end of buffer to 0 (this ensures that no overreading happens for damaged mpeg streams) */
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

	printf( "Opening: %s\n", filename );
    if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

//    dump_format(fmt_ctx, 0, filename, 0);

    /* select the video stream */
/*    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
        return ret;
    }*/

	for (i = 0 ; i < fmt_ctx->nb_streams; i++){
		printf( "%d\n", i );
		if (fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO ){
			video_stream_index = i;
			break;
		}

	}

    dec_ctx = fmt_ctx->streams[video_stream_index]->codec;
	dec = avcodec_find_decoder(dec_ctx->codec_id);

	encoderRescaledFrame = avcodec_alloc_frame();
	av_image_alloc(encoderRescaledFrame->data, encoderRescaledFrame->linesize,
                  dec_ctx->width, dec_ctx->height, PIX_FMT_RGB24, 1);

	printf( "Stream index: %d (%p %p)\n", video_stream_index, dec_ctx, dec );

    /* init the video decoder */
    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
        return ret;
    }



	printf( "OPENING: %d %s\n", ret, filename );

    frame = avcodec_alloc_frame();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    frame_count = 0;

    avpkt.data = NULL;
    avpkt.size = 0;
//    decode_write_frame( dec_ctx, frame, &frame_count, &avpkt, 1);

	avpkt.data = inbuf;
//	memset( &avpkt, 0, sizeof( avpkt ) );  Nope.
	while( av_read_frame(fmt_ctx, &avpkt) >= 0 )
	{
		if (avpkt.stream_index == video_stream_index)
		{
		    while (avpkt.size > 0)
			{
		        if (decode_write_frame( dec_ctx, frame, &frame_count, &avpkt, 0, encoderRescaledFrame) < 0)
		            exit(1);
			}
		}
		else
		{
		}
	}


    if (dec_ctx)
        avcodec_close(dec_ctx);
    avformat_close_input(&fmt_ctx);
//    avcodec_free_frame(&frame);
	printf( "Done?\n" );
    printf("\n");
}



