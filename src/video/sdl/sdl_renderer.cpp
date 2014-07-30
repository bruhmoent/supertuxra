//  SuperTux
//  Copyright (C) 2006 Matthias Braun <matze@braunis.de>
//	Updated by GiBy 2013 for SDL2 <giby_the_kid@yahoo.fr>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "video/sdl/sdl_renderer.hpp"

#include "util/log.hpp"
#include "video/drawing_request.hpp"
#include "video/sdl/sdl_surface_data.hpp"
#include "video/sdl/sdl_texture.hpp"

#include <iomanip>
#include <iostream>
#include <physfs.h>
#include <sstream>
#include <stdexcept>
#include "SDL2/SDL_video.h"

namespace {

SDL_Surface *apply_alpha(SDL_Surface *src, float alpha_factor)
{
  // FIXME: This is really slow
  assert(src->format->Amask);
  int alpha = (int) (alpha_factor * 256);
  SDL_Surface *dst = SDL_CreateRGBSurface(src->flags, src->w, src->h, src->format->BitsPerPixel, src->format->Rmask,  src->format->Gmask, src->format->Bmask, src->format->Amask);
  int bpp = dst->format->BytesPerPixel;
  if(SDL_MUSTLOCK(src))
  {
    SDL_LockSurface(src);
  }
  if(SDL_MUSTLOCK(dst))
  {
    SDL_LockSurface(dst);
  }
  for(int y = 0;y < dst->h;y++) {
    for(int x = 0;x < dst->w;x++) {
      Uint8 *srcpixel = (Uint8 *) src->pixels + y * src->pitch + x * bpp;
      Uint8 *dstpixel = (Uint8 *) dst->pixels + y * dst->pitch + x * bpp;
      Uint32 mapped = 0;
      switch(bpp) {
        case 1:
          mapped = *srcpixel;
          break;
        case 2:
          mapped = *(Uint16 *)srcpixel;
          break;
        case 3:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
          mapped |= srcpixel[0] << 16;
          mapped |= srcpixel[1] << 8;
          mapped |= srcpixel[2] << 0;
#else
          mapped |= srcpixel[0] << 0;
          mapped |= srcpixel[1] << 8;
          mapped |= srcpixel[2] << 16;
#endif
          break;
        case 4:
          mapped = *(Uint32 *)srcpixel;
          break;
      }
      Uint8 r, g, b, a;
      SDL_GetRGBA(mapped, src->format, &r, &g, &b, &a);
      mapped = SDL_MapRGBA(dst->format, r, g, b, (a * alpha) >> 8);
      switch(bpp) {
        case 1:
          *dstpixel = mapped;
          break;
        case 2:
          *(Uint16 *)dstpixel = mapped;
          break;
        case 3:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
          dstpixel[0] = (mapped >> 16) & 0xff;
          dstpixel[1] = (mapped >> 8) & 0xff;
          dstpixel[2] = (mapped >> 0) & 0xff;
#else
          dstpixel[0] = (mapped >> 0) & 0xff;
          dstpixel[1] = (mapped >> 8) & 0xff;
          dstpixel[2] = (mapped >> 16) & 0xff;
#endif
          break;
        case 4:
          *(Uint32 *)dstpixel = mapped;
          break;
      }
    }
  }
  if(SDL_MUSTLOCK(dst))
  {
    SDL_UnlockSurface(dst);
  }
  if(SDL_MUSTLOCK(src))
  {
    SDL_UnlockSurface(src);
  }
  return dst;
}

} // namespace

SDLRenderer::SDLRenderer() :
  window(),
  renderer(),
  numerator(),
  denominator()
{
  Renderer::instance_ = this;

  // Cannot currently find a way to do this with SDL2
  //const SDL_VideoInfo *info = SDL_GetVideoInfo();
  //log_info << "Hardware surfaces are " << (info->hw_available ? "" : "not ") << "available." << std::endl;
  //log_info << "Hardware to hardware blits are " << (info->blit_hw ? "" : "not ") << "accelerated." << std::endl;
  //log_info << "Hardware to hardware blits with colorkey are " << (info->blit_hw_CC ? "" : "not ") << "accelerated." << std::endl;
  //log_info << "Hardware to hardware blits with alpha are " << (info->blit_hw_A ? "" : "not ") << "accelerated." << std::endl;
  //log_info << "Software to hardware blits are " << (info->blit_sw ? "" : "not ") << "accelerated." << std::endl;
  //log_info << "Software to hardware blits with colorkey are " << (info->blit_sw_CC ? "" : "not ") << "accelerated." << std::endl;
  //log_info << "Software to hardware blits with alpha are " << (info->blit_sw_A ? "" : "not ") << "accelerated." << std::endl;
  //log_info << "Color fills are " << (info->blit_fill ? "" : "not ") << "accelerated." << std::endl;

 // int flags = SDL_SWSURFACE | SDL_ANYFORMAT;
 // if(g_config->use_fullscreen)
 //   flags |= SDL_FULLSCREEN;
    
  log_info << "creating SDLRenderer" << std::endl;
  int width  = 800; //FIXME: config->screenwidth;
  int height = 600; //FIXME: config->screenheight;
  int flags = 0;
  int ret = SDL_CreateWindowAndRenderer(width, height, flags,
                                        &window, &renderer);

  if(ret != 0) {
    std::stringstream msg;
    msg << "Couldn't set video mode (" << width << "x" << height
        << "): " << SDL_GetError();
    throw std::runtime_error(msg.str());
  }
  SDL_SetWindowTitle(window, "SuperTux");
  if(texture_manager == 0)
    texture_manager = new TextureManager();

#ifdef OLD_SDL1
  numerator   = 1;
  denominator = 1;
  /* FIXME: 
     float xfactor = (float) config->screenwidth / SCREEN_WIDTH;
     float yfactor = (float) config->screenheight / SCREEN_HEIGHT;
     if(xfactor < yfactor)
     {
     numerator = config->screenwidth;
     denominator = SCREEN_WIDTH;
     }
     else
     {
     numerator = config->screenheight;
     denominator = SCREEN_HEIGHT;
     }
  */
#endif
}

SDLRenderer::~SDLRenderer()
{
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
}

void
SDLRenderer::draw_surface(const DrawingRequest& request)
{
  //FIXME: support parameters request.alpha, request.angle, request.blend
  const Surface* surface = (const Surface*) request.request_data;
  boost::shared_ptr<SDLTexture> sdltexture = boost::dynamic_pointer_cast<SDLTexture>(surface->get_texture());

  SDL_Rect dst_rect;
  dst_rect.x = request.pos.x;
  dst_rect.y = request.pos.y;
  dst_rect.w = sdltexture->get_image_width();
  dst_rect.h = sdltexture->get_image_height();

#ifdef OLD_SDL1
  // FIXME: Use SDL_RenderCopyEx() to handle flipping
#endif

  SDL_RenderCopy(renderer, sdltexture->get_texture(), NULL, &dst_rect);

#ifdef OLD_SDL1
  SDLSurfaceData *surface_data = reinterpret_cast<SDLSurfaceData *>(surface->get_surface_data());

  DrawingEffect effect = request.drawing_effect;
  if (surface->get_flipx()) effect = HORIZONTAL_FLIP;

  SDL_Surface *transform = sdltexture->get_transform(request.color, effect);

  // get and check SDL_Surface
  if (transform == 0) {
    std::cerr << "Warning: Tried to draw NULL surface, skipped draw" << std::endl;
    return;
  }

  SDL_Rect *src_rect = surface_data->get_src_rect(effect);
  SDL_Rect dst_rect;
  dst_rect.x = (int) request.pos.x * numerator / denominator;
  dst_rect.y = (int) request.pos.y * numerator / denominator;

  Uint8 alpha = 0;
  if(request.alpha != 1.0)
  {
    if(!transform->format->Amask)
    {
      if(transform->flags & SDL_SRCALPHA)
      {
        alpha = transform->format->alpha;
      }
      else
      {
        alpha = 255;
      }
      SDL_SetSurfaceAlphaMod(transform, (Uint8) (request.alpha * alpha));
    }
    /*else
      {
      transform = apply_alpha(transform, request.alpha);
      }*/
  }

  SDL_BlitSurface(transform, src_rect, screen, &dst_rect);

  if(request.alpha != 1.0)
  {
    if(!transform->format->Amask)
    {
      if(alpha == 255)
      {
        SDL_SetSurfaceAlphaMod(transform, 0);
      }
      else
      {
        SDL_SetSurfaceAlphaMod(transform, alpha);
      }
    }
    /*else
      {
      SDL_FreeSurface(transform);
      }*/
  }
#endif
}

void
SDLRenderer::draw_surface_part(const DrawingRequest& request)
{
  //FIXME: support parameters request.alpha, request.angle, request.blend
  const SurfacePartRequest* surface = (const SurfacePartRequest*) request.request_data;
  const SurfacePartRequest* surfacepartrequest = (SurfacePartRequest*) request.request_data;

  boost::shared_ptr<SDLTexture> sdltexture = boost::dynamic_pointer_cast<SDLTexture>(surface->surface->get_texture());

  SDL_Rect src_rect;
  src_rect.x = surfacepartrequest->source.x;
  src_rect.y = surfacepartrequest->source.y;
  src_rect.w = surfacepartrequest->size.x;
  src_rect.h = surfacepartrequest->size.y;

  SDL_Rect dst_rect;
  dst_rect.x = request.pos.x;
  dst_rect.y = request.pos.y;
  dst_rect.w = surfacepartrequest->size.x;
  dst_rect.h = surfacepartrequest->size.y;

#ifdef OLD_SDL1
  // FIXME: Use SDL_RenderCopyEx() to handle flipping
#endif

  SDL_RenderCopy(renderer, sdltexture->get_texture(),
                 &src_rect, &dst_rect);

#ifdef OLD_SDL1
  const SurfacePartRequest* surfacepartrequest
    = (SurfacePartRequest*) request.request_data;

  const Surface* surface = surfacepartrequest->surface;
  boost::shared_ptr<SDLTexture> sdltexture = boost::dynamic_pointer_cast<SDLTexture>(surface->get_texture());

  DrawingEffect effect = request.drawing_effect;
  if (surface->get_flipx()) effect = HORIZONTAL_FLIP;

  SDL_Surface *transform = sdltexture->get_transform(request.color, effect);

  // get and check SDL_Surface
  if (transform == 0) {
    std::cerr << "Warning: Tried to draw NULL surface, skipped draw" << std::endl;
    return;
  }

  int ox, oy;
  if (effect == HORIZONTAL_FLIP)
  {
    ox = sdltexture->get_texture_width() - surface->get_x() - (int) surfacepartrequest->size.x;
  }
  else
  {
    ox = surface->get_x();
  }
  if (effect == VERTICAL_FLIP)
  {
    oy = sdltexture->get_texture_height() - surface->get_y() - (int) surfacepartrequest->size.y;
  }
  else
  {
    oy = surface->get_y();
  }

  SDL_Rect src_rect;
  src_rect.x = (ox + (int) surfacepartrequest->source.x) * numerator / denominator;
  src_rect.y = (oy + (int) surfacepartrequest->source.y) * numerator / denominator;
  src_rect.w = (int) surfacepartrequest->size.x * numerator / denominator;
  src_rect.h = (int) surfacepartrequest->size.y * numerator / denominator;

  SDL_Rect dst_rect;
  dst_rect.x = (int) request.pos.x * numerator / denominator;
  dst_rect.y = (int) request.pos.y * numerator / denominator;

  Uint8 alpha = 0;
  if(request.alpha != 1.0)
  {
    if(!transform->format->Amask)
    {
      if(transform->flags & SDL_SRCALPHA)
      {
        alpha = transform->format->alpha;
      }
      else
      {
        alpha = 255;
      }
      SDL_SetSurfaceAlphaMod(transform, (Uint8) (request.alpha * alpha));
    }
    /*else
      {
      transform = apply_alpha(transform, request.alpha);
      }*/
  }
#endif

#ifdef OLD_SDL1
  SDL_BlitSurface(transform, &src_rect, screen, &dst_rect);
#endif

#ifdef OLD_SDL1
  if(request.alpha != 1.0)
  {
    if(!transform->format->Amask)
    {
      if(alpha == 255)
      {
        SDL_SetSurfaceAlphaMod(transform, 0);
      }
      else
      {
        SDL_SetSurfaceAlphaMod(transform, alpha);
      }
    }
    /*else
      {
      SDL_FreeSurface(transform);
      }*/
  }
#endif
}

void
SDLRenderer::draw_gradient(const DrawingRequest& request)
{
  const GradientRequest* gradientrequest 
    = (GradientRequest*) request.request_data;
  const Color& top = gradientrequest->top;
  const Color& bottom = gradientrequest->bottom;

  int w;
  int h;
  SDL_GetWindowSize(window, &w, &h);

  // calculate the maximum number of steps needed for the gradient
  int n = static_cast<int>(std::max(std::max(fabsf(top.red - bottom.red),
                                             fabsf(top.green - bottom.green)),
                                    std::max(fabsf(top.blue - bottom.blue),
                                             fabsf(top.alpha - bottom.alpha))) * 255);
  for(int i = 0; i < n; ++i)
  {
    SDL_Rect rect;
    rect.x = 0;
    rect.y = h * i / n;
    rect.w = w;
    rect.h = (h * (i+1) / n) - rect.y;

    float p = static_cast<float>(i+1) / static_cast<float>(n);
    Uint8 r = static_cast<Uint8>(((1.0f - p) * top.red + p * bottom.red)  * 255);
    Uint8 g = static_cast<Uint8>(((1.0f - p) * top.green + p * bottom.green) * 255);
    Uint8 b = static_cast<Uint8>(((1.0f - p) * top.blue + p * bottom.blue) * 255);
    Uint8 a = static_cast<Uint8>(((1.0f - p) * top.alpha + p * bottom.alpha) * 255);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderFillRect(renderer, &rect);
  }

#ifdef OLD_SDL1
  const GradientRequest* gradientrequest 
    = (GradientRequest*) request.request_data;
  const Color& top = gradientrequest->top;
  const Color& bottom = gradientrequest->bottom;

  for(int y = 0;y < screen->h;++y)
  {
    Uint8 r = (Uint8)((((float)(top.red-bottom.red)/(0-screen->h)) * y + top.red) * 255);
    Uint8 g = (Uint8)((((float)(top.green-bottom.green)/(0-screen->h)) * y + top.green) * 255);
    Uint8 b = (Uint8)((((float)(top.blue-bottom.blue)/(0-screen->h)) * y + top.blue) * 255);
    Uint8 a = (Uint8)((((float)(top.alpha-bottom.alpha)/(0-screen->h)) * y + top.alpha) * 255);
    Uint32 color = SDL_MapRGB(screen->format, r, g, b);

    SDL_Rect rect;
    rect.x = 0;
    rect.y = y;
    rect.w = screen->w;
    rect.h = 1;

    if(a == SDL_ALPHA_OPAQUE) {
      SDL_FillRect(screen, &rect, color);
    } else if(a != SDL_ALPHA_TRANSPARENT) {
      SDL_Surface *temp = SDL_CreateRGBSurface(screen->flags, rect.w, rect.h, screen->format->BitsPerPixel, screen->format->Rmask, screen->format->Gmask, screen->format->Bmask, screen->format->Amask);

      SDL_FillRect(temp, 0, color);
      SDL_SetSurfaceAlphaMod(temp, a);
      SDL_BlitSurface(temp, 0, screen, &rect);
      SDL_FreeSurface(temp);
    }
  }
#endif
}

void
SDLRenderer::draw_filled_rect(const DrawingRequest& request)
{
  const FillRectRequest* fillrectrequest
    = (FillRectRequest*) request.request_data;

  SDL_Rect rect;
  rect.x = request.pos.x;
  rect.y = request.pos.y;
  rect.w = fillrectrequest->size.x;
  rect.h = fillrectrequest->size.y;

  if((rect.w != 0) && (rect.h != 0)) 
  {
    Uint8 r = static_cast<Uint8>(fillrectrequest->color.red * 255);
    Uint8 g = static_cast<Uint8>(fillrectrequest->color.green * 255);
    Uint8 b = static_cast<Uint8>(fillrectrequest->color.blue * 255);
    Uint8 a = static_cast<Uint8>(fillrectrequest->color.alpha * 255);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderFillRect(renderer, &rect);
  }

#ifdef OLD_SDL1
  const FillRectRequest* fillrectrequest
    = (FillRectRequest*) request.request_data;

  SDL_Rect rect;
  rect.x = (Sint16)request.pos.x * screen->w / SCREEN_WIDTH;
  rect.y = (Sint16)request.pos.y * screen->h / SCREEN_HEIGHT;
  rect.w = (Uint16)fillrectrequest->size.x * screen->w / SCREEN_WIDTH;
  rect.h = (Uint16)fillrectrequest->size.y * screen->h / SCREEN_HEIGHT;
  if((rect.w == 0) || (rect.h == 0)) {
    return;
  }
  Uint8 r = static_cast<Uint8>(fillrectrequest->color.red * 255);
  Uint8 g = static_cast<Uint8>(fillrectrequest->color.green * 255);
  Uint8 b = static_cast<Uint8>(fillrectrequest->color.blue * 255);
  Uint8 a = static_cast<Uint8>(fillrectrequest->color.alpha * 255);
  Uint32 color = SDL_MapRGB(screen->format, r, g, b);
  if(a == SDL_ALPHA_OPAQUE) {
    SDL_FillRect(screen, &rect, color);
  } else if(a != SDL_ALPHA_TRANSPARENT) {
    SDL_Surface *temp = SDL_CreateRGBSurface(screen->flags, rect.w, rect.h, screen->format->BitsPerPixel, screen->format->Rmask, screen->format->Gmask, screen->format->Bmask, screen->format->Amask);

    SDL_FillRect(temp, 0, color);
    SDL_SetSurfaceAlphaMod(temp, a);
    SDL_BlitSurface(temp, 0, screen, &rect);
    SDL_FreeSurface(temp);
  }
#endif
}

void
SDLRenderer::draw_inverse_ellipse(const DrawingRequest& request)
{
  const InverseEllipseRequest* ellipse = (InverseEllipseRequest*)request.request_data;

  int window_w;
  int window_h;
  SDL_GetWindowSize(window, &window_w, &window_h);

  float x = request.pos.x;
  float w = ellipse->size.x;
  float h = ellipse->size.y;

  int top = request.pos.y - (h / 2);

  const int max_slices = 256;
  SDL_Rect rects[2*max_slices+2];
  int slices = std::min(static_cast<int>(ellipse->size.y), max_slices);
  for(int i = 0; i < slices; ++i)
  {
    float p = ((static_cast<float>(i) + 0.5f) / static_cast<float>(slices)) * 2.0f - 1.0f; 
    int xoff = static_cast<int>(sqrtf(1.0f - p*p) * w / 2);

    SDL_Rect& left  = rects[2*i+0];
    SDL_Rect& right = rects[2*i+1];

    left.x = 0;
    left.y = top + (i * h / slices);
    left.w = x - xoff;
    left.h = (top + ((i+1) * h / slices)) - left.y;

    right.x = x + xoff;
    right.y = left.y;
    right.w = window_w - right.x;
    right.h = left.h;
  }

  SDL_Rect& top_rect = rects[2*slices+0];
  SDL_Rect& bottom_rect = rects[2*slices+1];

  top_rect.x = 0;
  top_rect.y = 0;
  top_rect.w = window_w;
  top_rect.h = top;

  bottom_rect.x = 0;
  bottom_rect.y = top + h;
  bottom_rect.w = window_w;
  bottom_rect.h = window_h - bottom_rect.y;

  Uint8 r = static_cast<Uint8>(ellipse->color.red * 255);
  Uint8 g = static_cast<Uint8>(ellipse->color.green * 255);
  Uint8 b = static_cast<Uint8>(ellipse->color.blue * 255);
  Uint8 a = static_cast<Uint8>(ellipse->color.alpha * 255);

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, r, g, b, a);
  SDL_RenderFillRects(renderer, rects, 2*slices+2);
}

void 
SDLRenderer::do_take_screenshot()
{
  // [Christoph] TODO: Yes, this method also takes care of the actual disk I/O. Split it?

  SDL_Surface *screen = SDL_GetWindowSurface(window);

  // save screenshot
  static const std::string writeDir = PHYSFS_getWriteDir();
  static const std::string dirSep = PHYSFS_getDirSeparator();
  static const std::string baseName = "screenshot";
  static const std::string fileExt = ".bmp";
  std::string fullFilename;
  for (int num = 0; num < 1000; num++) {
    std::ostringstream oss;
    oss << baseName;
    oss << std::setw(3) << std::setfill('0') << num;
    oss << fileExt;
    std::string fileName = oss.str();
    fullFilename = writeDir + dirSep + fileName;
    if (!PHYSFS_exists(fileName.c_str())) {
      SDL_SaveBMP(screen, fullFilename.c_str());
      log_debug << "Wrote screenshot to \"" << fullFilename << "\"" << std::endl;
      return;
    }
  }
  log_warning << "Did not save screenshot, because all files up to \"" << fullFilename << "\" already existed" << std::endl;
}

void
SDLRenderer::flip()
{
  SDL_RenderPresent(renderer);
}

void
SDLRenderer::resize(int, int)
{
    
}

/* EOF */
