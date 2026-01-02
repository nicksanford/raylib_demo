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
#include "libavutil/pixfmt.h"
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
#include <string.h>

// static AVBufferRef *hw_device_ctx = NULL;
// static enum AVPixelFormat hw_pix_fmt;
static FILE *output_file = NULL;

// static int hw_decoder_init(AVCodecContext *ctx,
//                            const enum AVHWDeviceType type) {
//   int err = 0;
//
//   if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type, NULL, NULL, 0)) <
//   0) {
//     fprintf(stderr, "Failed to create specified HW device.\n");
//     return err;
//   }
//   ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
//
//   return err;
// }
//

int convert_yuv420_to_rgba(AVFrame *src, // AV_PIX_FMT_YUV420P
                           AVFrame *dst  // AV_PIX_FMT_RGBA
) {
  struct SwsContext *sws_ctx = sws_getContext(
      src->width, src->height, AV_PIX_FMT_YUV420P, src->width, src->height,
      AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);

  if (!sws_ctx)
    return -1;

  int ret = sws_scale_frame(sws_ctx, dst, src);

  sws_freeContext(sws_ctx);

  return ret >= 0 ? 0 : -1;
}
static enum AVPixelFormat get_format(AVCodecContext *ctx,
                                     const enum AVPixelFormat *pix_fmts) {
  const enum AVPixelFormat *p;
  char str[255] = {0};

  for (p = pix_fmts; *p != -1; p++) {
    av_get_pix_fmt_string(str, sizeof(str), *p);
    printf("option: %s\n", str);
    bzero(str, sizeof(str));
    if (*p == AV_PIX_FMT_YUV420P)
      return *p;
  }

  fprintf(stderr, "Failed to get HW surface format.\n");
  return AV_PIX_FMT_NONE;
}

static int decode_write(AVCodecContext *avctx, AVPacket *packet) {
  AVFrame *yuv_frame = NULL;
  AVFrame *rgba_frame = NULL;
  uint8_t *buffer = NULL;
  int size;
  int ret = 0;

  ret = avcodec_send_packet(avctx, packet);
  if (ret < 0) {
    fprintf(stderr, "Error during decoding\n");
    return ret;
  }

  char buf[3000] = {0};
  while (1) {
    if (!(yuv_frame = av_frame_alloc()) || !(rgba_frame = av_frame_alloc())) {
      fprintf(stderr, "Can not alloc frame\n");
      ret = AVERROR(ENOMEM);
      goto fail;
    }

    ret = avcodec_receive_frame(avctx, yuv_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      av_frame_free(&yuv_frame);
      av_frame_free(&rgba_frame);
      return 0;
    } else if (ret < 0) {
      fprintf(stderr, "Error while decoding\n");
      goto fail;
    }
    printf("frame format: %s\n",
           av_get_pix_fmt_string(buf, sizeof(buf), yuv_frame->format));
    bzero(buf, sizeof(buf));

    if (convert_yuv420_to_rgba(yuv_frame, rgba_frame) != 0) {
      fprintf(stderr, "Error while converting from yuv420 to rgba\n");
      goto fail;
    }

    size = av_image_get_buffer_size(rgba_frame->format, rgba_frame->width,
                                    rgba_frame->height, 1);
    buffer = av_malloc(size);
    if (!buffer) {
      fprintf(stderr, "Can not alloc buffer\n");
      ret = AVERROR(ENOMEM);
      goto fail;
    }
    ret = av_image_copy_to_buffer(
        buffer, size, (const uint8_t *const *)rgba_frame->data,
        (const int *)rgba_frame->linesize, rgba_frame->format,
        rgba_frame->width, rgba_frame->height, 1);
    if (ret < 0) {
      fprintf(stderr, "Can not copy image to buffer\n");
      goto fail;
    }

    if ((ret = fwrite(buffer, 1, size, output_file)) < 0) {
      fprintf(stderr, "Failed to dump raw data.\n");
      goto fail;
    }

  fail:
    av_frame_free(&yuv_frame);
    av_frame_free(&rgba_frame);
    av_freep(&buffer);
    if (ret < 0)
      return ret;
  }
}

int main(int argc, char *argv[]) {
  AVFormatContext *input_ctx = NULL;
  int video_stream, ret;
  AVStream *video = NULL;
  AVCodecContext *decoder_ctx = NULL;
  const AVCodec *decoder = NULL;
  AVPacket *packet = NULL;
  // enum AVHWDeviceType type;
  // int i;

  if (argc < 3) {
    fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
    return -1;
  }

  // avcodec_find_decoder(enum AVCodecID id)
  // type = av_hwdevice_find_type_by_name(argv[1]);
  // if (type == AV_HWDEVICE_TYPE_NONE) {
  //   fprintf(stderr, "Device type %s is not supported.\n", argv[1]);
  //   fprintf(stderr, "Available device types:");
  //   while ((type = av_hwdevice_iterate_types(type)) !=
  //   AV_HWDEVICE_TYPE_NONE)
  //     fprintf(stderr, " %s", av_hwdevice_get_type_name(type));
  //   fprintf(stderr, "\n");
  //   return -1;
  // }
  //
  packet = av_packet_alloc();
  if (!packet) {
    fprintf(stderr, "Failed to allocate AVPacket\n");
    return -1;
  }

  /* open the input file */
  if (avformat_open_input(&input_ctx, argv[2], NULL, NULL) != 0) {
    fprintf(stderr, "Cannot open input file '%s'\n", argv[2]);
    return -1;
  }

  if (avformat_find_stream_info(input_ctx, NULL) < 0) {
    fprintf(stderr, "Cannot find input stream information.\n");
    return -1;
  }

  /* find the video stream information */
  ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
  if (ret < 0) {
    fprintf(stderr, "Cannot find a video stream in the input file\n");
    return -1;
  }
  video_stream = ret;

  // for (i = 0;; i++) {
  //   const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
  //   if (!config) {
  //     fprintf(stderr, "Decoder %s does not support device type %s.\n",
  //             decoder->name, av_hwdevice_get_type_name(type));
  //     return -1;
  //   }
  //   if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
  //       config->device_type == type) {
  //     hw_pix_fmt = config->pix_fmt;
  //     break;
  //   }
  // }

  if (!(decoder_ctx = avcodec_alloc_context3(decoder)))
    return AVERROR(ENOMEM);

  video = input_ctx->streams[video_stream];
  if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0)
    return -1;

  decoder_ctx->get_format = get_format;

  // if (hw_decoder_init(decoder_ctx, type) < 0)
  //   return -1;

  if ((ret = avcodec_open2(decoder_ctx, decoder, NULL)) < 0) {
    fprintf(stderr, "Failed to open codec for stream #%u\n", video_stream);
    return -1;
  }

  /* open the file to dump raw data */
  output_file = fopen(argv[3], "w+b");

  /* actual decoding and dump the raw data */
  while (ret >= 0) {
    if ((ret = av_read_frame(input_ctx, packet)) < 0)
      break;

    if (video_stream == packet->stream_index)
      ret = decode_write(decoder_ctx, packet);

    av_packet_unref(packet);
    break;
  }

  /* flush the decoder */
  ret = decode_write(decoder_ctx, NULL);

  if (output_file)
    fclose(output_file);
  av_packet_free(&packet);
  avcodec_free_context(&decoder_ctx);
  avformat_close_input(&input_ctx);
  // av_buffer_unref(&hw_device_ctx);

  return 0;
}
