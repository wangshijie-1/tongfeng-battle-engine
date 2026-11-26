import struct, sys
from PIL import Image, ImageFont, ImageDraw

# 需要生成的汉字： (unicode码点, 汉字)
chars = [
    (0x4FEE, "修"),
    (0x5C06, "将"),
    (0x5931, "失"),
    (0x53BB, "去"),
    (0x6863, "档"),
    (0x6D88, "消"),
    (0x952E, "键"),
    (0x8FD4, "返"),
    (0x5B58, "存"),
    (0x6539, "改"),
    (0x53D8, "变"),
    (0x6E05, "清"),
    (0x7A7A, "空"),
]

# 尝试常见中文字体
font_paths = [
    "C:/Windows/Fonts/simhei.ttf",
    "C:/Windows/Fonts/simsun.ttc",
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/STHeiti.TTF",
    "C:/Windows/Fonts/simkai.ttf",
]
font = None
for fp in font_paths:
    try:
        font = ImageFont.truetype(fp, 14)
        print("using font:", fp, file=sys.stderr)
        break
    except Exception as e:
        continue
if font is None:
    font = ImageFont.load_default()
    print("using default font (no CJK)", file=sys.stderr)

SIZE = 16
out = []
for cp, ch in chars:
    img = Image.new("L", (SIZE, SIZE), 0)
    d = ImageDraw.Draw(img)
    # 居中绘制
    bbox = d.textbbox((0, 0), ch, font=font)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    x = (SIZE - w) // 2 - bbox[0]
    y = (SIZE - h) // 2 - bbox[1]
    d.text((x, y), ch, fill=255, font=font)
    # 二值化
    px = img.load()
    rows = []
    for r in range(SIZE):
        val = 0
        for c in range(SIZE):
            if px[c, r] > 128:
                val |= (0x8000 >> c)
        rows.append(val)
    out.append((cp, ch, rows))

# 输出 C 数组
for cp, ch, rows in out:
    line = "    {0x" + format(cp, "04X") + ", {" + ", ".join("0x{:04X}".format(v) for v in rows) + "}}, // " + ch
    print(line)
