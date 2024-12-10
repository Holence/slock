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

#include "arg.h"
#include "util.h"

char *argv0;

#include "config.h"
#define NUM_BRICKS       LENGTH(bricks)
#define NUM_MICROBRICKS  (NUM_BRICKS * MICRO_BRICKS_NUM * MICRO_BRICKS_NUM)
#define NUM_BRICK_COLORS LENGTH(BRICK_COLORS)

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
    XRectangle bricks[NUM_MICROBRICKS];
};
unsigned long background_color;
unsigned long brick_colors[NUM_BRICK_COLORS];

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

static void
caclulate_bricks(struct lock *lock) {
    size_t index = 0;
    for (size_t n = 0; n < NUM_BRICKS; n++) {
        unsigned short x = (bricks[n].x * PIXEL_PER_BRICK) + lock->xoff + ((lock->mw) / 2) - (LOGO_W / 2 * PIXEL_PER_BRICK);
        unsigned short y = (bricks[n].y * PIXEL_PER_BRICK) + lock->yoff + ((lock->mh) / 2) - (LOGO_H / 2 * PIXEL_PER_BRICK);
        unsigned short micro_width = bricks[n].width * PIXEL_PER_BRICK / MICRO_BRICKS_NUM;
        unsigned short micro_height = bricks[n].height * PIXEL_PER_BRICK / MICRO_BRICKS_NUM;

        unsigned short microbrick_y = y;
        for (size_t i = 0; i < MICRO_BRICKS_NUM; i++) {
            unsigned short microbrick_x = x;
            for (size_t j = 0; j < MICRO_BRICKS_NUM; j++) {
                lock->bricks[index].x = microbrick_x;
                lock->bricks[index].y = microbrick_y;
                lock->bricks[index].width = micro_width;
                lock->bricks[index].height = micro_height;
                microbrick_x += micro_width;
                index++;
            }
            microbrick_y += micro_height;
        }
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

static unsigned int seed = 0;
static int logo_blocks_left = NUM_MICROBRICKS;

static void
reset_logo(struct lock **locks, int nscreens) {
    seed = time(NULL);
    srand(seed);
    logo_blocks_left = NUM_MICROBRICKS;
    for (size_t screen = 0; screen < nscreens; screen++) {
        shuffle(locks[screen]->bricks, NUM_MICROBRICKS, sizeof(XRectangle));
    }
}

static void
drawlogo(Display *dpy, struct lock **lock, int nscreens) {
    for (int screen = 0; screen < nscreens; screen++) {
        XSetForeground(dpy, lock[screen]->gc, background_color);
        XFillRectangle(dpy, lock[screen]->drawable, lock[screen]->gc, 0, 0, lock[screen]->x, lock[screen]->y);
        srand(seed);
        for (size_t i = 0; i < logo_blocks_left; i++) {
            XSetForeground(dpy, lock[screen]->gc, brick_colors[rand() % NUM_BRICK_COLORS]);
            XFillRectangle(dpy, lock[screen]->drawable, lock[screen]->gc, lock[screen]->bricks[i].x, lock[screen]->bricks[i].y, lock[screen]->bricks[i].width, lock[screen]->bricks[i].height);
        }
        XCopyArea(dpy, lock[screen]->drawable, lock[screen]->win, lock[screen]->gc, 0, 0, lock[screen]->x, lock[screen]->y, 0, 0);
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
                reset_logo(locks, nscreens);
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
    XColor color, dummy;
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

    XAllocNamedColor(dpy, DefaultColormap(dpy, lock->screen), BACKGROUND_COLOR, &color, &dummy);
    background_color = color.pixel;
    for (i = 0; i < NUM_BRICK_COLORS; i++) {
        // set random color to each brick
        XAllocNamedColor(dpy, DefaultColormap(dpy, lock->screen), BRICK_COLORS[i], &color, &dummy);
        brick_colors[i] = color.pixel;
    }

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
    XSync(dpy, 0);

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
    drawlogo(dpy, locks, nscreens);

    /* everything is now blank. Wait for the correct password */
    readpw(dpy, &rr, locks, nscreens, hash);

    for (nlocks = 0, s = 0; s < nscreens; s++) {
        XFreePixmap(dpy, locks[s]->drawable);
        XFreeGC(dpy, locks[s]->gc);
    }

    XSync(dpy, 0);
    XCloseDisplay(dpy);
    return 0;
}
