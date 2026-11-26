from PIL import Image, ImageFont, ImageDraw

chars = [(0x53D6, "取"), (0x56DE, "回")]
font = None
for fp in ["C:/Windows/Fonts/simhei.ttf", "C:/Windows/Fonts/simsun.ttc", "C:/Windows/Fonts/msyh.ttc"]:
    try:
        font = ImageFont.truetype(fp, 14)
        print("using", fp, flush=True)
        break
    except Exception:
        continue
if font is None:
    font = ImageFont.load_default()

SIZE = 16
for cp, ch in chars:
    img = Image.new("L", (SIZE, SIZE), 0)
    d = ImageDraw.Draw(img)
    bbox = d.textbbox((0, 0), ch, font=font)
    w = bbox[2] - bbox[0]; h = bbox[3] - bbox[1]
    x = (SIZE - w)//2 - bbox[0]; y = (SIZE - h)//2 - bbox[1]
    d.text((x, y), ch, fill=255, font=font)
    px = img.load()
    rows = []
    for r in range(SIZE):
        v = 0
        for c in range(SIZE):
            if px[c, r] > 128:
                v |= (0x8000 >> c)
        rows.append(v)
    line = "    {0x" + format(cp, "04X") + ", {" + ", ".join("0x{:04X}".format(val) for val in rows) + "}}, // " + ch
    print(line)
