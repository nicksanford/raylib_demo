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

#include "raylib.h"
#include <assert.h>
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include "bstrlib-1.0.0/bstrlib.h"
#include <stdio.h>
#include <stdlib.h> // Required for: calloc(), free()
#define MAX_FILEPATH_SIZE 2048

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void) {
  // Initialization
  //--------------------------------------------------------------------------------------
  const int screenWidth = 800;
  const int screenHeight = 450;

  InitWindow(screenWidth, screenHeight, "raylib [core] example - drop a file");
  const bstring filepath = bfromcstr("");

  // Allocate space for the required file paths

  SetTargetFPS(60); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  // Detect window close button or ESC key
  while (!WindowShouldClose()) {
    // Update
    //----------------------------------------------------------------------------------
    if (IsFileDropped()) {
      FilePathList droppedFiles = LoadDroppedFiles();

      if (droppedFiles.count > 0) {
        assert(bassigncstr(filepath, droppedFiles.paths[0]) == BSTR_OK);
      }

      UnloadDroppedFiles(droppedFiles); // Unload filepaths from
    }

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    if (filepath->slen == 0) {
      DrawText("Drop your file into this window!", 100, 40, 20, DARKGRAY);
    } else {
      DrawText("Dropped file:", 100, 40, 20, DARKGRAY);
      DrawRectangle(0, 85, screenWidth, 40, Fade(LIGHTGRAY, 0.5f));
      DrawText((const char *)filepath->data, 120, 100, 10, GRAY);
    }

    EndDrawing();
  }

  CloseWindow(); // Close window and OpenGL context

  // De-Initialization
  assert(bdestroy(filepath) != BSTR_ERR);
  return 0;
}
