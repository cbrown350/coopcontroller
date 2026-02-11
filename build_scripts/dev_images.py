import math
import os

def _try_load_font(font_path, size):
    from PIL import ImageFont
    if font_path:
        try:
            return ImageFont.truetype(font_path, size)
        except Exception:
            pass
    for fp in ("DejaVuSans.ttf",):
        try:
            return ImageFont.truetype(fp, size)
        except Exception:
            pass
    win_fonts = [
        r"C:\Windows\Fonts\arial.ttf",
        r"C:\Windows\Fonts\segoeui.ttf",
        r"C:\Windows\Fonts\calibri.ttf",
    ]
    for fp in win_fonts:
        try:
            return ImageFont.truetype(fp, size)
        except Exception:
            pass
    return ImageFont.load_default()

def _get_text_bbox(draw, text, font, stroke_w):
    # Prefer Pillow 8.0+ textbbox (handles ascent/descent and stroke). Fallback to textsize.
    try:
        return draw.textbbox((0, 0), text, font=font, stroke_width=stroke_w)
    except AttributeError:
        w, h = draw.textsize(text, font=font)
        return (0, 0, w, h)

def _rotated_bounds(w, h, angle_deg):
    theta = math.radians(abs(angle_deg))
    c, s = math.cos(theta), math.sin(theta)
    # Axis-aligned bounding box after rotation
    bw = w * c + h * s
    bh = w * s + h * c
    return bw, bh

def _render_diagonal_text(img, text="DEV", font_path=None, color=(255, 0, 0, 180), angle_deg=-45):
    from PIL import Image, ImageDraw

    width, height = img.size
    margin = max(4, int(min(width, height) * 0.015))  # edge margin
    baseline_size = 100
    stroke_ratio = 0.06

    # 1) Measure at baseline
    probe_font = _try_load_font(font_path, baseline_size)
    probe_img = Image.new("RGBA", (1, 1), (0, 0, 0, 0))
    probe_draw = ImageDraw.Draw(probe_img)
    probe_stroke = max(1, int(baseline_size * stroke_ratio))
    l, t, r, b = _get_text_bbox(probe_draw, text, probe_font, probe_stroke)
    text_w0, text_h0 = (r - l), (b - t)

    # Include padding for stroke and safe spacing
    pad0 = max(2, int(baseline_size * 0.15))
    strip_w0 = text_w0 + 2 * pad0
    strip_h0 = text_h0 + 2 * pad0

    # Rotated bounds at baseline
    rw0, rh0 = _rotated_bounds(strip_w0, strip_h0, angle_deg)

    # 2) Scale to fit inside image (no clipping), near full-bleed
    sf_w = (width - 2 * margin) / rw0
    sf_h = (height - 2 * margin) / rh0
    scale = max(12 / baseline_size, min(sf_w, sf_h) * 0.98)  # keep slight margin, enforce min size

    computed_size = max(12, int(baseline_size * scale))
    font = _try_load_font(font_path, computed_size)
    stroke_w = max(1, int(computed_size * stroke_ratio))
    pad = max(2, int(computed_size * 0.15))

    # Re-measure with final font, including stroke
    l, t, r, b = _get_text_bbox(probe_draw, text, font, stroke_w)
    text_w, text_h = (r - l), (b - t)
    strip_w = text_w + 2 * pad
    strip_h = text_h + 2 * pad

    # 3) Draw centered in its own strip, compensating for bbox offsets
    text_img = Image.new("RGBA", (strip_w, strip_h), (255, 255, 255, 0))
    draw_text = ImageDraw.Draw(text_img)
    # Draw so that full bbox lands within the strip (avoid cutoff top/bottom)
    draw_pos = (pad - l, pad - t)
    draw_text.text(
        draw_pos,
        text,
        font=font,
        fill=color,
        stroke_width=stroke_w,
        stroke_fill=(255, 255, 255, color[3] if len(color) > 3 else 255),
    )

    # 4) Rotate and center onto original image
    rotated_text = text_img.rotate(angle_deg, expand=True)
    rw, rh = rotated_text.size
    # If rotation still slightly exceeds image, downscale the rotated layer once
    if rw > width - 2 * margin or rh > height - 2 * margin:
        scale2 = min((width - 2 * margin) / rw, (height - 2 * margin) / rh) * 0.98
        new_w = max(1, int(rw * scale2))
        new_h = max(1, int(rh * scale2))
        rotated_text = rotated_text.resize((new_w, new_h), resample=Image.LANCZOS)
        rw, rh = rotated_text.size

    x = (width - rw) // 2
    y = (height - rh) // 2

    layer = Image.new("RGBA", img.size, (255, 255, 255, 0))
    layer.paste(rotated_text, (x, y), rotated_text)
    combined = Image.alpha_composite(img.convert("RGBA"), layer)
    return combined

def _watermark_raster(file_path, out_path, text, font_path, color):
    from PIL import Image
    img = Image.open(file_path).convert("RGBA")
    combined = _render_diagonal_text(img, text=text, font_path=font_path, color=color)
    ext = os.path.splitext(file_path)[1].lower()
    save_kwargs = {}
    if ext == ".webp":
        save_kwargs.update(dict(quality=95, method=6))
    combined.save(out_path, **save_kwargs)

def _watermark_ico(file_path, out_path, text, font_path, color):
    # Extract the largest frame, watermark it, then save back to ICO with original sizes
    from PIL import Image
    with Image.open(file_path) as ico:
        sizes = []
        largest_img = None
        largest_area = -1
        try:
            n = getattr(ico, "n_frames", 1)
        except Exception:
            n = 1
        for i in range(n):
            try:
                ico.seek(i)
            except Exception:
                break
            frame = ico.convert("RGBA").copy()
            w, h = frame.size
            if w == h:  # prefer square sizes in the set
                sizes.append((w, h))
            area = w * h
            if area > largest_area:
                largest_area = area
                largest_img = frame

    if not sizes and largest_img:
        sizes = [largest_img.size]

    if not largest_img:
        # Fallback: treat as raster if decoding failed
        _watermark_raster(file_path, out_path, text, font_path, color)
        return

    watermarked = _render_diagonal_text(largest_img, text=text, font_path=font_path, color=color)
    # Sort and de-dup sizes, ensure typical favicon sizes if none detected
    sizes = sorted(set(sizes))
    if not sizes:
        sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]
    # Pillow will downscale the provided image to each size
    watermarked.save(out_path, format="ICO", sizes=sizes)

def add_diagonal_text(image_path, output_path, text="DEV", font_path=None, font_size=40, color=(255, 0, 0, 180)):
    # Verify PIL is installed
    try:
        from PIL import Image  # noqa: F401
    except ImportError as e:
        print("Required module not found:", e)
        print("In order to modify dev images, install Pillow (pip install Pillow); you may need to activate your Platformio environment first ('source ~/.platformio/penv/bin/activate' or '%USERPROFILE%\\.platformio\\penv\\Scripts\\activate.bat' depending on platform).")
        raise e

    ext = os.path.splitext(image_path)[1].lower()
    try:
        if ext == ".ico":
            _watermark_ico(image_path, output_path, text, font_path, color)
        else:
            _watermark_raster(image_path, output_path, text, font_path, color)
    except Exception as ex:
        print(f"Watermark failed for {image_path}: {ex}")