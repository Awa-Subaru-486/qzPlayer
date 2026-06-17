#!/usr/bin/env python3
import os
import sys
import subprocess
import asyncio
import struct

NUMBERS_ZH = ["一", "二", "三", "四", "五", "六", "七", "八", "九", "十"]
NUMBERS_EN = ["One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten"]
DURATION = 1
FPS = 30
WIDTH, HEIGHT = 1920, 1080
BUILD = "build"
OUTPUT = "chinese_numbers.mkv"

def run_ffmpeg(args, check=True):
    cmd = ["ffmpeg", "-y"] + args
    r = subprocess.run(cmd, capture_output=True, text=True)
    if check and r.returncode != 0:
        raise RuntimeError(r.stderr)
    return r

def ensure_deps():
    import importlib
    for pkg, mod in [("Pillow", "PIL"), ("edge-tts", "edge_tts")]:
        try:
            importlib.import_module(mod)
        except ImportError:
            subprocess.run(
                [sys.executable, "-m", "pip", "install", pkg], check=True
            )

def _find_font(size, prefer_cjk=True):
    from PIL import ImageFont
    cjk_fonts = [
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc",
    ]
    latin_fonts = [
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/times.ttf",
    ]
    candidates = cjk_fonts if prefer_cjk else latin_fonts
    for fp in candidates:
        if os.path.exists(fp):
            try:
                return ImageFont.truetype(fp, size)
            except Exception:
                continue
    return ImageFont.load_default()

def make_images():
    from PIL import Image, ImageDraw
    font_zh = _find_font(300, prefer_cjk=True)
    font_en = _find_font(260, prefer_cjk=False)
    for i, ch in enumerate(NUMBERS_ZH):
        img = Image.new("RGB", (WIDTH, HEIGHT), (255, 255, 255))
        draw = ImageDraw.Draw(img)
        draw.text((WIDTH // 2, HEIGHT // 2), ch, fill=(0, 0, 0), font=font_zh, anchor="mm")
        img.save(os.path.join(BUILD, f"zh_img_{i:02d}.png"))
    for i, word in enumerate(NUMBERS_EN):
        img = Image.new("RGB", (WIDTH, HEIGHT), (255, 255, 255))
        draw = ImageDraw.Draw(img)
        draw.text((WIDTH // 2, HEIGHT // 2), word, fill=(0, 0, 0), font=font_en, anchor="mm")
        img.save(os.path.join(BUILD, f"en_img_{i:02d}.png"))

def fmt_srt_time(seconds):
    h = int(seconds) // 3600
    m = (int(seconds) % 3600) // 60
    s = int(seconds) % 60
    ms = int((seconds - int(seconds)) * 1000)
    return f"{h:02d}:{m:02d}:{s:02d},{ms:03d}"

def fmt_vtt_time(seconds):
    h = int(seconds) // 3600
    m = (int(seconds) % 3600) // 60
    s = int(seconds) % 60
    ms = int((seconds - int(seconds)) * 1000)
    return f"{h:02d}:{m:02d}:{s:02d}.{ms:03d}"

def make_srt():
    lines = []
    for i, ch in enumerate(NUMBERS_ZH):
        s, e = i * DURATION, (i + 1) * DURATION
        lines.append(str(i + 1))
        lines.append(f"{fmt_srt_time(s)} --> {fmt_srt_time(e)}")
        lines.append(ch)
        lines.append("")
    with open(os.path.join(BUILD, "sub.srt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

def make_ass():
    lines = [
        "[Script Info]",
        "ScriptType: v4.00+",
        f"PlayResX: {WIDTH}",
        f"PlayResY: {HEIGHT}",
        "",
        "[V4+ Styles]",
        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
        "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
        "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
        "Alignment, MarginL, MarginR, MarginV, Encoding",
        "Style: Default,Microsoft YaHei,52,&H00FFFFFF,&H000000FF,&H00000000,"
        "&H80000000,-1,0,0,0,100,100,0,0,1,2,1,2,10,10,30,1",
        "",
        "[Events]",
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text",
    ]
    for i, ch in enumerate(NUMBERS_ZH):
        s, e = i * DURATION, (i + 1) * DURATION
        lines.append(
            f"Dialogue: 0,{s // 3600}:{s % 3600 // 60:02d}:{s % 60:05.2f},"
            f"{e // 3600}:{e % 3600 // 60:02d}:{e % 60:05.2f},"
            f"Default,,0,0,0,,{ch}"
        )
    with open(os.path.join(BUILD, "sub.ass"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

def make_vtt():
    lines = ["WEBVTT", ""]
    for i, ch in enumerate(NUMBERS_ZH):
        s, e = i * DURATION, (i + 1) * DURATION
        lines.append(f"{fmt_vtt_time(s)} --> {fmt_vtt_time(e)}")
        lines.append(ch)
        lines.append("")
    with open(os.path.join(BUILD, "sub.vtt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

async def make_audio():
    import edge_tts
    for i, ch in enumerate(NUMBERS_ZH):
        raw = os.path.join(BUILD, f"zh_tts_{i:02d}.mp3")
        wav = os.path.join(BUILD, f"zh_aud_{i:02d}.wav")
        await edge_tts.Communicate(ch, "zh-CN-XiaoxiaoNeural").save(raw)
        run_ffmpeg(["-i", raw, "-af", f"apad=whole_dur={DURATION}", "-t", str(DURATION), "-ar", "44100", "-ac", "1", wav])
    for i, word in enumerate(NUMBERS_EN):
        raw = os.path.join(BUILD, f"en_tts_{i:02d}.mp3")
        wav = os.path.join(BUILD, f"en_aud_{i:02d}.wav")
        await edge_tts.Communicate(word, "en-US-JennyNeural").save(raw)
        run_ffmpeg(["-i", raw, "-af", f"apad=whole_dur={DURATION}", "-t", str(DURATION), "-ar", "44100", "-ac", "1", wav])

def _build_video_from_images(prefix):
    img_list = os.path.join(BUILD, f"{prefix}_img_list.txt")
    numbers = NUMBERS_ZH if prefix == "zh" else NUMBERS_EN
    with open(img_list, "w", encoding="utf-8") as f:
        for i in range(len(numbers)):
            p = os.path.abspath(os.path.join(BUILD, f"{prefix}_img_{i:02d}.png")).replace("\\", "/")
            f.write(f"file '{p}'\nduration {DURATION}\n")
        p = os.path.abspath(os.path.join(BUILD, f"{prefix}_img_{len(numbers) - 1:02d}.png")).replace("\\", "/")
        f.write(f"file '{p}'\n")
    vid = os.path.join(BUILD, f"{prefix}_video.mp4")
    run_ffmpeg([
        "-f", "concat", "-safe", "0", "-i", img_list,
        "-vf", f"fps={FPS},format=yuv420p",
        "-t", str(len(numbers) * DURATION),
        "-c:v", "libx264", "-preset", "fast", "-crf", "18",
        vid,
    ])
    return vid

def _build_audio_from_wavs(prefix):
    aud_list = os.path.join(BUILD, f"{prefix}_aud_list.txt")
    numbers = NUMBERS_ZH if prefix == "zh" else NUMBERS_EN
    with open(aud_list, "w", encoding="utf-8") as f:
        for i in range(len(numbers)):
            p = os.path.abspath(os.path.join(BUILD, f"{prefix}_aud_{i:02d}.wav")).replace("\\", "/")
            f.write(f"file '{p}'\n")
    aud = os.path.join(BUILD, f"{prefix}_audio.m4a")
    run_ffmpeg(["-f", "concat", "-safe", "0", "-i", aud_list, "-c:a", "aac", "-b:a", "192k", aud])
    return aud

def build_mkv_with_text_subs(zh_vid, en_vid, zh_aud, en_aud):
    ass_path = os.path.abspath(os.path.join(BUILD, "sub.ass")).replace("\\", "/")
    srt_path = os.path.abspath(os.path.join(BUILD, "sub.srt")).replace("\\", "/")
    vtt_path = os.path.abspath(os.path.join(BUILD, "sub.vtt")).replace("\\", "/")
    run_ffmpeg([
        "-i", zh_vid, "-i", en_vid, "-i", zh_aud, "-i", en_aud,
        "-i", ass_path, "-i", srt_path, "-i", vtt_path,
        "-map", "0:v", "-map", "1:v", "-map", "2:a", "-map", "3:a",
        "-map", "4:s", "-map", "5:s", "-map", "6:s",
        "-c:v", "copy", "-c:a", "copy",
        "-c:s:0", "ass", "-c:s:1", "subrip", "-c:s:2", "webvtt",
        "-metadata:s:v:0", "language=chi", "-metadata:s:v:0", "handler_name=Chinese",
        "-metadata:s:v:1", "language=eng", "-metadata:s:v:1", "handler_name=English",
        "-metadata:s:a:0", "language=chi", "-metadata:s:a:0", "handler_name=Chinese",
        "-metadata:s:a:1", "language=eng", "-metadata:s:a:1", "handler_name=English",
        "-metadata:s:s:0", "language=chi", "-metadata:s:s:0", "handler_name=ASS",
        "-metadata:s:s:1", "language=chi", "-metadata:s:s:1", "handler_name=SRT",
        "-metadata:s:s:2", "language=chi", "-metadata:s:s:2", "handler_name=WebVTT",
        OUTPUT,
    ])

def _pgs_rle_encode(row):
    out = bytearray()
    i = 0
    n = len(row)
    while i < n:
        val = row[i]
        run = 1
        while i + run < n and row[i + run] == val and run < 16383:
            run += 1
        out.extend(_pgs_rle_run(run, val))
        i += run
    out.append(0x00)
    out.append(0x00)
    return bytes(out)

def _pgs_rle_run(run, val):
    out = bytearray()
    if val != 0 and run == 1:
        out.append(val)
    elif val == 0 and run <= 63:
        out.append(0x00)
        out.append(run & 0x3F)
    elif val == 0 and run > 63:
        out.append(0x00)
        out.append(0x40 | ((run >> 8) & 0x3F))
        out.append(run & 0xFF)
    elif val != 0 and run <= 63:
        out.append(0x00)
        out.append(0x80 | (run & 0x3F))
        out.append(val)
    else:
        out.append(0x00)
        out.append(0xC0 | ((run >> 8) & 0x3F))
        out.append(run & 0xFF)
        out.append(val)
    return out

def _pgs_segment(seg_type, pts, dts, data):
    hdr = b"\x50\x47"
    hdr += struct.pack(">I", pts)
    hdr += struct.pack(">I", dts)
    hdr += struct.pack("B", seg_type)
    hdr += struct.pack(">H", len(data))
    return hdr + data

def rgb_to_ycbcr(r, g, b):
    """将RGB转换为PGS需要的YCbCr色彩空间"""
    y  = int(0.299 * r + 0.587 * g + 0.114 * b)
    cb = int(-0.168736 * r - 0.331264 * g + 0.5 * b + 128)
    cr = int(0.5 * r - 0.418688 * g - 0.081312 * b + 128)
    return max(0, min(255, y)), max(0, min(255, cb)), max(0, min(255, cr))

def make_pgs():
    from PIL import Image, ImageDraw
    font = _find_font(52, prefer_cjk=True)
    sub_w, sub_h = WIDTH, 140
    y_off = HEIGHT - sub_h - 30
    fps_pts = 90000 // FPS
    pgs_data = bytearray()
    
    # 调色板 (使用 YCbCr + Alpha 格式)
    palette_rgb = [(0, 0, 0, 0), (255, 255, 255, 255), (0, 0, 0, 255)]
    
    for i, ch in enumerate(NUMBERS_ZH):
        img = Image.new("RGBA", (sub_w, sub_h), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        bbox = draw.textbbox((0, 0), ch, font=font)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        tx, ty = (sub_w - tw) // 2, (sub_h - th) // 2
        draw.text((tx + 2, ty + 2), ch, fill=(0, 0, 0, 255), font=font)
        draw.text((tx, ty), ch, fill=(255, 255, 255, 255), font=font)

        pixels = list(img.getdata())
        idx_map = []
        for r, g, b, a in pixels:
            if a < 32:
                idx_map.append(0)
            elif r > 128 and g > 128 and b > 128:
                idx_map.append(1)
            else:
                idx_map.append(2)

        rle_buf = bytearray()
        for row_start in range(0, len(idx_map), sub_w):
            rle_buf.extend(_pgs_rle_encode(idx_map[row_start:row_start + sub_w]))

        pts_s = i * DURATION * 90000
        pts_e = (i + 1) * DURATION * 90000
        dts_v = max(0, pts_s - 3 * fps_pts)

        # 1. PCS (呈现合成段) - 开始
        # 规范格式: video_w(H), video_h(H), comp_num(H), comp_state(B), pal_upd(B), pal_id(B), num_obj(B), [obj_id(H), win_id(B), crop(B), x(H), y(H)]
        pcs = struct.pack(">HHBHBBBBHBBHH",
            WIDTH, HEIGHT,
            0x14,
            i * 2,
            0x80,
            0x00,
            0,
            1,
            0,
            0,
            0,
            (WIDTH - sub_w) // 2,
            y_off
        )
        pgs_data.extend(_pgs_segment(0x16, pts_s, dts_v, pcs))

        # 2. WDS (窗口定义段)
        # 规范格式: num_wins(B), win_id(B), x(H), y(H), w(H), h(H)
        wds = struct.pack(">BBHHHH",
            1,                      # number_of_windows
            0,                      # window_id
            (WIDTH - sub_w) // 2,   # x_position
            y_off,                  # y_position
            sub_w,                  # width
            sub_h                   # height
        )
        pgs_data.extend(_pgs_segment(0x17, pts_s, dts_v, wds))

        # 3. PDS (调色板段)
        # 规范格式: pal_id(B), pal_ver(B), [entry_id(B), Y(B), Cr(B), Cb(B), Alpha(B)]
        pds = struct.pack(">BB", 0, len(palette_rgb))
        for idx, (r, g, b, a) in enumerate(palette_rgb):
            y, cb, cr = rgb_to_ycbcr(r, g, b)
            pds += struct.pack("BBBBB", idx, y, cr, cb, a)
        pgs_data.extend(_pgs_segment(0x14, pts_s, dts_v, pds))

        # 4. ODS (对象定义段)
        # 规范格式: obj_id(H), obj_ver(B), last_seq(B), data_len(I), width(H), height(H)
        data_len = 4 + len(rle_buf)
        ods_hdr = struct.pack(">HBB", 0, 0, 0xC0)
        ods_hdr += bytes([(data_len >> 16) & 0xFF, (data_len >> 8) & 0xFF, data_len & 0xFF])
        ods_hdr += struct.pack(">HH", sub_w, sub_h)
        pgs_data.extend(_pgs_segment(0x15, pts_s, dts_v, ods_hdr + bytes(rle_buf)))
        
        # 5. END (显示结束段)
        pgs_data.extend(_pgs_segment(0x80, pts_s, dts_v, b""))

        # 6. PCS (呈现合成段) - 清空显示
        # 发送 num_obj=0 的 PCS 段来清空当前字幕
        pcs_e = struct.pack(">HHBHBBBB",
            WIDTH, HEIGHT,
            0x14,
            i * 2 + 1,
            0x80,
            0x00,
            0,
            0
        )
        pgs_data.extend(_pgs_segment(0x16, pts_e, dts_v, pcs_e))
        
        # 7. END (清空显示结束段)
        pgs_data.extend(_pgs_segment(0x80, pts_e, dts_v, b""))

    sup_path = os.path.join(BUILD, "sub.sup")
    with open(sup_path, "wb") as f:
        f.write(pgs_data)
    return sup_path

def _fix_mkv_duration(path, duration_s):
    with open(path, "rb") as f:
        data = bytearray(f.read())
    ts_scale = 1000000
    ts_id = b'\x2a\xd7\xb1'
    idx = data.find(ts_id)
    if idx >= 0:
        off = idx + 3
        sz_byte = data[off]
        if sz_byte & 0x80:
            sz = sz_byte & 0x7f
            off += 1
        elif sz_byte & 0x40:
            sz = ((sz_byte & 0x3f) << 8) | data[off + 1]
            off += 2
        else:
            sz = 0
        if sz > 0:
            ts_scale = int.from_bytes(data[off:off + sz], 'big')
    correct = (duration_s * 1e9) / ts_scale
    dur_id = b'\x44\x89'
    idx = 0
    while True:
        idx = data.find(dur_id, idx)
        if idx < 0:
            break
        off = idx + 2
        sz_byte = data[off]
        if sz_byte == 0x88:
            off += 1
            data[off:off + 8] = struct.pack(">d", correct)
            break
        elif sz_byte == 0x84:
            off += 1
            data[off:off + 4] = struct.pack(">f", correct)
            break
        idx += 2
    with open(path, "wb") as f:
        f.write(data)

def _extract_err(stderr):
    for line in stderr.strip().split("\n"):
        lower = line.lower()
        if any(kw in lower for kw in ["error", "could not", "unsupported", "not currently", "cannot", "invalid", "not implemented"]):
            return line.strip()
    for line in reversed(stderr.strip().split("\n")):
        if line.strip():
            return line.strip()
    return "Unknown error"

def _count_sub_streams(path):
    r = subprocess.run(
        ["ffprobe", "-v", "quiet", "-select_streams", "s", "-show_entries", "stream=index", "-of", "csv=p=0", path],
        capture_output=True, text=True,
    )
    return len([l for l in r.stdout.strip().split("\n") if l.strip()])

def _pgs_sub_index(path):
    r = subprocess.run(
        ["ffprobe", "-v", "quiet", "-select_streams", "s", "-show_entries", "stream=index,codec_name", "-of", "csv=p=0", path],
        capture_output=True, text=True,
    )
    for line in r.stdout.strip().split("\n"):
        parts = line.strip().split(",")
        if len(parts) == 2 and parts[1] in ("pgssub", "hdmv_pgs_subtitle"):
            return int(parts[0])
    return None

def try_add_bitmap_from_pgs(name, codec, codec_id):
    pgs_idx = _pgs_sub_index(OUTPUT)
    if pgs_idx is None:
        return ("❌ 失败", codec_id, "PGS stream not found")
    sub_count = _count_sub_streams(OUTPUT)
    new_sub_idx = sub_count
    tmp = os.path.join(BUILD, f"with_{codec}.mkv")
    # -map 0 会把原有的流全部拷贝，-map 0:{pgs_idx} 会把PGS作为新的输入流映射进来供转码
    r = run_ffmpeg([
        "-i", OUTPUT, "-map", "0", "-map", f"0:{pgs_idx}",
        "-c", "copy", f"-c:s:{new_sub_idx}", codec,
        f"-metadata:s:s:{new_sub_idx}", "language=chi",
        tmp,
    ], check=False)
    if r.returncode == 0 and os.path.exists(tmp) and os.path.getsize(tmp) > 0:
        os.replace(tmp, OUTPUT)
        return ("✅ 成功", codec_id, "")
    else:
        err = _extract_err(r.stderr)
        if os.path.exists(tmp):
            os.remove(tmp)
        return ("❌ 失败", codec_id, err)

def print_summary(text_results, bitmap_results):
    print("\n" + "=" * 95)
    print("字幕流生成结果汇总")
    print("=" * 95)
    print(f"{'字幕格式':<20} {'FFmpeg codec_id':<35} {'结果':<10} {'备注'}")
    print("-" * 95)
    for name, codec_id, status in [
        ("SRT", "AV_CODEC_ID_SUBRIP", text_results.get("srt", "❌ 失败")),
        ("ASS/SSA", "AV_CODEC_ID_ASS", text_results.get("ass", "❌ 失败")),
        ("WebVTT", "AV_CODEC_ID_WEBVTT", text_results.get("webvtt", "❌ 失败")),
    ]:
        print(f"{name:<20} {codec_id:<35} {status}")
    for name, (status, codec_id, err) in bitmap_results.items():
        note = f"原因: {err}" if "失败" in status else ""
        print(f"{name:<20} {codec_id:<35} {status} {note}")
    print("=" * 95)

async def main():
    os.makedirs(BUILD, exist_ok=True)
    print("[1/6] Checking dependencies...")
    ensure_deps()
    print("[2/6] Generating images (ZH + EN)...")
    make_images()
    print("[3/6] Generating subtitle files (SRT, ASS, WebVTT)...")
    make_srt()
    make_ass()
    make_vtt()
    print("[4/6] Generating TTS audio (ZH + EN)...")
    await make_audio()
    print("[5/6] Building MKV: 2 video + 2 audio + 3 text subs...")
    zh_vid = _build_video_from_images("zh")
    en_vid = _build_video_from_images("en")
    zh_aud = _build_audio_from_wavs("zh")
    en_aud = _build_audio_from_wavs("en")
    text_results = {}
    try:
        build_mkv_with_text_subs(zh_vid, en_vid, zh_aud, en_aud)
        text_results = {"srt": "✅ 成功", "ass": "✅ 成功", "webvtt": "✅ 成功"}
    except RuntimeError as e:
        print(f"Text subtitle muxing failed: {e}", file=sys.stderr)
        text_results = {"srt": "❌ 失败", "ass": "❌ 失败", "webvtt": "❌ 失败"}

    print("[6/6] Adding bitmap subtitle streams...")
    bitmap_results = {}
    try:
        sup_path = make_pgs()
        tmp = os.path.join(BUILD, "with_pgs.mkv")
        r = run_ffmpeg([
            "-i", OUTPUT, "-i", sup_path,
            "-map", "0", "-map", "1:s",
            "-c", "copy",
            "-metadata:s:s:3", "language=chi",  # 修复: PGS为第4个字幕流(索引3)
            tmp,
        ], check=False)
        if r.returncode == 0 and os.path.exists(tmp) and os.path.getsize(tmp) > 0:
            os.replace(tmp, OUTPUT)
            bitmap_results["PGS (Blu-ray)"] = ("✅ 成功", "AV_CODEC_ID_HDMV_PGS_SUBTITLE", "")
        else:
            bitmap_results["PGS (Blu-ray)"] = ("❌ 失败", "AV_CODEC_ID_HDMV_PGS_SUBTITLE", _extract_err(r.stderr))
            if os.path.exists(tmp):
                os.remove(tmp)
    except Exception as e:
        bitmap_results["PGS (Blu-ray)"] = ("❌ 失败", "AV_CODEC_ID_HDMV_PGS_SUBTITLE", str(e))

    for name, codec, codec_id in [
        ("VOBSub (DVD)", "dvdsub", "AV_CODEC_ID_DVD_SUBTITLE"),
        ("DVB (数字电视)", "dvbsub", "AV_CODEC_ID_DVB_SUBTITLE"),
        ("XSUB (AVI)", "xsub", "AV_CODEC_ID_XSUB"),
    ]:
        print(f"  Trying {name}...")
        bitmap_results[name] = try_add_bitmap_from_pgs(name, codec, codec_id)

    total_dur = len(NUMBERS_ZH) * DURATION

    _fix_mkv_duration(OUTPUT, total_dur)

    print_summary(text_results, bitmap_results)
    r = subprocess.run(
        ["ffprobe", "-v", "quiet", "-show_entries", "stream=index,codec_type,codec_name", "-of", "csv=p=0", OUTPUT],
        capture_output=True, text=True,
    )
    print(f"\nFinal streams in {OUTPUT}:")
    for line in r.stdout.strip().split("\n"):
        parts = line.strip().split(",")
        if len(parts) == 3:
            idx, cname, ctype = parts
            print(f"  Stream #{idx}: {ctype} ({cname})")
    print(f"\nOutput: {os.path.abspath(OUTPUT)}")

if __name__ == "__main__":
    asyncio.run(main())
