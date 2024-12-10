logo = """
   #
   #
   #
#### # #####
#  # # # # #
######## # #
"""

PIXEL_PER_BRICK = input("brick's width/height in pixel: ")
if not PIXEL_PER_BRICK:
    PIXEL_PER_BRICK = 100
else:
    PIXEL_PER_BRICK = int(PIXEL_PER_BRICK)
print(f"PIXEL_PER_BRICK = {PIXEL_PER_BRICK}")

MICRO_BRICKS_NUM = input("one brick splits into n*n micro-bricks, n = ")
if not MICRO_BRICKS_NUM:
    MICRO_BRICKS_NUM = 4
else:
    MICRO_BRICKS_NUM = int(MICRO_BRICKS_NUM)
print(f"MICRO_BRICKS_NUM = {MICRO_BRICKS_NUM}")

if PIXEL_PER_BRICK % MICRO_BRICKS_NUM != 0:
    print("ERROR: PIXEL_PER_BRICK % MICRO_BRICKS_NUM != 0")
    exit(1)

print()
print(f"so each micro-brick is {PIXEL_PER_BRICK / MICRO_BRICKS_NUM} pixels")
print(f"there are {logo.count('#')} bricks in total")
print(f"and {logo.count('#') * MICRO_BRICKS_NUM * MICRO_BRICKS_NUM} micro-bricks in total")

line_list = [i for i in logo.split("\n") if i != ""]
LOGO_W = len(line_list[-1])
LOGO_H = len(line_list)

print()
print("// copy below to config.def.h")
print("static XRectangle bricks[] = {")
y = 0
for line in line_list:
    x = 0
    for c in line:
        if c == "#":
            print(f"\t{{ {x}, {y}, 1, 1 }},")
        x += 1
    y += 1
print("};")
print(f"#define LOGO_W {LOGO_W}")
print(f"#define LOGO_H {LOGO_H}")
print(f"#define PIXEL_PER_BRICK {PIXEL_PER_BRICK}")
print(f"#define MICRO_BRICKS_NUM {MICRO_BRICKS_NUM}")
