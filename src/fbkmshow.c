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
 * Animated GIFs are played back frame-by-frame using stb_image's dedicated
 * multi-frame GIF decoder (stbi_load_gif_from_memory), honoring each frame's
 * own delay from the file. Every other format (and single-frame GIFs) still
 * goes through the plain single-image path, unchanged.
 *
 * A long-running `--loops=0` (forever) animation is meant to be stopped with
 * SIGTERM from another process once whatever it was waiting on is ready.
 * SIGTERM is caught and only checked between frames (never mid-render), so
 * the loop always exits on a complete, cleanly-written frame rather than
 * whatever the OS's default abrupt termination would leave behind.
 *
 * Usage: fbkmshow [-h|--help] [--version] [--rotate=0|90|180|270] [--fb=/dev/fb0] [--loops=N] <image-file>
 *
 * Author: Pablo Trujillo <https://github.com/pablotrujillo>
 * License: MIT (see LICENSE)
 */

/* Bump on every user-visible change, per Semantic Versioning — keep in sync
 * with CHANGELOG.md. This is the single source of truth for the binary's own
 * version; release tags/.deb packages should derive from this, not the
 * other way around. */
#define FBKMSHOW_VERSION "1.1.0"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;
static void handle_stop(int sig) { (void)sig; g_stop = 1; }

static void die(const char *msg) {
    fprintf(stderr, "fbkmshow: %s\n", msg);
    exit(1);
}

static void usage(const char *prog, FILE *out) {
    fprintf(out, "Usage: %s [-h|--help] [--version] [--rotate=0|90|180|270] [--fb=/dev/fb0] [--loops=N] <image-file>\n", prog);
    fprintf(out, "  --loops=N   animated GIFs only: play the animation N times (default 1, 0 = forever)\n");
    fprintf(out, "  --version   print the version number and exit\n");
}

/* Scales+centers one RGBA frame onto the framebuffer's logical canvas,
 * converts to the framebuffer's native BGRX8888 layout while placing each
 * pixel at its rotated destination, and writes the result with one write().
 * fbmem/canvas are scratch buffers owned by the caller and reused across
 * frames so an animated GIF doesn't malloc/free per frame. */
static void render_frame(unsigned char *img, int img_w, int img_h,
                          unsigned char *canvas, unsigned char *fbmem,
                          int fbfd, int fb_w, int fb_h, long fb_stride,
                          size_t fb_size, int rotate) {
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

    memset(canvas, 0, (size_t)lw * lh * 4); /* RGBA, black bg */

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
}

int main(int argc, char **argv) {
    const char *fb_path = "/dev/fb0";
    const char *img_path = NULL;
    int rotate = 0;
    int loops = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0], stdout);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("fbkmshow %s\n", FBKMSHOW_VERSION);
            return 0;
        } else if (strncmp(argv[i], "--rotate=", 9) == 0) {
            rotate = atoi(argv[i] + 9);
        } else if (strncmp(argv[i], "--fb=", 5) == 0) {
            fb_path = argv[i] + 5;
        } else if (strncmp(argv[i], "--loops=", 8) == 0) {
            loops = atoi(argv[i] + 8);
        } else {
            img_path = argv[i];
        }
    }
    if (!img_path || loops < 0 || (rotate != 0 && rotate != 90 && rotate != 180 && rotate != 270)) {
        usage(argv[0], stderr);
        return 1;
    }

    signal(SIGTERM, handle_stop);

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

    FILE *f = fopen(img_path, "rb");
    if (!f) die("cannot open image file");
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *file_buf = malloc((size_t)file_size);
    if (!file_buf) die("malloc for image file failed");
    if (file_size > 0 && fread(file_buf, 1, (size_t)file_size, f) != (size_t)file_size) die("short read on image file");
    fclose(f);

    int img_w, img_h, img_channels, frame_count = 1;
    int *delays_ms = NULL;
    unsigned char *frames = stbi_load_gif_from_memory(file_buf, (int)file_size, &delays_ms,
                                                        &img_w, &img_h, &frame_count, &img_channels, 4);
    if (!frames) {
        /* Not an animated GIF (or not a GIF at all) — fall back to the plain
         * single-image decoder, which covers every other format plus
         * single-frame GIFs. */
        frame_count = 1;
        frames = stbi_load_from_memory(file_buf, (int)file_size, &img_w, &img_h, &img_channels, 4);
        if (!frames) {
            fprintf(stderr, "fbkmshow: stbi_load failed: %s\n", stbi_failure_reason());
            return 1;
        }
    }
    free(file_buf);
    fprintf(stderr, "image: %dx%d, %d channels, %d frame(s) (loaded as RGBA)\n",
            img_w, img_h, img_channels, frame_count);

    int swap = (rotate == 90 || rotate == 270);
    unsigned char *canvas = malloc((size_t)(swap ? fb_h : fb_w) * (swap ? fb_w : fb_h) * 4);
    unsigned char *fbmem = malloc(fb_size);
    if (!canvas || !fbmem) die("malloc for scratch buffers failed");

    size_t frame_stride = (size_t)img_w * img_h * 4;
    int pass = 0;
    do {
        for (int i = 0; i < frame_count && !g_stop; i++) {
            render_frame(frames + (size_t)i * frame_stride, img_w, img_h,
                         canvas, fbmem, fbfd, fb_w, fb_h, fb_stride, fb_size, rotate);
            if (frame_count > 1) {
                int delay_ms = delays_ms ? delays_ms[i] : 100;
                if (delay_ms <= 10) delay_ms = 100; /* many GIFs store 0 meaning "use viewer default" */
                usleep((useconds_t)delay_ms * 1000);
            }
        }
        pass++;
    } while (!g_stop && frame_count > 1 && (loops == 0 || pass < loops));

    free(fbmem);
    free(canvas);
    if (delays_ms) stbi_image_free(delays_ms);
    stbi_image_free(frames);
    close(fbfd);
    fprintf(stderr, "fbkmshow: done.\n");
    return 0;
}
