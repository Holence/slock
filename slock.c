/* See LICENSE file for license details. */
#define _XOPEN_SOURCE 500
#if HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <X11/extensions/Xrandr.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#include "arg.h"
#include "util.h"

char *argv0;

#include "config.h"
#define NUM_BRICKS            LENGTH(bricks_pos)
#define NUM_MICROBRICKS       (NUM_BRICKS * MICROBRICKS_PER_BRICK * MICROBRICKS_PER_BRICK)
#define NUM_DIRT_COLORS       LENGTH(DIRT_COLORS)
#define NUM_BRICK_COLORS      LENGTH(BRICK_COLORS)
#define NUM_GRASS_COLORS      LENGTH(GRASS_COLORS)
#define PIXELS_PER_MICROBRICK (PIXELS_PER_BRICK / MICROBRICKS_PER_BRICK)
#define PIXELS_PER_DIRT       (PIXELS_PER_MICROBRICK / DIRTS_PER_MICROBRICK)

#define LENGTH(X) (sizeof X / sizeof X[0])
#define MAX(i, j) (((i) > (j)) ? (i) : (j))
#define MIN(i, j) (((i) < (j)) ? (i) : (j))

struct lock {
    int screen;
    Window root, win;
    Pixmap pmap;
    unsigned int x, y;
    unsigned int xoff, yoff, mw, mh;
    Drawable drawable;
    GC gc;
    short bricks_pos[NUM_MICROBRICKS][2];
};

struct xrandr {
    int active;
    int evbase;
    int errbase;
};

static void
die(const char *errstr, ...) {
    va_list ap;

    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(1);
}

#ifdef __linux__
#include <fcntl.h>
#include <linux/oom.h>

static void
dontkillme(void) {
    FILE *f;
    const char oomfile[] = "/proc/self/oom_score_adj";

    if (!(f = fopen(oomfile, "w"))) {
        if (errno == ENOENT)
            return;
        die("slock: fopen %s: %s\n", oomfile, strerror(errno));
    }
    fprintf(f, "%d", OOM_SCORE_ADJ_MIN);
    if (fclose(f)) {
        if (errno == EACCES)
            die("slock: unable to disable OOM killer. "
                "Make sure to suid or sgid slock.\n");
        else
            die("slock: fclose %s: %s\n", oomfile, strerror(errno));
    }
}
#endif

static const char *
gethash(void) {
    const char *hash;
    struct passwd *pw;

    /* Check if the current user has a password entry */
    errno = 0;
    if (!(pw = getpwuid(getuid()))) {
        if (errno)
            die("slock: getpwuid: %s\n", strerror(errno));
        else
            die("slock: cannot retrieve password entry\n");
    }
    hash = pw->pw_passwd;

#if HAVE_SHADOW_H
    if (!strcmp(hash, "x")) {
        struct spwd *sp;
        if (!(sp = getspnam(pw->pw_name)))
            die("slock: getspnam: cannot retrieve shadow entry. "
                "Make sure to suid or sgid slock.\n");
        hash = sp->sp_pwdp;
    }
#else
    if (!strcmp(hash, "*")) {
#ifdef __OpenBSD__
        if (!(pw = getpwuid_shadow(getuid())))
            die("slock: getpwnam_shadow: cannot retrieve shadow entry. "
                "Make sure to suid or sgid slock.\n");
        hash = pw->pw_passwd;
#else
        die("slock: getpwuid: cannot retrieve shadow entry. "
            "Make sure to suid or sgid slock.\n");
#endif /* __OpenBSD__ */
    }
#endif /* HAVE_SHADOW_H */

    return hash;
}

static short brick_y_max;
static short brick_y_min;
static short brick_y_span;
static void
caclulate_bricks(struct lock *lock) {
    brick_y_max = 0;
    brick_y_min = 0x7fff;

    size_t index = 0;
    for (size_t n = 0; n < NUM_BRICKS; n++) {
        short x = (bricks_pos[n][0] * PIXELS_PER_BRICK) + lock->xoff + ((lock->mw) / 2) - (LOGO_W / 2 * PIXELS_PER_BRICK);
        short y = (bricks_pos[n][1] * PIXELS_PER_BRICK) + lock->yoff + ((lock->mh) / 2) - (LOGO_H / 2 * PIXELS_PER_BRICK);

        brick_y_min = y < brick_y_min ? y : brick_y_min;
        short microbrick_y = y;
        for (size_t i = 0; i < MICROBRICKS_PER_BRICK; i++) {
            short microbrick_x = x;
            for (size_t j = 0; j < MICROBRICKS_PER_BRICK; j++) {
                lock->bricks_pos[index][0] = microbrick_x;
                lock->bricks_pos[index][1] = microbrick_y;
                microbrick_x += PIXELS_PER_MICROBRICK;
                index++;
            }
            microbrick_y += PIXELS_PER_MICROBRICK;
            brick_y_max = microbrick_y > brick_y_max ? microbrick_y : brick_y_max;
        }
    }
    brick_y_span = brick_y_max - brick_y_min;
}

static unsigned long
dim_color(unsigned long hex_color, double percent) {
    if (percent < 1) {
        unsigned char r = (hex_color >> 16) & 0xFF;
        unsigned char g = (hex_color >> 8) & 0xFF;
        unsigned char b = hex_color & 0xFF;

        r = (double)r * percent;
        g = (double)g * percent;
        b = (double)b * percent;

        return (r << 16) | (g << 8) | b;
    } else {
        return hex_color;
    }
}

static int
rand_comparison(const void *a, const void *b) {
    return rand() % 2 ? +1 : -1;
}

static void
shuffle(void *base, size_t nmemb, size_t size) {
    qsort(base, nmemb, size, rand_comparison);
}

static struct timeval tick;
static unsigned int logo_seed;
static int logo_blocks_left = NUM_MICROBRICKS;

static void
reset_logo(struct lock **locks, int nscreens) {
    gettimeofday(&tick, NULL);
    logo_seed = tick.tv_sec * 1000000 + tick.tv_usec;
    srand(logo_seed);
    logo_blocks_left = NUM_MICROBRICKS;
    for (size_t screen = 0; screen < nscreens; screen++) {
        shuffle(locks[screen]->bricks_pos, NUM_MICROBRICKS, sizeof(short) * 2);
    }
}

static unsigned long
generate_backgroung_pixle_color(unsigned int height, unsigned int y) {
    // top reserved for grass
    y += PIXELS_PER_DIRT;

    unsigned int grass_depth = ((double)PIXELS_PER_BRICK * 2 / 3);
    unsigned long color;
    if (y < grass_depth && (y <= PIXELS_PER_DIRT || rand() % (int)((double)y / PIXELS_PER_DIRT * 100 * 2 / 3) < 100)) {
        // grass
        color = GRASS_COLORS[rand() % NUM_GRASS_COLORS];
    } else {
        color = DIRT_COLORS[rand() % NUM_DIRT_COLORS];
    }

    return dim_color(color, exp(-(double)y / height * 2.75));
}

static void
drawbackground(Display *dpy, struct lock **locks, int nscreens) {
    gettimeofday(&tick, NULL);
    srand(tick.tv_sec * 1000000 + tick.tv_usec);
    for (int screen = 0; screen < nscreens; screen++) {
        unsigned short width = locks[screen]->x;
        unsigned short height = locks[screen]->y;
        for (unsigned int y = 0; y < height; y += PIXELS_PER_DIRT) {
            for (unsigned int x = 0; x < width; x += PIXELS_PER_DIRT) {
                XSetForeground(dpy, locks[screen]->gc, generate_backgroung_pixle_color(height, y));
                XFillRectangle(dpy, locks[screen]->drawable, locks[screen]->gc, x, y, PIXELS_PER_DIRT, PIXELS_PER_DIRT);
            }
        }
        // store background in locks[screen]->pmap
        locks[screen]->pmap = XCreatePixmap(dpy, locks[screen]->root, locks[screen]->x, locks[screen]->y, DefaultDepth(dpy, screen));
        XCopyArea(dpy, locks[screen]->drawable, locks[screen]->pmap, locks[screen]->gc, 0, 0, locks[screen]->x, locks[screen]->y, 0, 0);
    }
}

static void
drawlogo(Display *dpy, struct lock **locks, int nscreens) {
    for (int screen = 0; screen < nscreens; screen++) {
        // restore background
        XCopyArea(dpy, locks[screen]->pmap, locks[screen]->drawable, locks[screen]->gc, 0, 0, locks[screen]->x, locks[screen]->y, 0, 0);

        srand(logo_seed);
        for (size_t i = 0; i < logo_blocks_left; i++) {
            short y = locks[screen]->bricks_pos[i][1];
            unsigned long dimmed_color = dim_color(BRICK_COLORS[rand() % NUM_BRICK_COLORS], exp(-(double)(y - brick_y_min) / (brick_y_span * 1.25)) + 0.125);
            XSetForeground(dpy, locks[screen]->gc, dimmed_color);
            XFillRectangle(dpy, locks[screen]->drawable, locks[screen]->gc, locks[screen]->bricks_pos[i][0], locks[screen]->bricks_pos[i][1], PIXELS_PER_MICROBRICK, PIXELS_PER_MICROBRICK);
        }
        XCopyArea(dpy, locks[screen]->drawable, locks[screen]->win, locks[screen]->gc, 0, 0, locks[screen]->x, locks[screen]->y, 0, 0);
        XSync(dpy, False);
    }
}

static void
readpw(Display *dpy, struct xrandr *rr, struct lock **locks, int nscreens,
       const char *hash) {
    XRRScreenChangeNotifyEvent *rre;
    char buf[32], passwd[256], *inputhash;
    int num, screen, running;
    unsigned int len;
    KeySym ksym;
    XEvent ev;

    len = 0;
    running = 1;

    while (running && !XNextEvent(dpy, &ev)) {
        if (ev.type == KeyPress) {
            explicit_bzero(&buf, sizeof(buf));
            num = XLookupString(&ev.xkey, buf, sizeof(buf), &ksym, 0);
            if (IsKeypadKey(ksym)) {
                if (ksym == XK_KP_Enter)
                    ksym = XK_Return;
                else if (ksym >= XK_KP_0 && ksym <= XK_KP_9)
                    ksym = (ksym - XK_KP_0) + XK_0;
            }
            if (IsFunctionKey(ksym) ||
                IsKeypadKey(ksym) ||
                IsMiscFunctionKey(ksym) ||
                IsPFKey(ksym) ||
                IsPrivateKeypadKey(ksym))
                continue;
            switch (ksym) {
            case XK_Return:
                passwd[len] = '\0';
                errno = 0;
                if (!(inputhash = crypt(passwd, hash)))
                    fprintf(stderr, "slock: crypt: %s\n", strerror(errno));
                else
                    running = !!strcmp(inputhash, hash);
                if (running) {
                    XBell(dpy, 100);
                    reset_logo(locks, nscreens);
                }
                explicit_bzero(&passwd, sizeof(passwd));
                len = 0;
                break;
            case XK_Escape:
                explicit_bzero(&passwd, sizeof(passwd));
                len = 0;
                drawbackground(dpy, locks, nscreens);
                logo_blocks_left = NUM_MICROBRICKS;
                break;
            case XK_BackSpace:
                if (len) {
                    passwd[--len] = '\0';
                    if (len) {
                        int add = rand() % MAX_BREAK_BRICKS_NUM;
                        logo_blocks_left += MAX(1, add);
                        logo_blocks_left = MIN(NUM_MICROBRICKS - len, logo_blocks_left);
                    } else {
                        logo_blocks_left = NUM_MICROBRICKS;
                    }
                }
                break;
            default:
                if (num && !iscntrl((int)buf[0]) &&
                    (len + num < sizeof(passwd))) {
                    memcpy(passwd + len, buf, num);
                    len += num;

                    int minus = rand() % MAX_BREAK_BRICKS_NUM;
                    logo_blocks_left -= MAX(1, minus);
                    logo_blocks_left = MAX(0, logo_blocks_left);
                }
                break;
            }
            // select color
            if (running) {
                drawlogo(dpy, locks, nscreens);
            }
        } else if (rr->active && ev.type == rr->evbase + RRScreenChangeNotify) {
            rre = (XRRScreenChangeNotifyEvent *)&ev;
            for (screen = 0; screen < nscreens; screen++) {
                if (locks[screen]->win == rre->window) {
                    if (rre->rotation == RR_Rotate_90 ||
                        rre->rotation == RR_Rotate_270)
                        XResizeWindow(dpy, locks[screen]->win,
                                      rre->height, rre->width);
                    else
                        XResizeWindow(dpy, locks[screen]->win,
                                      rre->width, rre->height);
                    XClearWindow(dpy, locks[screen]->win);
                    break;
                }
            }
        } else {
            for (screen = 0; screen < nscreens; screen++)
                XRaiseWindow(dpy, locks[screen]->win);
        }
    }
}

static struct lock *
lockscreen(Display *dpy, struct xrandr *rr, int screen) {
    char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
    int i, ptgrab, kbgrab;
    struct lock *lock;
    XColor color;
    XSetWindowAttributes wa;
    Cursor invisible;
#ifdef XINERAMA
    XineramaScreenInfo *info;
    int n;
#endif

    if (dpy == NULL || screen < 0 || !(lock = malloc(sizeof(struct lock))))
        return NULL;

    lock->screen = screen;
    lock->root = RootWindow(dpy, lock->screen);

    lock->x = DisplayWidth(dpy, lock->screen);
    lock->y = DisplayHeight(dpy, lock->screen);
#ifdef XINERAMA
    if ((info = XineramaQueryScreens(dpy, &n))) {
        lock->xoff = info[0].x_org;
        lock->yoff = info[0].y_org;
        lock->mw = info[0].width;
        lock->mh = info[0].height;
    } else
#endif
    {
        lock->xoff = lock->yoff = 0;
        lock->mw = lock->x;
        lock->mh = lock->y;
    }
    lock->drawable = XCreatePixmap(dpy, lock->root,
                                   lock->x, lock->y, DefaultDepth(dpy, screen));
    lock->gc = XCreateGC(dpy, lock->root, 0, NULL);
    XSetLineAttributes(dpy, lock->gc, 1, LineSolid, CapButt, JoinMiter);

    /* init */
    wa.override_redirect = 1;
    // wa.background_pixel = background_color;
    lock->win = XCreateWindow(dpy, lock->root, 0, 0,
                              lock->x, lock->y,
                              0, DefaultDepth(dpy, lock->screen),
                              CopyFromParent,
                              DefaultVisual(dpy, lock->screen),
                              CWOverrideRedirect | CWBackPixel, &wa);
    lock->pmap = XCreateBitmapFromData(dpy, lock->win, curs, 8, 8);
    invisible = XCreatePixmapCursor(dpy, lock->pmap, lock->pmap,
                                    &color, &color, 0, 0);
    XDefineCursor(dpy, lock->win, invisible);

    caclulate_bricks(lock);

    /* Try to grab mouse pointer *and* keyboard for 600ms, else fail the lock */
    for (i = 0, ptgrab = kbgrab = -1; i < 6; i++) {
        if (ptgrab != GrabSuccess) {
            ptgrab = XGrabPointer(dpy, lock->root, False,
                                  ButtonPressMask | ButtonReleaseMask |
                                      PointerMotionMask,
                                  GrabModeAsync,
                                  GrabModeAsync, None, invisible, CurrentTime);
        }
        if (kbgrab != GrabSuccess) {
            kbgrab = XGrabKeyboard(dpy, lock->root, True,
                                   GrabModeAsync, GrabModeAsync, CurrentTime);
        }

        /* input is grabbed: we can lock the screen */
        if (ptgrab == GrabSuccess && kbgrab == GrabSuccess) {
            XMapRaised(dpy, lock->win);
            if (rr->active)
                XRRSelectInput(dpy, lock->win, RRScreenChangeNotifyMask);

            XSelectInput(dpy, lock->root, SubstructureNotifyMask);
            return lock;
        }

        /* retry on AlreadyGrabbed but fail on other errors */
        if ((ptgrab != AlreadyGrabbed && ptgrab != GrabSuccess) ||
            (kbgrab != AlreadyGrabbed && kbgrab != GrabSuccess))
            break;

        usleep(100000);
    }

    /* we couldn't grab all input: fail out */
    if (ptgrab != GrabSuccess)
        fprintf(stderr, "slock: unable to grab mouse pointer for screen %d\n",
                screen);
    if (kbgrab != GrabSuccess)
        fprintf(stderr, "slock: unable to grab keyboard for screen %d\n",
                screen);
    return NULL;
}

static void
usage(void) {
    die("usage: slock [-v] [cmd [arg ...]]\n");
}

int main(int argc, char **argv) {
    struct xrandr rr;
    struct lock **locks;
    struct passwd *pwd;
    struct group *grp;
    uid_t duid;
    gid_t dgid;
    const char *hash;
    Display *dpy;
    int s, nlocks, nscreens;

    ARGBEGIN {
    case 'v':
        puts("slock-" VERSION);
        return 0;
    default:
        usage();
    }
    ARGEND

    /* validate drop-user and -group */
    errno = 0;
    if (!(pwd = getpwnam(user)))
        die("slock: getpwnam %s: %s\n", user,
            errno ? strerror(errno) : "user entry not found");
    duid = pwd->pw_uid;
    errno = 0;
    if (!(grp = getgrnam(group)))
        die("slock: getgrnam %s: %s\n", group,
            errno ? strerror(errno) : "group entry not found");
    dgid = grp->gr_gid;

#ifdef __linux__
    dontkillme();
#endif

    hash = gethash();
    errno = 0;
    if (!crypt("", hash))
        die("slock: crypt: %s\n", strerror(errno));

    if (!(dpy = XOpenDisplay(NULL)))
        die("slock: cannot open display\n");

    /* drop privileges */
    if (setgroups(0, NULL) < 0)
        die("slock: setgroups: %s\n", strerror(errno));
    if (setgid(dgid) < 0)
        die("slock: setgid: %s\n", strerror(errno));
    if (setuid(duid) < 0)
        die("slock: setuid: %s\n", strerror(errno));

    /* check for Xrandr support */
    rr.active = XRRQueryExtension(dpy, &rr.evbase, &rr.errbase);

    /* get number of screens in display "dpy" and blank them */
    nscreens = ScreenCount(dpy);
    if (!(locks = calloc(nscreens, sizeof(struct lock *))))
        die("slock: out of memory\n");
    for (nlocks = 0, s = 0; s < nscreens; s++) {
        if ((locks[s] = lockscreen(dpy, &rr, s)) != NULL)
            nlocks++;
        else
            break;
    }

    /* did we manage to lock everything? */
    if (nlocks != nscreens)
        return 1;

    /* run post-lock command */
    if (argc > 0) {
        switch (fork()) {
        case -1:
            die("slock: fork failed: %s\n", strerror(errno));
        case 0:
            if (close(ConnectionNumber(dpy)) < 0)
                die("slock: close: %s\n", strerror(errno));
            execvp(argv[0], argv);
            fprintf(stderr, "slock: execvp %s: %s\n", argv[0], strerror(errno));
            _exit(1);
        }
    }

    reset_logo(locks, nscreens);
    drawbackground(dpy, locks, nscreens);
    drawlogo(dpy, locks, nscreens);

    /* everything is now blank. Wait for the correct password */
    readpw(dpy, &rr, locks, nscreens, hash);

    for (nlocks = 0, s = 0; s < nscreens; s++) {
        XFreePixmap(dpy, locks[s]->pmap);
        XFreePixmap(dpy, locks[s]->drawable);
        XFreeGC(dpy, locks[s]->gc);
    }

    XSync(dpy, 0);
    XCloseDisplay(dpy);
    return 0;
}
