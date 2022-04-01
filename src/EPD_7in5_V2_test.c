/*****************************************************************************
* | File      	:   EPD_7in5_V2_test.c
* | Author      :   Waveshare team
* | Function    :   7.5inch e-paper test demo
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2019-06-13
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "EPD_Test.h"
#include "EPD_7in5_V2.h"
#include "ft2build.h"
#include "defs.h"
#include "log.h"
#include "hobbit.h"
#include FT_FREETYPE_H

BOOL init_ft (FT_Face *face, FT_Library *ft, 
               int req_size, char **error,
               const FT_Byte* file_base,
               FT_Long file_size

               )
  {
  //LOG_IN
  BOOL ret = FALSE;
  //log_debug ("Requested glyph size is %d px", req_size);
  if (FT_Init_FreeType (ft) == 0) 
    {
    //log_info ("Initialized FreeType");
    if (FT_New_Memory_Face(*ft, file_base, file_size, 0, face) == 0)
      {
      //log_info ("Loaded TTF file");
      // Note -- req_size is a request, not an instruction
      if (FT_Set_Pixel_Sizes(*face, 0, req_size) == 0)
        {
        //log_info ("Set pixel size");
        ret = TRUE;
        }
      else
        {
        //log_error ("Can't set font size to %d", req_size);
        if (error)
          *error = strdup ("Can't set font size");
        }
      }
    else
      {
      //log_error ("Can't load TTF font");
      if (error)
        *error = strdup ("Can't load TTF font");
      }
    }
  else
    {
    //log_error ("Can't initialize FreeType library"); 
    if (error)
      *error = strdup ("Can't init freetype library");
    }

  //LOG_OUT
  return ret;
  }


void done_ft (FT_Library ft)
{
  FT_Done_FreeType (ft);
}

int face_get_line_spacing (FT_Face face)
  {
  return face->size->metrics.height / 64;
  // There are other possibilities the give subtly different results:
  // return (face->bbox.yMax - face->bbox.yMin)  / 64;
  // return face->height / 64;
  }

struct _FB
{ 
  BYTE *fb_data;
};

typedef struct _FB FrameBuffer;

FrameBuffer* framebuffer_create(void) {
  FrameBuffer *self = malloc(sizeof(FrameBuffer));
  self->fb_data = (BYTE*)malloc(100*480);
  for (int i = 0; i < 48000; i++)
    self->fb_data[i] = 0U;
  return self;
}
  
// our pretend framebuffer
void framebuffer_set_pixel (FrameBuffer *self, int x, int y, BYTE r, BYTE _g, BYTE _b)
{
  /*
  these are bytes, so it looks like

  0        0:    8[0], 8[1], .. 8[99]
  100:     1:    8[0], 8[1], .. 8[99]
  200:     2:    8[0], 8[1], .. 8[99]
  ...
  4700     479:  8[0], 8[1], .. 8[99]
  */

  // landscape view
  int idx = y*100 + (x >> 3);

  const unsigned int d = 1U << 3; // So d will be one of: 1, 2, 4, 8, 16, 32, ...
  unsigned int m;                // m will be n % d
  m = x & (d - 1); 

  // i have literally no idea what is going on here
  // the order of bits seems to be reversed
  int bit = 7 - m;

  if(r == 0) {
    self->fb_data[idx] &= ~(1U << bit);

  } else {
    self->fb_data[idx] |= 1U << bit;
  }
}


/*===========================================================================

  face_draw_char_on_fb

  Draw a specific character, at a specific location, direct to the 
  framebuffer. The Y coordinate is the left-hand edge of the character.
  The Y coordinate is the top of the bounding box that contains all
  glyphs in the specific face. That is, (X,Y) are the top-left corner
  of where the largest glyph in the face would need to be drawn.
  In practice, most glyphs will be drawn a little below ths point, to
  make the baselines align. 

  The X coordinate is expressed as a pointer so it can be incremented, 
  ready for the next draw on the same line.

  =========================================================================*/
void face_draw_char_on_fb (FT_Face face, FrameBuffer *fb, 
      int c, int *x, int y)
  {
  // Note that TT fonts have no built-in padding. 
  // That is, first,
  //  the top row of the bitmap is the top row of pixels to 
  //  draw. These rows usually won't be at the face bounding box. We need to
  //  work out the overall height of the character cell, and
  //  offset the drawing vertically by that amount. 
  //
  // Similar, there is no left padding. The first pixel in each row will not
  //  be drawn at the left margin of the bounding box, but in the centre of
  //  the screen width that will be occupied by the glyph.
  //
  //  We need to calculate the x and y offsets of the glyph, but we can't do
  //  this until we've loaded the glyph, because metrics
  //  won't be available.

  // Note that, by default, TT metrics are in 64'ths of a pixel, hence
  //  all the divide-by-64 operations below.

  // Get a FreeType glyph index for the character. If there is no
  //  glyph in the face for the character, this function returns
  //  zero. We should really check for this, and substitute a default
  //  glyph. Naturally, the TTF font chosen must contain glyphs for
  //  all the characters to be displayed. 
  FT_UInt gi = FT_Get_Char_Index (face, c);

  // Loading the glyph makes metrics data available
  FT_Load_Glyph (face, gi, FT_LOAD_DEFAULT);

  // Now we have the metrics, let's work out the x and y offset
  //  of the glyph from the specified x and y. Because there is
  //  no padding, we can't just draw the bitmap so that it's
  //  TL corner is at (x,y) -- we must insert the "missing" 
  //  padding by aligning the bitmap in the space available.

  // bbox.yMax is the height of a bounding box that will enclose
  //  any glyph in the face, starting from the glyph baseline.
  int bbox_ymax = face->bbox.yMax / 64;
  // horiBearingX is the height of the top of the glyph from
  //   the baseline. So we work out the y offset -- the distance
  //   we must push down the glyph from the top of the bounding
  //   box -- from the height and the Y bearing.
  int y_off = bbox_ymax - face->glyph->metrics.horiBearingY / 64;

  // glyph_width is the pixel width of this specific glyph
  int glyph_width = face->glyph->metrics.width / 64;
  // Advance is the amount of x spacing, in pixels, allocated
  //   to this glyph
  int advance = face->glyph->metrics.horiAdvance / 64;
  // Work out where to draw the left-most row of pixels --
  //   the x offset -- by halving the space between the 
  //   glyph width and the advance
  int x_off = (advance - glyph_width) / 2;

  // So now we have (x_off,y_off), the location at which to
  //   start drawing the glyph bitmap.

  // Rendering a loaded glyph creates the bitmap
  FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);

  // Write out the glyph row-by-row using framebuffer_set_pixel
  // Note that the glyph can contain horizontal padding. We need
  //  to take this into account when working out where the pixels
  //  are in memory, but we don't actually need to "draw" these
  //  empty pixels. bitmap.width is the number of pixels that actually
  //  contain values; bitmap.pitch is the spacing between bitmap
  //  rows in memory.
  for (int i = 0; i < (int)face->glyph->bitmap.rows; i++)
    {
    // Row offset is the distance from the top of the framebuffer
    //  of this particular row of pixels in the glyph.
    int row_offset = y + i + y_off;
    for (int j = 0; j < (int)face->glyph->bitmap.width; j++)
      {
      unsigned char p =
        face->glyph->bitmap.buffer [i * face->glyph->bitmap.pitch + j];
      
      // Working out the Y position is a little fiddly. horiBearingY 
      //  is how far the glyph extends about the baseline. We push
      //  the bitmap down by the height of the bounding box, and then
      //  back up by this "bearing" value. 
      if (p)
        framebuffer_set_pixel (fb, *x + j + x_off, row_offset, p, p, p);
      }
    }
  // horiAdvance is the nominal X spacing between displayed glyphs. 
  *x += advance;
  }

/*===========================================================================

  face_draw_string_on_fb

  draw a string of UTF32 characters (null-terminated), advancing each
  character by enough to create reasonable horizontal spacing. The
  X coordinate is expressed as a pointer so it can be incremented, 
  ready for the next draw on the same line.

  =========================================================================*/
void face_draw_string_on_fb (FT_Face face, FrameBuffer *fb, const UTF32 *s, 
       int *x, int y)
  {
  while (*s)
    {
    face_draw_char_on_fb (face, fb, *s, x, y);
    s++;
    }
  }

/*===========================================================================

  face_get_char_extent

  =========================================================================*/
void face_get_char_extent (const FT_Face face, int c, int *x, int *y)
  {
  // Note that, by default, TT metrics are in 64'ths of a pixel, hence
  //  all the divide-by-64 operations below.

  // Get a FreeType glyph index for the character. If there is no
  //  glyph in the face for the character, this function returns
  //  zero. We should really check for this, and substitute a default
  //  glyph. Naturally, the TTF font chosen must contain glyphs for
  //  all the characters to be displayed. 
  FT_UInt gi = FT_Get_Char_Index(face, c);

  // Loading the glyph makes metrics data available
  FT_Load_Glyph (face, gi, FT_LOAD_NO_BITMAP);

  *y = face_get_line_spacing (face);
  *x = face->glyph->metrics.horiAdvance / 64;
  }

/*===========================================================================

  face_get_string_extent

  UTF32 characters (null-terminated), 

  =========================================================================*/
void face_get_string_extent (const FT_Face face, const UTF32 *s, 
      int *x, int *y)
  {
  *x = 0;
  int y_extent = 0;
  while (*s)
    {
    int x_extent;
    face_get_char_extent (face, *s, &x_extent, &y_extent);
    *x += x_extent;
    s++;
    }
  *y = y_extent;
  }

/*===========================================================================

  utf8_to_utf32 

  Convert an 8-bit character string to a 32-bit character string; both
  are null-terminated.

  If this weren't just a demo, we'd have a real character set 
    conversion here. It's not that difficult, but it's not really what
    this demonstration is for. For now, just pad the 8-bit characters
    to 32-bit.

  =========================================================================*/
UTF32 *utf8_to_utf32 (const UTF8 *word)
  {
  assert (word != NULL);
  int l = strlen ((char *)word);
  UTF32 *ret = malloc ((l + 1) * sizeof (UTF32));
  for (int i = 0; i < l; i++)
    {
    ret[i] = (UTF32) word[i];
    }
  ret[l] = 0;
  return ret;
  }


/*

  EPD test function

*/

int EPD_7in5_V2_test(void)
{
    int width = 800;
    int height = 480;
    int log_level = LOG_DEBUG;
    int font_size = 20;
    int init_x = 5;
    int init_y = 5;

    char *error = NULL;

    printf("EPD_7IN5_V2_test Demo\r\n");
    if(DEV_Module_Init()!=0){
        return -1;
    }

    printf("e-Paper Init and Clear...\r\n");
    EPD_7IN5_V2_Init();

    EPD_7IN5_V2_Clear();
    DEV_Delay_ms(500);

    FT_Face face;
    FT_Library ft;
    extern uint32_t liberation_serif_regular_size;
    extern uint8_t liberation_serif_regular_data[34260];

    if (init_ft (&face, &ft, font_size, &error, (FT_Byte*)liberation_serif_regular_data, liberation_serif_regular_size))
    {
      printf("Font face initialized OK\n");
    };

	  FrameBuffer *fb = framebuffer_create();

    //Create a new image cache
    UBYTE *BlackImage;
    /* you have to edit the startup_stm32fxxx.s file and set a big enough heap size */
    UWORD Imagesize = ((EPD_7IN5_V2_WIDTH % 8 == 0)? (EPD_7IN5_V2_WIDTH / 8 ): (EPD_7IN5_V2_WIDTH / 8 + 1)) * EPD_7IN5_V2_HEIGHT;
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        return -1;
    }

    
    static const UTF32 utf32_space[2] = {' ', 0};

    int space_y;
    int space_x; // Pixel width of a space
    face_get_string_extent (face, utf32_space, &space_x, &space_y); 

    printf ("Obtained a face whose space has height %d px\n", space_y);
    printf ("Line spacing is %d px\n", face_get_line_spacing (face));

    // x and y are the current coordinates of the top-left corner of
    //  the bounding box of the text being written, relative to the
    //  TL corner of the screen.
    int x = init_x;
    int y = init_y;

    printf ("Starting drawing at %d,%d\n", x, y);
    int line_spacing = face_get_line_spacing (face);

    // Loop around the remaining arguments to the program, printing
    //  each word, followed by a space.
    

    const char *words[3];
    words[0] = "one";
    words[1] = "two";
    words[2] = "three";

  // for (int i = 0; i < 3; i++)
    for (int i = 0; i < hobbit_size; i++)
      {
      //const char* word = words[i];
      const char* word = hobbit[i];
      printf ("Next word is %s\n", word);

      // The face_xxx text handling functions take UTF32 character strings
      //  as input.
      UTF32 *word32 = utf8_to_utf32 ((UTF8 *)word);
      
      // Get the extent of the bounding box of this word, to see 
      //  if it will fit in the specified width.
      int x_extent, y_extent;
      face_get_string_extent (face, word32, &x_extent, &y_extent); 
      int x_advance = x_extent + space_x;
      printf ("Word width is %d px; would advance X position by %d\n", x_extent, x_advance);

      // If the text won't fit, move down to the next line
      if (x + x_advance > width) 
        {
        printf ("Text too large for bounds -- move to next line\n");
        x = init_x; 
        y += line_spacing;
        }
      // If we're already below the specified height, don't write anything
      if (y + line_spacing < init_y + height)
        {
        face_draw_string_on_fb (face, fb, word32, &x, y);
        face_draw_string_on_fb (face, fb, utf32_space, &x, y);
        }
      free (word32);
      }

    done_ft (ft);

    // convert all this into freetype functions
    // Paint_NewImage
    // Paint_Clear
    // Paint_SelectImage
    // Paint_DrawBitMap
    // Paint_DrawString_EN
    // EPD_7IN5_V2_Display
    // EPD_7IN5_V2_Clear
    //
    // maybe the types might indicate what they do
    Paint_NewImage(BlackImage, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, 0, WHITE);     

#if 1   // show image for array   
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
    Paint_DrawBitMap(gImage_7in5_V2);
    //EPD_7IN5_V2_Display(BlackImage); // why is this commented out?
    DEV_Delay_ms(2000);
#endif

#if 1   // Drawing on the image
    //1.Select Image
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);

    // 2.Drawing on the image
    printf("Drawing:BlackImage\r\n");
    Paint_DrawString_EN(10, 20, "hello world", &Font12, WHITE, BLACK);

    printf("EPD_Display\r\n");
    //EPD_7IN5_V2_Display(BlackImage);
    
    EPD_7IN5_V2_Display((UBYTE*)fb->fb_data);

    DEV_Delay_ms(2000);
#endif

    printf("Clear...\r\n");
    EPD_7IN5_V2_Clear();

    printf("Goto Sleep...\r\n");
    EPD_7IN5_V2_Sleep();
    free(BlackImage);
    BlackImage = NULL;
    DEV_Delay_ms(2000);//important, at least 2s
    // close 5V
    printf("close 5V, Module enters 0 power consumption ...\r\n");
    DEV_Module_Exit();
    
    return 0;
}

