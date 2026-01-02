/*
 * Copyright (c) 2017 Jun Zhao
 * Copyright (c) 2017 Kaixuan Liu
 *
 * HW Acceleration API (video decoding) decode sample
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file HW-accelerated decoding API usage.example
 * @example hw_decode.c
 *
 * Perform HW-accelerated decoding with output frames from HW video
 * surfaces.
 */

#include "libavcodec/codec.h"
#include "libavcodec/packet.h"
#include "libavutil/error.h"
#include "libavutil/log.h"
#include "libavutil/pixfmt.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avassert.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <stdlib.h>
#include <string.h>

#define SCRIBE_SOURCE_PIX_FMT AV_PIX_FMT_YUV420P
#define SCRIBE_TARGET_PIX_FMT AV_PIX_FMT_RGBA

typedef struct Image {
  void *data;  // Image raw data
  int width;   // Image base width
  int height;  // Image base height
  int mipmaps; // Mipmap levels, 1 by default
  int format;  // Data format (PixelFormat type)
} Image;

typedef struct scribe_decoder_ctx {
  AVFormatContext *input_ctx;
  int video_stream;
  AVStream *video;
  AVCodecContext *decoder_ctx;
  AVPacket *packet;

  AVFrame *yuv_frame;
  AVFrame *rgba_frame;
  uint8_t *buffer;
  int buffer_size;

  SwsContext *sws_ctx;
} scribe_decoder_ctx;

// int convert_to_rgba(AVFrame *src, AVFrame *dst) {
//   SwsContext *sws_ctx = sws_getContext(
//       src->width, src->height, src->format, src->width, src->height,
//       SCRIBE_TARGET_PIX_FMT, SWS_BILINEAR, NULL, NULL, NULL);
//
//   if (!sws_ctx) {
//     return -1;
//   }
//
//   int ret = sws_scale_frame(sws_ctx, dst, src);
//
//   sws_freeContext(sws_ctx);
//
//   return ret >= 0 ? 0 : -1;
// }

static enum AVPixelFormat get_format(AVCodecContext *ctx,
                                     const enum AVPixelFormat *pix_fmts) {
  const enum AVPixelFormat *p;
  char str[255] = {0};

  for (p = pix_fmts; *p != -1; p++) {
    av_get_pix_fmt_string(str, sizeof(str), *p);
    printf("option: %s\n", str);
    bzero(str, sizeof(str));
    if (*p == SCRIBE_SOURCE_PIX_FMT) {
      return *p;
    }
  }

  fprintf(stderr, "Failed to get HW surface format.\n");
  return AV_PIX_FMT_NONE;
}

/**
 * decode_to_buffer
 *
 * returns 0 when done
 *
 * returns 1 when buffer is ready to read
 *
 * returns other negative number on unrecoverable error
 **/
static int decode_to_buffer(scribe_decoder_ctx *ctx) {
  int err = 0;
  // char buf[3000] = {0};
  while (1) {
    if ((err = av_read_frame(ctx->input_ctx, ctx->packet)) < 0) {
      fprintf(stderr, "Failed to read frame, err: %s\n", av_err2str(err));
      return err;
    }

    if (ctx->video_stream != ctx->packet->stream_index) {
      continue;
    }

    // printf("\n");
    // printf("\n");
    // printf("-----------\n");
    // printf("ctx->video_stream: %d ctx->packet->stream_index: %d, pts %lld, "
    //        "pos: %lld, flags: %d\n",
    //        ctx->video_stream, ctx->packet->stream_index, ctx->packet->pts,
    //        ctx->packet->pos, ctx->packet->flags);
    // AVStream *video = ctx->input_ctx->streams[ctx->video_stream];
    // printf("video_index: %d\n", video->index);
    // printf("video_id: %d\n", video->id);
    // printf("avg_frame_rate: %d/%d\n", video->avg_frame_rate.num,
    //        video->avg_frame_rate.den);
    // printf("duration: %lld\n", video->duration);
    // printf("width: %d, height: %d, codec_id: %d, format: %d\n",
    //        video->codecpar->width, video->codecpar->height,
    //        video->codecpar->codec_id, video->codecpar->format);
    // printf("-----------\n");

    if ((err = avcodec_send_packet(ctx->decoder_ctx, ctx->packet)) < 0) {
      fprintf(stderr, "Error decoding packet: err: %s\n", av_err2str(err));
      return err;
    }
    av_packet_unref(ctx->packet);

    err = avcodec_receive_frame(ctx->decoder_ctx, ctx->yuv_frame);
    if (err == AVERROR_EOF) {
      fprintf(stderr, "done, err: %s\n", av_err2str(err));
      return 0;
    } else if (err == AVERROR(EAGAIN)) {
      // need more data
      // TEST
      fprintf(stderr, "need more data to receive frame, err: %s\n",
              av_err2str(err));
      continue;
    } else if (err < 0) {
      // decoder error
      fprintf(stderr, "Error while decoding, err: %s\n", av_err2str(err));
      return err;
    }
    // otherwise err == 0 which is a success and we have a frame

    // printf("frame format: %s\n",
    //        av_get_pix_fmt_string(buf, sizeof(buf),
    //        ctx->yuv_frame->format));
    // bzero(buf, sizeof(buf));

    if ((err = sws_scale_frame(ctx->sws_ctx, ctx->rgba_frame, ctx->yuv_frame)) <
        0) {
      fprintf(stderr, "Error while converting from yuv420 to rgba: %s\n",
              av_err2str(err));
      return err;
    }

    if ((err = av_image_copy_to_buffer(
             ctx->buffer, ctx->buffer_size,
             (const uint8_t *const *)ctx->rgba_frame->data,
             (const int *)ctx->rgba_frame->linesize, ctx->rgba_frame->format,
             ctx->rgba_frame->width, ctx->rgba_frame->height, 1)) < 0) {
      fprintf(stderr, "Can not copy image to buffer, err: %s\n",
              av_err2str(err));
      return err;
    }
    return 1;
  }
}
void scribe_decoder_ctx_free(scribe_decoder_ctx **p_ctx) {
  assert(p_ctx != NULL);
  assert(*p_ctx != NULL);

  if ((*p_ctx)->packet) {
    av_packet_free(&(*p_ctx)->packet);
  }

  if ((*p_ctx)->input_ctx) {
    avformat_close_input(&(*p_ctx)->input_ctx);
  }

  if ((*p_ctx)->decoder_ctx) {
    avcodec_free_context(&(*p_ctx)->decoder_ctx);
  }

  if ((*p_ctx)->yuv_frame) {
    av_frame_free(&(*p_ctx)->yuv_frame);
  }

  if ((*p_ctx)->rgba_frame) {
    av_frame_free(&(*p_ctx)->rgba_frame);
  }

  if ((*p_ctx)->buffer) {
    av_freep(&(*p_ctx)->buffer);
  }

  if ((*p_ctx)->sws_ctx) {
    sws_freeContext((*p_ctx)->sws_ctx);
  }

  free(*p_ctx);
  *p_ctx = NULL;
}

scribe_decoder_ctx *scribe_decoder_ctx_alloc(void) {
  return calloc(1, sizeof(scribe_decoder_ctx));
}

int scribe_decoder_ctx_init(scribe_decoder_ctx **p_ctx, // OUT
                            char *filename) {
  scribe_decoder_ctx *ctx = *p_ctx;
  int err = 0;
  AVStream *video = NULL;
  const AVCodec *decoder = NULL;
  if (!(ctx = scribe_decoder_ctx_alloc())) {
    fprintf(stderr, "Failed to allocate scribe_decoder_ctx\n");
    goto fail;
  };
  if (!(ctx->packet = av_packet_alloc())) {
    fprintf(stderr, "Failed to allocate AVPacket\n");
    goto fail;
  }

  /* open the input file */
  if ((err = avformat_open_input(&ctx->input_ctx, filename, NULL, NULL))) {
    fprintf(stderr, "Cannot open input file '%s', err: %s\n", filename,
            av_err2str(err));
    goto fail;
  }

  if ((err = avformat_find_stream_info(ctx->input_ctx, NULL)) < 0) {
    fprintf(stderr, "Cannot find input stream information. %s\n",
            av_err2str(err));
    goto fail;
  }

  /* find the video stream information */
  if ((err = av_find_best_stream(ctx->input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1,
                                 &decoder, 0)) < 0) {
    fprintf(stderr, "Cannot find a video stream in the input file: %s\n",
            av_err2str(err));
    goto fail;
  }
  ctx->video_stream = err;
  printf("best stream: %d\n", ctx->video_stream);

  if (!(ctx->decoder_ctx = avcodec_alloc_context3(decoder))) {
    fprintf(stderr, "Cannot allocate decoder context\n");
    goto fail;
  }

  video = ctx->input_ctx->streams[ctx->video_stream];
  printf("video_index: %d\n", video->index);
  printf("video_id: %d\n", video->id);
  printf("avg_frame_rate: %d/%d\n", video->avg_frame_rate.num,
         video->avg_frame_rate.den);
  printf("duration: %lld\n", video->duration);
  printf("width: %d, height: %d, codec_id: %d, format: %d\n",
         video->codecpar->width, video->codecpar->height,
         video->codecpar->codec_id, video->codecpar->format);
  // exit(1);
  if ((err = avcodec_parameters_to_context(ctx->decoder_ctx, video->codecpar)) <
      0) {
    fprintf(stderr, "Cannot get codec parameters from decoder context: %s\n",
            av_err2str(err));
    goto fail;
  }

  ctx->decoder_ctx->get_format = get_format;
  if ((err = avcodec_open2(ctx->decoder_ctx, decoder, NULL)) < 0) {
    fprintf(stderr, "Failed to open codec for stream #%u, err: %s\n",
            ctx->video_stream, av_err2str(err));
    goto fail;
  }

  if (!(ctx->yuv_frame = av_frame_alloc()) ||
      !(ctx->rgba_frame = av_frame_alloc())) {
    fprintf(stderr, "Can not alloc frame\n");
    goto fail;
  }

  if ((err = av_image_get_buffer_size(SCRIBE_TARGET_PIX_FMT,
                                      video->codecpar->width,
                                      video->codecpar->height, 1)) < 0) {
    fprintf(stderr, "Failed to comupte buffer size: err: %s\n",
            av_err2str(err));
    goto fail;
  }
  ctx->buffer_size = err;

  if (!(ctx->buffer = av_malloc(ctx->buffer_size))) {
    fprintf(stderr, "Can not alloc buffer\n");
    goto fail;
  }

  if (!(ctx->sws_ctx =
            sws_getContext(video->codecpar->width, video->codecpar->height,
                           SCRIBE_SOURCE_PIX_FMT, video->codecpar->width,
                           video->codecpar->height, SCRIBE_TARGET_PIX_FMT,
                           SWS_BILINEAR, NULL, NULL, NULL))) {
    fprintf(stderr, "Can not create sws context\n");
    goto fail;
  }
  *p_ctx = ctx;

  return 0;

fail:
  if (ctx) {
    scribe_decoder_ctx_free(&ctx);
  }
  fprintf(stderr, "failed\n");
  assert(1 == 2);
  return -1;
}

int main(int argc, char *argv[]) {
  // av_log_set_level(AV_LOG_DEBUG);
  scribe_decoder_ctx *ctx = NULL;
  int err = 0;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <input file> <output directory>\n", argv[0]);
    return -1;
  }

  if (scribe_decoder_ctx_init(&ctx, argv[1])) {
    fprintf(stderr, "failed oto init decoder");
    return -1;
  }
  printf("ptr: %p\n", (void *)ctx);

  /* actual decoding and dump the raw data */
  FILE *f = NULL;
  char buf[1024];
  while (true) {
    err = decode_to_buffer(ctx);
    if (err == 1) {
      printf("pts: %lld\n", ctx->yuv_frame->pts);
      bzero(buf, sizeof(buf));
      snprintf(buf, sizeof(buf), "%s/%lld.rgba", argv[2], ctx->yuv_frame->pts);
      f = fopen(buf, "wb");
      fwrite(ctx->buffer, ctx->buffer_size, 1, f);
      fclose(f);
      continue;
    }
    break;
  }

  if (ctx) {
    scribe_decoder_ctx_free(&ctx);
  }
  return 0;
}
