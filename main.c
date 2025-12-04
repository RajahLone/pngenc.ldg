
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ldg.h>
#include <png.h>

#define STRINGIFY(x) #x
#define VERSION_LIB(A,B,C) STRINGIFY(A) "." STRINGIFY(B) "." STRINGIFY(C)
#define VERSION_LDG(A,B,C) "APNG encoder from the PNGLIB (" STRINGIFY(A) "." STRINGIFY(B) "." STRINGIFY(C) ")"

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define PNG_ERROR 0
#define PNG_OK 1

/* structures */

typedef struct png_mem_file {
  uint8_t *data;
  int size;
  int offset;
} png_mem_file;

/* global variables */

static png_mem_file png_mf;

static png_structp png_ptr;
static png_infop   info_ptr;
static png_uint_32 frame_count;
static png_uint_32 frame_idx;
static png_uint_32 image_height;
static png_uint_32 image_rowbytes;

/* internal functions */

static void pngldg_write(png_structp png_ptr, png_bytep data, png_size_t count)
{
  png_mem_file *mf = (png_mem_file *) png_get_io_ptr(png_ptr);
    
  uint32_t new_size;
   
  if (mf->offset + count > mf->size)
  {
    new_size = 2 * mf->size;
      
    if (mf->offset + count > new_size) { new_size = (((mf->offset + count + 15) >> 4) << 4); }
      
    mf->data = realloc(mf->data, new_size);
      
    if (mf->data == NULL) { return; }
      
    mf->size = new_size;
  }
    
  memcpy(mf->data + mf->offset, data, count);
  mf->offset += count;
}
void pngldg_flush(png_structp png_ptr) { }

void *pngldg_malloc(png_struct *png_ptr, png_alloc_size_t size) { return ldg_Malloc(size); }
void pngldg_free(png_struct *png_ptr, void *ptr) { ldg_Free(ptr); }

/* functions */

const char * CDECL pngenc_get_lib_version() { return VERSION_LIB(PNG_LIBPNG_VER_MAJOR, PNG_LIBPNG_VER_MINOR, PNG_LIBPNG_VER_RELEASE); }

int32_t CDECL pngenc_open(uint32_t width, uint32_t height, uint32_t depth, uint32_t colortype, uint32_t frames_count, uint32_t plays_count)
{
  int size = (width * height) + 1024 + 7; size &= ~7;
  
  png_mf.data = malloc(size);
  png_mf.size = size;
  png_mf.offset = 0;

  if (png_mf.data == NULL) { return PNG_ERROR; }

  png_ptr  = png_create_write_struct_2(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL, NULL, pngldg_malloc, pngldg_free);
  info_ptr = png_create_info_struct(png_ptr);
  
  frame_count = frames_count;
  frame_idx = 0;
  image_height = height;

  if (png_ptr && info_ptr)
  {
    if (setjmp(png_jmpbuf(png_ptr))) { png_destroy_write_struct(&png_ptr, &info_ptr); return PNG_ERROR; }
    
    png_set_write_fn(png_ptr, &png_mf, pngldg_write, pngldg_flush);
    png_set_compression_level(png_ptr, 9);
    png_set_IHDR(png_ptr, info_ptr, width, height, depth, colortype, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    
    if (frames_count > 1) { png_set_acTL(png_ptr, info_ptr, frames_count, plays_count); }
    
    png_write_info(png_ptr, info_ptr);
    
    image_rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    return PNG_OK;
  }

  return PNG_ERROR;
}

int32_t CDECL pngenc_add_image(uint32_t left, uint32_t top, uint32_t width, uint32_t height, uint32_t delay_num, uint32_t delay_den, uint32_t dispose_op, uint32_t blend_op, uint8_t *p_frame)
{
  if (png_ptr && info_ptr)
  {
    png_bytepp rows = (png_bytepp)malloc(image_height * sizeof(png_bytep));

    if (p_frame && rows)
    {
      unsigned int j;
      
      for (j = 0; j < image_height; j++) { rows[j] = p_frame + (j * image_rowbytes); }

      if (frame_count == 1)
      {
        png_write_image(png_ptr, rows);
      }
      else
      {
        png_write_frame_head(png_ptr, info_ptr, NULL, width, height, left, top, delay_num, delay_den, dispose_op, blend_op);
        png_write_image(png_ptr, rows);
        png_write_frame_tail(png_ptr, info_ptr);
      }
  
      frame_idx++;
      free(rows);
    }

    return PNG_OK;
  }
  return PNG_ERROR;
}

int32_t CDECL pngenc_write() { if (png_ptr && info_ptr) { png_write_end(png_ptr, info_ptr); return PNG_OK; } return PNG_ERROR; }

uint8_t* CDECL pngenc_get_filedata() { return png_mf.data; }
uint32_t CDECL pngenc_get_filesize() { return png_mf.offset; }

int32_t CDECL pngenc_close()
{
  png_destroy_write_struct(&png_ptr, &info_ptr);

  free(png_mf.data);

  png_mf.data = NULL;
  png_mf.size = 0;
  png_mf.offset = 0;

  frame_count = 0;
  frame_idx = 0;
  image_height = 0;
  image_rowbytes = 0;

  return PNG_OK;
}

/* populate functions list and info for the LDG */

PROC LibFunc[] =
{
  {"pngenc_get_lib_version", "const char* pngenc_get_lib_version();\n", pngenc_get_lib_version},
   
  {"pngenc_open", "int32_t pngenc_open(uint32_t width, uint32_t height, uint32_t depth, uint32_t colortype, uint32_t frames_count, uint32_t plays_count);\n", pngenc_open},
  
  {"pngenc_add_image", "int32_t pngenc_add_image(uint32_t left, uint32_t top, uint32_t width, uint32_t height, uint32_t delay_num, uint32_t delay_den, uint32_t dispose_op, uint32_t blend_op, const uint8_t *p_frame);\n", pngenc_add_image},
  
  {"pngenc_write", "uint32_t pngenc_write();\n", pngenc_write},
  {"pngenc_get_filedata", "uint8_t* pngenc_get_filedata();\n", pngenc_get_filedata},
  {"pngenc_get_filesize", "uint32_t pngenc_get_filesize();\n", pngenc_get_filesize},
  {"pngenc_close", "int32_t pngenc_close();\n", pngenc_close},
};

LDGLIB LibLdg[] = { { 0x0001, 7, LibFunc, VERSION_LDG(PNG_LIBPNG_VER_MAJOR, PNG_LIBPNG_VER_MINOR, PNG_LIBPNG_VER_RELEASE), 1} };

/*  */

int main(void)
{
  ldg_init(LibLdg);
  return 0;
}
