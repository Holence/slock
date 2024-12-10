/* user and group to drop privileges to */
static const char *user = "nobody";
static const char *group = "nogroup";

// color from Minecraft Grass and Mossy Cobblestone
static const char *BACKGROUND_COLOR = "#2E231E";
static const char *BRICK_COLORS[] = {
    "#464d38",
    "#676865",
    "#657547",
    "#728452",
    "#5b6d41",
    "#7c8b5b",
    "#a2ac97",
    "#323a23",
    "#3a3d36",
    "#4d5c35",
    "#3f4829",
    "#35482a",
    "#273221",
};

// how many brick disappear per input
#define MAX_BREAK_BRICKS_NUM 25

// use pixels.py to generate stuff below
static XRectangle bricks[] = {
    {3, 0, 1, 1},
    {3, 1, 1, 1},
    {3, 2, 1, 1},
    {0, 3, 1, 1},
    {1, 3, 1, 1},
    {2, 3, 1, 1},
    {3, 3, 1, 1},
    {5, 3, 1, 1},
    {7, 3, 1, 1},
    {8, 3, 1, 1},
    {9, 3, 1, 1},
    {10, 3, 1, 1},
    {11, 3, 1, 1},
    {0, 4, 1, 1},
    {3, 4, 1, 1},
    {5, 4, 1, 1},
    {7, 4, 1, 1},
    {9, 4, 1, 1},
    {11, 4, 1, 1},
    {0, 5, 1, 1},
    {1, 5, 1, 1},
    {2, 5, 1, 1},
    {3, 5, 1, 1},
    {4, 5, 1, 1},
    {5, 5, 1, 1},
    {6, 5, 1, 1},
    {7, 5, 1, 1},
    {9, 5, 1, 1},
    {11, 5, 1, 1},
};
#define LOGO_W           12
#define LOGO_H           6
#define PIXEL_PER_BRICK  100
#define MICRO_BRICKS_NUM 5