/*******************************************************************************************
 *
 *   raylib [core] example - Windows drop files
 *
 *   NOTE: This example only works on platforms that support drag & drop
 *(Windows, Linux, OSX, Html5?)
 *
 *   Example originally created with raylib 1.3, last time updated with
 *raylib 4.2
 *
 *   Example licensed under an unmodified zlib/libpng license, which is an
 *OSI-certified, BSD-like license that allows static linking with closed source
 *software
 *
 *   Copyright (c) 2015-2024 Ramon Santamaria (@raysan5)
 *
 ********************************************************************************************/

#include "raylib/src/raylib.h"
#include <assert.h>
#include <raylib.h>
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include "libavcodec/codec.h"
#include "libavcodec/packet.h"
#include "libavutil/error.h"
#include "libavutil/pixfmt.h"
#include <assert.h>
#include <bstrlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // Required for: calloc(), free()

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
  int fps;
  int width;
  int height;

  SwsContext *sws_ctx;
} scribe_decoder_ctx;

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

  fprintf(stderr, "Failed to get format.\n");
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

  ctx->fps = video->avg_frame_rate.num;
  ctx->width = video->codecpar->width;
  ctx->height = video->codecpar->height;

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

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void) {
  // Initialization
  //--------------------------------------------------------------------------------------
  const int windowWidth = 800;
  const int windowHeight = 450;
  bool fileProvided = false;

  InitWindow(windowWidth, windowHeight,
             "raylib [core] example - drop an image file");
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  const bstring filepath = bfromcstr("");

  int targetFPS = 60;
  printf("GetScreenWidth: %d\n", GetScreenWidth());
  printf("GetScreenHeight: %d\n", GetScreenHeight());
  SetTargetFPS(targetFPS); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------
  Texture2D texture = {0};
  Image image = {0};
  scribe_decoder_ctx *video_ctx = NULL;
  int err = 0;
  // Detect window close button or ESC key
  while (!WindowShouldClose()) {
    // Update
    //----------------------------------------------------------------------------------
    if (IsKeyPressed(KEY_Q) &&
        (IsKeyPressed(KEY_LEFT_SUPER) || IsKeyPressed(KEY_RIGHT_SUPER))) {
      break;
    }

    if (IsKeyPressed(KEY_T)) {
      ToggleFullscreen();
    }

    // when a file is dropped
    if (IsFileDropped()) {
      FilePathList droppedFiles = LoadDroppedFiles();

      if (droppedFiles.count > 0) {
        if (video_ctx) {
          scribe_decoder_ctx_free(&video_ctx);
        }

        assert(bassigncstr(filepath, droppedFiles.paths[0]) == BSTR_OK);
        fileProvided = true;
        if (IsFileExtension((const char *)filepath->data, ".mp4")) {
          if (scribe_decoder_ctx_init(&video_ctx, (char *)filepath->data)) {
            fprintf(stderr, "failed to init decoder");
            return -1;
          }
          SetTargetFPS(targetFPS);
          SetWindowSize(video_ctx->width, video_ctx->height);
        } else {
          if (IsTextureValid(texture)) {
            UnloadTexture(texture);
          }

          texture = LoadTexture((const char *)filepath->data);
          SetWindowSize(texture.width, texture.height);
        }
      }

      UnloadDroppedFiles(droppedFiles); // Unload filepaths from
    }

    // get next video frame if there is a video
    if (video_ctx) {
      err = decode_to_buffer(video_ctx);
      assert(err >= 0);
      if (err == 1) {
        image.width = video_ctx->rgba_frame->width;
        image.height = video_ctx->rgba_frame->height;
        image.mipmaps = 1;
        image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        // TODO: This might be dangerous
        image.data = video_ctx->buffer;
        // SetTargetFPS(video_ctx->fps);
        UnloadTexture(texture);
        texture = LoadTextureFromImage(image);
      } else {
        scribe_decoder_ctx_free(&video_ctx);
      }
    }

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    if (!IsTextureValid(texture) && !fileProvided) {
      // file is not a valid image
      DrawText("Drop your file into this window.", 100, 40, 20, DARKGRAY);
    } else if (!IsTextureValid(texture)) {
      DrawText("That is not a valid file type.", 100, 40, 20, DARKGRAY);
      DrawText("Drop your file into this window.", 100, 60, 20, DARKGRAY);
    } else {
      // texture is valid
      DrawTexture(texture, 0, 0, WHITE);
    }
    DrawFPS(100, 100);

    EndDrawing();
  }

  CloseWindow(); // Close window and OpenGL context

  // De-Initialization
  assert(bdestroy(filepath) != BSTR_ERR);
  if (video_ctx) {
    scribe_decoder_ctx_free(&video_ctx);
  }
  return 0;
}
