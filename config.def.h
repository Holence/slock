/* user and group to drop privileges to */
static const char *user = "nobody";
static const char *group = "nogroup";

// color from Minecraft Grass
static const unsigned long GRASS_COLORS[] = {
    0x2c3a21,  // #2c3a21
    0x34442a,  // #34442a
    0x3d5030,  // #3d5030
    0x465c38,  // #465c38
    0x556e46,  // #556e46
    0x74945c,  // #74945c
};
// color from Minecraft Dirt
static const unsigned long DIRT_COLORS[] = {
    0x2E231E,  // #2E231E
    0x312318,  // #312318
    0x432d20,  // #432d20
    0x452f21,  // #452f21
    0x474b4c,  // #474b4c
    0x4e4f51,  // #4e4f51
    0x583d28,  // #583d28
    0x5a3f2a,  // #5a3f2a
    0x656668,  // #656668
    0x684934,  // #684934
    0x745135,  // #745135
    0x876041,  // #876041
    0x8a6344,  // #8a6344
};
// color from Minecraft Cobblestone
static const unsigned long BRICK_COLORS[] = {
    0x273221,  // #273221
    0x303821,  // #303821
    0x323a23,  // #323a23
    0x35482a,  // #35482a
    0x3f4829,  // #3f4829
    0x464d38,  // #464d38
    0x4d5c35,  // #4d5c35
    0x4e5d36,  // #4e5d36
    0x505d2f,  // #505d2f
    0x5b6d41,  // #5b6d41
    0x627243,  // #627243
    0x657547,  // #657547
    0x676767,  // #676767
    0x728452,  // #728452
    0x7c8b5b,  // #7c8b5b
    0xa2ac97,  // #a2ac97
    0xb3b994,  // #b3b994
};

// how many brick disappear per input
#define MAX_BREAK_BRICKS_NUM 25

// use pixels.py to generate stuff below
static short bricks_pos[][2] = {
    {3, 0},
    {3, 1},
    {3, 2},
    {0, 3},
    {1, 3},
    {2, 3},
    {3, 3},
    {5, 3},
    {7, 3},
    {8, 3},
    {9, 3},
    {10, 3},
    {11, 3},
    {0, 4},
    {3, 4},
    {5, 4},
    {7, 4},
    {9, 4},
    {11, 4},
    {0, 5},
    {1, 5},
    {2, 5},
    {3, 5},
    {4, 5},
    {5, 5},
    {6, 5},
    {7, 5},
    {9, 5},
    {11, 5},
};
#define LOGO_W                12
#define LOGO_H                6
#define PIXELS_PER_BRICK      150
#define MICROBRICKS_PER_BRICK 5
#define DIRTS_PER_MICROBRICK  2