logo = """
   #
   #
   #
#### # #####
#  # # # # #
######## # #
"""

PIXELS_PER_BRICK = input("brick's width/height in pixel: ")
if not PIXELS_PER_BRICK:
    PIXELS_PER_BRICK = 100
else:
    PIXELS_PER_BRICK = int(PIXELS_PER_BRICK)
print(f"PIXELS_PER_BRICK = {PIXELS_PER_BRICK}")


MICROBRICKS_PER_BRICK = input("one brick splits into n*n micro-bricks, n = ")
if not MICROBRICKS_PER_BRICK:
    MICROBRICKS_PER_BRICK = 4
else:
    MICROBRICKS_PER_BRICK = int(MICROBRICKS_PER_BRICK)
print(f"MICROBRICKS_PER_BRICK = {MICROBRICKS_PER_BRICK}")
if PIXELS_PER_BRICK % MICROBRICKS_PER_BRICK != 0:
    print("ERROR: PIXELS_PER_BRICK % MICROBRICKS_PER_BRICK != 0")
    exit(1)

DIRTS_PER_MICROBRICK = input("one micro-bricks splits into n*n dirt, n = ")
if not DIRTS_PER_MICROBRICK:
    DIRTS_PER_MICROBRICK = 2
else:
    DIRTS_PER_MICROBRICK = int(DIRTS_PER_MICROBRICK)
print(f"DIRTS_PER_MICROBRICK = {DIRTS_PER_MICROBRICK}")

bricks_num = logo.count('#')
print()
print(f"so:")
print(f"each brick       is {PIXELS_PER_BRICK} pixels")
print(f"each micro-brick is {PIXELS_PER_BRICK / MICROBRICKS_PER_BRICK} pixels")
print(f"each dirt        is {PIXELS_PER_BRICK / MICROBRICKS_PER_BRICK / DIRTS_PER_MICROBRICK} pixels")
print(f"there are {bricks_num} bricks in total")
print(f"there are {bricks_num * MICROBRICKS_PER_BRICK * MICROBRICKS_PER_BRICK} micro-bricks in total")

line_list = [i for i in logo.split("\n") if i != ""]
LOGO_W = len(line_list[-1])
LOGO_H = len(line_list)

print()
print("// copy below to config.def.h")
print("static short bricks_pos[][2] = {")
y = 0
for line in line_list:
    x = 0
    for c in line:
        if c == "#":
            print(f"\t{{ {x}, {y} }},")
        x += 1
    y += 1
print("};")
print(f"#define LOGO_W {LOGO_W}")
print(f"#define LOGO_H {LOGO_H}")
print(f"#define PIXELS_PER_BRICK {PIXELS_PER_BRICK}")
print(f"#define MICROBRICKS_PER_BRICK {MICROBRICKS_PER_BRICK}")
print(f"#define DIRTS_PER_MICROBRICK {DIRTS_PER_MICROBRICK}")
