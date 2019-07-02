
#include <stdio.h>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
}

const char* SRC_FILE = "audio.mkv";
const char* OUT_FILE = "outfile.h264";
const char* OUT_FMT_FILE = "outfmtfile.mp4";
int main()
{
	av_register_all();



	AVFormatContext* pFormat = NULL;
	if (avformat_open_input(&pFormat, SRC_FILE, NULL, NULL) < 0)
	{
		return 0;
	}
	AVCodecContext* video_dec_ctx = NULL;
	AVCodec* video_dec = NULL;
	if (avformat_find_stream_info(pFormat, NULL) < 0)
	{
		return 0;
	}
	av_dump_format(pFormat, 0, SRC_FILE, 0);
	video_dec_ctx = pFormat->streams[0]->codec;
	video_dec = avcodec_find_decoder(video_dec_ctx->codec_id);
	if (avcodec_open2(video_dec_ctx, video_dec, NULL) < 0)
	{
		return 0;
	}

	AVFormatContext* pOFormat = NULL;
	AVOutputFormat* ofmt = NULL;
	if (avformat_alloc_output_context2(&pOFormat, NULL, NULL, OUT_FILE) < 0)
	{
		return 0;
	}
	ofmt = pOFormat->oformat;
	if (avio_open(&(pOFormat->pb), OUT_FILE, AVIO_FLAG_READ_WRITE) < 0)
	{
		return 0;
	}
	AVCodecContext *video_enc_ctx = NULL;
	AVCodec *video_enc = NULL;
	video_enc = avcodec_find_encoder(AV_CODEC_ID_H264);
	AVStream *video_st = avformat_new_stream(pOFormat, video_enc);
	if (!video_st)
		return 0;
	video_enc_ctx = video_st->codec;
	video_enc_ctx->width = video_dec_ctx->width;
	video_enc_ctx->height = video_dec_ctx->height;
	video_enc_ctx->pix_fmt = PIX_FMT_YUV420P;
	video_enc_ctx->time_base.num = 1;
	video_enc_ctx->time_base.den = 25;
	video_enc_ctx->bit_rate = video_dec_ctx->bit_rate;
	video_enc_ctx->gop_size = 250;
	video_enc_ctx->max_b_frames = 10;
	//H264 
	//pCodecCtx->me_range = 16; 
	//pCodecCtx->max_qdiff = 4; 
	video_enc_ctx->qmin = 10;
	video_enc_ctx->qmax = 51;
	if (avcodec_open2(video_enc_ctx, video_enc, NULL) < 0)
	{
		printf("±àÂëÆ÷´ò¿ªÊ§°Ü£¡\n");
		return 0;
	}
	printf("Output264video Information====================\n");
	av_dump_format(pOFormat, 0, OUT_FILE, 1);
	printf("Output264video Information====================\n");

	//mp4 file
	AVFormatContext* pMp4Format = NULL;
	AVOutputFormat* pMp4OFormat = NULL;
	if (avformat_alloc_output_context2(&pMp4Format, NULL, NULL, OUT_FMT_FILE) < 0)
	{
		return 0;
	}
	pMp4OFormat = pMp4Format->oformat;
	if (avio_open(&(pMp4Format->pb), OUT_FMT_FILE, AVIO_FLAG_READ_WRITE) < 0)
	{
		return 0;
	}

	for (int i = 0; i < pFormat->nb_streams; i++) {
		AVStream *in_stream = pFormat->streams[i];
		AVStream *out_stream = avformat_new_stream(pMp4Format, in_stream->codec->codec);
		if (!out_stream) {
			return 0;
		}
		int ret = 0;
		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		if (ret < 0) {
			fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
			return 0;
		}
		out_stream->codec->codec_tag = 0;
		if (pMp4Format->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}


	av_dump_format(pMp4Format, 0, OUT_FMT_FILE, 1);

	if (avformat_write_header(pMp4Format, NULL) < 0)
	{
		return 0;
	}


	////



	av_opt_set(video_enc_ctx->priv_data, "preset", "superfast", 0);
	av_opt_set(video_enc_ctx->priv_data, "tune", "zerolatency", 0);
	avformat_write_header(pOFormat, NULL);
	AVPacket *pkt = new AVPacket();
	av_init_packet(pkt);
	AVFrame *pFrame = avcodec_alloc_frame();
	int ts = 0;
	while (1)
	{
		if (av_read_frame(pFormat, pkt) < 0)
		{
			avio_close(pOFormat->pb);
			av_write_trailer(pMp4Format);
			avio_close(pMp4Format->pb);
			delete pkt;
			return 0;
		}
		if (pkt->stream_index == 0)
		{

			int got_picture = 0, ret = 0;
			ret = avcodec_decode_video2(video_dec_ctx, pFrame, &got_picture, pkt);
			if (ret < 0)
			{
				delete pkt;
				return 0;
			}
			pFrame->pts = pFrame->pkt_pts;//ts++;
			if (got_picture)
			{
				AVPacket *tmppkt = new AVPacket;
				av_init_packet(tmppkt);
				int size = video_enc_ctx->width*video_enc_ctx->height * 3 / 2;
				char* buf = new char[size];
				memset(buf, 0, size);
				tmppkt->data = (uint8_t*)buf;
				tmppkt->size = size;
				ret = avcodec_encode_video2(video_enc_ctx, tmppkt, pFrame, &got_picture);
				if (ret < 0)
				{
					avio_close(pOFormat->pb);
					delete buf;
					return 0;
				}
				if (got_picture)
				{
					//ret = av_interleaved_write_frame(pOFormat, tmppkt);
					AVStream *in_stream = pFormat->streams[pkt->stream_index];
					AVStream *out_stream = pMp4Format->streams[pkt->stream_index];

					tmppkt->pts = av_rescale_q_rnd(tmppkt->pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
					tmppkt->dts = av_rescale_q_rnd(tmppkt->dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
					tmppkt->duration = av_rescale_q(tmppkt->duration, in_stream->time_base, out_stream->time_base);
					tmppkt->pos = -1;
					ret = av_interleaved_write_frame(pMp4Format, tmppkt);
					if (ret < 0)
						return 0;
					delete tmppkt;
					delete buf;
				}
			}
			//avcodec_free_frame(&pFrame);
		}
		else if (pkt->stream_index == 1)
		{
			AVStream *in_stream = pFormat->streams[pkt->stream_index];
			AVStream *out_stream = pMp4Format->streams[pkt->stream_index];

			pkt->pts = av_rescale_q_rnd(pkt->pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
			pkt->dts = av_rescale_q_rnd(pkt->dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
			pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
			pkt->pos = -1;
			if (av_interleaved_write_frame(pMp4Format, pkt) < 0)
				return 0;
		}
	}
	avcodec_free_frame(&pFrame);
	return 0;
}
