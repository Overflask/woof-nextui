#include "SDL_internal.h"
#ifdef SDL_VIDEO_DRIVER_DUMMY
#include "../SDL_sysvideo.h"
#include "../../SDL_properties_c.h"
#include "SDL_nullframebuffer_c.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#define DUMMY_SURFACE "SDL.internal.window.surface"
static int fb_fd = -1;
static uint8_t *fb_map = NULL;
static size_t fb_map_size = 0;
static struct fb_var_screeninfo fb_vinfo;
static struct fb_fix_screeninfo fb_finfo;

static void fbdev_init(void) {
    if (fb_fd >= 0) return;
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) return;
    ioctl(fb_fd, FBIOBLANK, FB_BLANK_UNBLANK);
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fb_finfo) < 0 ||
        ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_vinfo) < 0) {
        close(fb_fd); fb_fd = -1; return;
    }
    fb_vinfo.yoffset = 0;
    fb_vinfo.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
    ioctl(fb_fd, FBIOPUT_VSCREENINFO, &fb_vinfo);
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_vinfo);
    fb_map_size = fb_finfo.smem_len;
    fb_map = (uint8_t *)mmap(NULL, fb_map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_map == MAP_FAILED) { fb_map = NULL; close(fb_fd); fb_fd = -1; }
}

bool SDL_DUMMY_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, SDL_PixelFormat *format, void **pixels, int *pitch) {
    SDL_Surface *surface;
    const SDL_PixelFormat surface_format = SDL_PIXELFORMAT_XRGB8888;
    int w, h;
    (void)_this;
    fbdev_init();
    SDL_GetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateSurfaceZeroed(w, h, surface_format);
    if (!surface) return false;
    SDL_SetSurfaceProperty(SDL_GetWindowProperties(window), DUMMY_SURFACE, surface);
    *format = surface_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    return true;
}

bool SDL_DUMMY_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects, int numrects) {
    SDL_Surface *surface = (SDL_Surface *)SDL_GetPointerProperty(SDL_GetWindowProperties(window), DUMMY_SURFACE, NULL);
    (void)_this; (void)rects; (void)numrects;
    if (!surface) return SDL_SetError("Couldn't find dummy surface for window");

    if (fb_map && surface->w > 0 && surface->h > 0) {
        uint8_t *dst_base = fb_map + (fb_vinfo.yoffset * fb_finfo.line_length);
        bool portrait = (fb_vinfo.xres < fb_vinfo.yres);
        int sw = surface->w;
        int sh = surface->h;
        int spitch = surface->pitch / 4;
        const uint32_t *src32 = (const uint32_t *)surface->pixels;

        if (portrait) {
            int lw = (int)fb_vinfo.yres;
            int lh = (int)fb_vinfo.xres;
            if (fb_vinfo.bits_per_pixel == 32) {
                int stride = fb_finfo.line_length / 4;
                uint32_t *dst32 = (uint32_t *)dst_base;
                for (int ly = 0; ly < lh; ly++) {
                    int sy = (ly * sh) / lh;
                    const uint32_t *srow = src32 + sy * spitch;
                    for (int lx = 0; lx < lw; lx++) {
                        int sx = (lx * sw) / lw;
                        dst32[(lw - 1 - lx) * stride + ly] = 0xFF000000u | srow[sx];
                    }
                }
            } else if (fb_vinfo.bits_per_pixel == 16) {
                int stride = fb_finfo.line_length / 2;
                uint16_t *dst16 = (uint16_t *)dst_base;
                for (int ly = 0; ly < lh; ly++) {
                    int sy = (ly * sh) / lh;
                    const uint32_t *srow = src32 + sy * spitch;
                    for (int lx = 0; lx < lw; lx++) {
                        int sx = (lx * sw) / lw;
                        uint32_t p = srow[sx];
                        dst16[(lw - 1 - lx) * stride + ly] = (uint16_t)(((p >> 8) & 0xF800) | ((p >> 5) & 0x07E0) | ((p >> 3) & 0x001F));
                    }
                }
            }
        } else {
            int lw = (int)fb_vinfo.xres;
            int lh = (int)fb_vinfo.yres;
            if (fb_vinfo.bits_per_pixel == 32) {
                int stride = fb_finfo.line_length / 4;
                uint32_t *dst32 = (uint32_t *)dst_base;
                for (int ly = 0; ly < lh; ly++) {
                    int sy = (ly * sh) / lh;
                    const uint32_t *srow = src32 + sy * spitch;
                    uint32_t *drow = dst32 + ly * stride;
                    for (int lx = 0; lx < lw; lx++) {
                        drow[lx] = 0xFF000000u | srow[(lx * sw) / lw];
                    }
                }
            } else if (fb_vinfo.bits_per_pixel == 16) {
                int stride = fb_finfo.line_length / 2;
                uint16_t *dst16 = (uint16_t *)dst_base;
                for (int ly = 0; ly < lh; ly++) {
                    int sy = (ly * sh) / lh;
                    const uint32_t *srow = src32 + sy * spitch;
                    uint16_t *drow = dst16 + ly * stride;
                    for (int lx = 0; lx < lw; lx++) {
                        uint32_t p = srow[(lx * sw) / lw];
                        drow[lx] = (uint16_t)(((p >> 8) & 0xF800) | ((p >> 5) & 0x07E0) | ((p >> 3) & 0x001F));
                    }
                }
            }
        }
        ioctl(fb_fd, FBIOPAN_DISPLAY, &fb_vinfo);
    }
    return true;
}

void SDL_DUMMY_DestroyWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window) {
    (void)_this;
    SDL_ClearProperty(SDL_GetWindowProperties(window), DUMMY_SURFACE);
    if (fb_map) { munmap(fb_map, fb_map_size); fb_map = NULL; }
    if (fb_fd >= 0) { close(fb_fd); fb_fd = -1; }
}
#endif
