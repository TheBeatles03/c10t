#include "Image.h"
#include "global.h"

#include <assert.h>

#include <png.h>

void MemoryImage::set_pixel_rgba(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  assert(x >= 0 && x < get_width());
  assert(y >= 0 && y < get_height());
  size_t p = get_offset(x, y);
  colors[p] = r;
  colors[p+1] = g;
  colors[p+2] = b;
  colors[p+3] = a;
}

void MemoryImage::get_pixel_rgba(int x, int y, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a){
  assert(x >= 0 && x < get_width());
  assert(y >= 0 && y < get_height());
  size_t p = get_offset(x, y);
  r = this->colors[p];
  g = this->colors[p+1];
  b = this->colors[p+2];
  a = this->colors[p+3];
}

void MemoryImage::blend_pixel(int x, int y, Color &c){
  Color o;
  get_pixel(x, y, o);

  if (o.is_transparent()) {
    set_pixel(x, y, c);
    return;
  }
  
  o.blend(c);
  set_pixel(x, y, o);
}

void Image::get_pixel(int x, int y, Color &c){
  get_pixel_rgba(x, y, c.r, c.g, c.b, c.a);
}

void Image::set_pixel(int x, int y, Color &q){
  set_pixel_rgba(x, y, q.r, q.g, q.b, q.a);
}

void Image::composite(int xoffset, int yoffset, ImageBuffer &img) {
  assert(xoffset >= 0);
  assert(xoffset + img.get_width() <= w);
  assert(yoffset >= 0);
  assert(yoffset + img.get_height() <= h);
  
  Color hp;
  
  for (int x = 0; x < img.get_width(); x++) {
    for (int y = 0; y < img.get_height(); y++) {
      Color base;
      get_pixel(xoffset + x, yoffset + y, base);
      
      if (img.reversed) {
        for (int h = img.get_pixel_depth(x, y); h >= 0; h--) {
          img.get_pixel(x, y, h, hp);
          base.blend(hp);
        }
      } else {
        for (int h = 0; h < img.get_pixel_depth(x, y); h++) {
          Color hp;
          img.get_pixel(x, y, h, hp);
          base.blend(hp);
        }
      }
      
      set_pixel(xoffset + x, yoffset + y, base);
    }
  }
}

bool Image::save_png(const char *filename, const char *title)
{
  bool ret = true;
  
  FILE *fp;
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  png_bytep row = NULL;
  
  fp = fopen(filename, "wb");

  if (fp == NULL) {
     ret = false;
     goto finalise;
  }
  
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
 
  if (png_ptr == NULL) {
     ret = false;
     goto finalise;
  }

  info_ptr = png_create_info_struct(png_ptr);

  if (info_ptr == NULL) {
     ret = false;
     goto finalise;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
     ret = false;
     goto finalise;
  }

  png_init_io(png_ptr, fp);

  png_set_IHDR(png_ptr, info_ptr, get_width(), get_height(),
        8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
   
  if (title != NULL) {
     png_text title_text;
     title_text.compression = PNG_TEXT_COMPRESSION_NONE;
     title_text.key = (char *)"Title";
     title_text.text = (char *)title;
     png_set_text(png_ptr, info_ptr, &title_text, 1);
  }

  png_write_info(png_ptr, info_ptr);

  row = (png_bytep) malloc(4 * get_width() * sizeof(png_byte));

  int x, y;
  
  for (y=0 ; y < get_height(); y++) {
     for (x=0 ; x < get_width(); x++) {
       Color c;
       get_pixel(x, y, c);
       
       if (c.a == 0x0) {
         row[0 + x*4] = 0;
         row[1 + x*4] = 0;
         row[2 + x*4] = 0;
         row[3 + x*4] = 0x00;
       }
       else {
         row[0 + x*4] = c.r;
         row[1 + x*4] = c.g;
         row[2 + x*4] = c.b;
         row[3 + x*4] = 0xff;
       }
     }

     png_write_row(png_ptr, row);
  }
 
  png_write_end(png_ptr, NULL);

finalise:
  if (fp != NULL) {
    fclose(fp);
  }
   
  if (info_ptr != NULL) {
    png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
  }

  if (png_ptr != NULL) {
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
  }

  if (row != NULL) {
    free(row);
  }
  
  return ret;
}


void CachedImage::set_pixel_rgba(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  assert(x >= 0 && x < get_width());
  assert(y >= 0 && y < get_height());
  assert(f != NULL);
  size_t p = get_offset(x, y);
  uint8_t cb[] = {r, g, b, a};
  fseek(f, p, SEEK_SET);
  assert(fwrite(cb, sizeof(cb), 1, f) == 1);
}

void CachedImage::get_pixel_rgba(int x, int y, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a) {
  assert(x >= 0 && x < get_width());
  assert(y >= 0 && y < get_height());
  assert(f != NULL);
  size_t p = get_offset(x, y);
  uint8_t cb[] = {0x00, 0x00, 0x00, 0x00};
  fseek(f, p, SEEK_SET);
  assert(fread(cb, sizeof(cb), 1, f) == 1);
  r = cb[0];
  g = cb[1];
  b = cb[2];
  a = cb[3];
}

void CachedImage::blend_pixel(int x, int y, Color &c){
  size_t s = (x + y * get_width()) % buffer_size;
  
  icache ic = buffer[s];
  
  // cache hit
  if (ic.is_set) {
    // cache hit, but wrong coordinatesflush pizel to file
    if (ic.x != x || ic.y != y)  {
      set_pixel(ic.x, ic.y, ic.c);
      ic.c = c;
      return;
    }
  }
  else {
    ic.c = c;
    ic.is_set = true;
  }
  
  if (ic.c.is_transparent()) {
    return;
  }
  
  ic.c.blend(c);
}

void ImageBuffer::set_pixel_rgba(int x, int y, int z, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  assert(x >= 0 && x < w);
  assert(y >= 0 && y < h);
  assert(z >= 0 && z < d);
  size_t p = (z * COLOR_TYPE) + (y * d * COLOR_TYPE) + (x * d * h * COLOR_TYPE);
  colors[p] = r;
  colors[p+1] = g;
  colors[p+2] = b;
  colors[p+3] = a;
}

void ImageBuffer::set_pixel(int x, int y, int z, Color &q){
  set_pixel_rgba(x, y, z, q.r, q.g, q.b, q.a);
}

void ImageBuffer::get_pixel(int x, int y, int z, Color &c) {
  assert(x >= 0 && x < w);
  assert(y >= 0 && y < h);
  assert(z >= 0 && z < d);
  size_t p = (z * COLOR_TYPE) + (y * d * COLOR_TYPE) + (x * d * h * COLOR_TYPE);
  c.r = colors[p];
  c.g = colors[p+1];
  c.b = colors[p+2];
  c.a = colors[p+3];
}

void ImageBuffer::add_pixel(int x, int y, Color &c) {
  if (c.is_opaque()) {
    set_pixel_depth(x, y, 0);
  }
  
  add_pixel(x, y, c.r, c.g, c.b, c.a);
}

void ImageBuffer::add_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  uint8_t ph = get_pixel_depth(x, y);
  set_pixel_rgba(x, y, ph, r, g, b, a);
  set_pixel_depth(x, y, ph + 1);
}

void ImageBuffer::set_reversed(bool reversed) {
  this->reversed = reversed;
}

void ImageBuffer::set_pixel_depth(int x, int y, uint8_t ph) {
  assert(x >= 0 && x < w);
  assert(y >= 0 && y < h);
  heights[x + (y * w)] = ph;
}

uint8_t ImageBuffer::get_pixel_depth(int x, int y) {
  assert(x >= 0 && x < w);
  assert(y >= 0 && y < h);
  return heights[x + (y * w)];
}
