/*
 * fbkmshow.c — minimal framebuffer image viewer using stb_image (public
 * domain, single-header, no shared-library deps once statically linked).
 *
 * Works against any /dev/fbN device: classic legacy fbdev drivers as well as
 * the generic fbdev-emulation layer modern DRM/KMS drivers expose (common on
 * newer SoCs). Decodes JPEG/PNG/BMP/GIF/etc, fits it centered onto the
 * framebuffer's own geometry (queried live via FBIOGET_VSCREENINFO — no
 * hardcoded resolution), optionally rotates it (0/90/180/270 — useful for
 * panels mounted sideways or upside down), converts to the framebuffer's
 * native pixel format, and writes it directly via write(). Some fbdev
 * drivers — notably DRM's fbdev-emulation shim on some SoCs — don't
 * implement mmap, so plain write() is used instead: the same approach `dd`
 * uses to push raw pixels to /dev/fb0.
 *
 * Usage: fbkmshow [-h|--help] [--rotate=0|90|180|270] [--fb=/dev/fb0] <image-file>
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static void die(const char *msg) {
    fprintf(stderr, "fbkmshow: %s\n", msg);
    exit(1);
}

static void usage(const char *prog, FILE *out) {
    fprintf(out, "Usage: %s [-h|--help] [--rotate=0|90|180|270] [--fb=/dev/fb0] <image-file>\n", prog);
}

int main(int argc, char **argv) {
    const char *fb_path = "/dev/fb0";
    const char *img_path = NULL;
    int rotate = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0], stdout);
            return 0;
        } else if (strncmp(argv[i], "--rotate=", 9) == 0) {
            rotate = atoi(argv[i] + 9);
        } else if (strncmp(argv[i], "--fb=", 5) == 0) {
            fb_path = argv[i] + 5;
        } else {
            img_path = argv[i];
        }
    }
    if (!img_path || (rotate != 0 && rotate != 90 && rotate != 180 && rotate != 270)) {
        usage(argv[0], stderr);
        return 1;
    }

    int fbfd = open(fb_path, O_RDWR);
    if (fbfd < 0) die("cannot open framebuffer device");

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) < 0) die("FBIOGET_VSCREENINFO failed");
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) < 0) die("FBIOGET_FSCREENINFO failed");

    int fb_w = vinfo.xres;
    int fb_h = vinfo.yres;
    int fb_bpp = vinfo.bits_per_pixel;
    long fb_stride = finfo.line_length;
    size_t fb_size = (size_t)fb_stride * fb_h;

    fprintf(stderr, "fb: %dx%d, %dbpp, stride=%ld, rotate=%d\n", fb_w, fb_h, fb_bpp, fb_stride, rotate);
    if (fb_bpp != 32) die("only 32bpp framebuffers supported");

    int img_w, img_h, img_channels;
    unsigned char *img = stbi_load(img_path, &img_w, &img_h, &img_channels, 4); /* force RGBA */
    if (!img) {
        fprintf(stderr, "fbkmshow: stbi_load failed: %s\n", stbi_failure_reason());
        return 1;
    }
    fprintf(stderr, "image: %dx%d, %d channels (loaded as RGBA)\n", img_w, img_h, img_channels);

    /* Logical canvas: for 90/270 the content is authored sideways relative to
     * the panel's fixed physical geometry, so width/height swap here and the
     * per-pixel mapping below un-swaps them back into fb_w x fb_h memory. */
    int swap = (rotate == 90 || rotate == 270);
    int lw = swap ? fb_h : fb_w;
    int lh = swap ? fb_w : fb_h;

    double scale = lw / (double)img_w;
    double scale_h = lh / (double)img_h;
    if (scale_h < scale) scale = scale_h;
    int dst_w = (int)(img_w * scale);
    int dst_h = (int)(img_h * scale);
    int off_x = (lw - dst_w) / 2;
    int off_y = (lh - dst_h) / 2;

    unsigned char *canvas = calloc(1, (size_t)lw * lh * 4); /* RGBA, black bg */
    if (!canvas) die("calloc failed");

    for (int y = 0; y < dst_h; y++) {
        int sy = (int)(y / scale);
        if (sy >= img_h) sy = img_h - 1;
        for (int x = 0; x < dst_w; x++) {
            int sx = (int)(x / scale);
            if (sx >= img_w) sx = img_w - 1;
            unsigned char *src = &img[(sy * img_w + sx) * 4];
            int dx = off_x + x, dy = off_y + y;
            if (dx < 0 || dx >= lw || dy < 0 || dy >= lh) continue;
            unsigned char *dst = &canvas[(dy * lw + dx) * 4];
            dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
        }
    }
    stbi_image_free(img);

    unsigned char *fbmem = calloc(1, fb_size);
    if (!fbmem) die("calloc for fb buffer failed");

    /* Map each logical canvas pixel to its final position in fb_w x fb_h
     * memory per rotation, converting RGBA -> native BGRX/XRGB8888 LE order
     * (confirmed on hardware) in the same pass. */
    for (int y = 0; y < lh; y++) {
        for (int x = 0; x < lw; x++) {
            int fx, fy;
            switch (rotate) {
                case 0:   fx = x;              fy = y;              break;
                case 180: fx = fb_w - 1 - x;    fy = fb_h - 1 - y;   break;
                case 90:  fx = fb_w - 1 - y;    fy = x;              break; /* clockwise */
                case 270: fx = y;              fy = fb_h - 1 - x;   break; /* counter-clockwise */
                default:  fx = x;              fy = y;              break;
            }
            if (fx < 0 || fx >= fb_w || fy < 0 || fy >= fb_h) continue;
            unsigned char *src = &canvas[(y * lw + x) * 4];
            unsigned char *dst = fbmem + (size_t)fy * fb_stride + (size_t)fx * 4;
            dst[0] = src[2]; /* B */
            dst[1] = src[1]; /* G */
            dst[2] = src[0]; /* R */
            dst[3] = 0;      /* X */
        }
    }

    lseek(fbfd, 0, SEEK_SET);
    ssize_t written = write(fbfd, fbmem, fb_size);
    if (written < 0 || (size_t)written != fb_size) die("write to framebuffer failed or incomplete");

    free(fbmem);
    free(canvas);
    close(fbfd);
    fprintf(stderr, "fbkmshow: done.\n");
    return 0;
}
