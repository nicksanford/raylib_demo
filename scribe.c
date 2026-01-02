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

#include <assert.h>
#include <raylib.h>
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include <bstrlib.h>
#include <stdio.h>
#include <stdlib.h> // Required for: calloc(), free()
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
  const bstring filepath = bfromcstr("");

  SetTargetFPS(60); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------
  Texture2D texture = {0};
  // Detect window close button or ESC key
  while (!WindowShouldClose()) {
    // Update
    //----------------------------------------------------------------------------------
    if (IsKeyPressed(KEY_Q) &&
        (IsKeyPressed(KEY_LEFT_SUPER) || IsKeyPressed(KEY_RIGHT_SUPER))) {
      break;
    }
    if (IsFileDropped()) {
      FilePathList droppedFiles = LoadDroppedFiles();

      if (droppedFiles.count > 0) {
        assert(bassigncstr(filepath, droppedFiles.paths[0]) == BSTR_OK);
      }
      if (IsTextureValid(texture)) {
        UnloadTexture(texture);
      }

      texture = LoadTexture((const char *)filepath->data);
      fileProvided = true;

      UnloadDroppedFiles(droppedFiles); // Unload filepaths from
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
      // file is a valid image
      SetWindowSize(texture.width, texture.height);
      DrawTexture(texture, 0, 0, WHITE);
    }

    EndDrawing();
  }

  CloseWindow(); // Close window and OpenGL context

  // De-Initialization
  assert(bdestroy(filepath) != BSTR_ERR);
  return 0;
}
