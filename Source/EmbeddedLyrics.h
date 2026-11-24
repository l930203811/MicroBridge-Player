#pragma once

// 音频文件内嵌歌词 / 基础标签提取（纯标准 C++，无 JUCE / 第三方依赖）。
//
// 支持容器（按魔数识别，不依赖扩展名）：
//   - ID3v2.2/2.3/2.4：USLT/ULT 文本歌词、SYLT/SLT 同步歌词（转成 LRC）
//   - FLAC / OGG / Opus：Vorbis Comment 里的 LYRICS / UNSYNCEDLYRICS
//   - MP4 / M4A：moov -> udta -> meta -> ilst -> (c)lyr
//   - APEv2 尾标签：Lyrics / Artist / Title
//   - WAV 内嵌 ID3 chunk
// 只读取标签区字节（ID3 <=8MB、OGG 头部 512KB、APE 尾部），不整读音频数据。
//
// 路径按 UTF-8 传入；Windows 下内部转 UTF-16 打开，中文路径可用。

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
#endif

class EmbeddedLyrics
{
public:
    struct Tags
    {
        std::string title;
        std::string artist;
        std::string albumArtist;   // 独立槽，不顶掉 artist
    };

    // 返回内嵌歌词文本（LRC 或毫秒制逐字格式）；无可用歌词返回空串。
    // 多个候选时优先返回含时间戳的同步歌词。
    static std::string extractLyrics (const std::string& utf8Path)
    {
        Reader r;
        if (! r.open (utf8Path)) return {};
        Tags ignored;
        const auto texts = collect (r, &ignored);
        for (const auto& t : texts)
            if (hasTimestamps (t)) return t;
        return {};
    }

    // 读取标题 / 艺术家（供元数据缺失时回退显示，当前 UI 暂未接入）。
    static Tags readTags (const std::string& utf8Path)
    {
        Reader r;
        Tags tags;
        if (r.open (utf8Path)) collect (r, &tags);
        return tags;
    }

    // 诊断：排查「内嵌歌词为什么没出来」用，可按需接入日志。
    static std::string diagnose (const std::string& utf8Path)
    {
        Reader r;
        if (! r.open (utf8Path)) return "无法打开文件";
        Tags tags;
        const auto texts = collect (r, &tags);
        std::string s = "候选=" + std::to_string (texts.size());
        for (const auto& t : texts)
            s += " | len=" + std::to_string (t.size())
               + " 时间戳=" + (hasTimestamps (t) ? "true" : "false")
               + " 预览=" + t.substr (0, 40);
        if (! tags.artist.empty()) s += " | artist=" + tags.artist;
        return s;
    }

private:
    // ────────────────────────── 随机读 ──────────────────────────
    class Reader
    {
    public:
        Reader() = default;
        ~Reader() { if (f != nullptr) std::fclose (f); }
        Reader (const Reader&) = delete;
        Reader& operator= (const Reader&) = delete;

        bool open (const std::string& path)
        {
        #ifdef _WIN32
            const int wlen = MultiByteToWideChar (CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
            if (wlen <= 0) return false;
            std::wstring w ((size_t) wlen, L'\0');
            MultiByteToWideChar (CP_UTF8, 0, path.c_str(), -1, w.data(), wlen);
        #ifdef _MSC_VER
            if (_wfopen_s (&f, w.c_str(), L"rb") != 0) f = nullptr;
        #else
            f = _wfopen (w.c_str(), L"rb");
        #endif
        #else
            f = std::fopen (path.c_str(), "rb");
        #endif
            if (f == nullptr) return false;
        #ifdef _WIN32
            if (_fseeki64 (f, 0, SEEK_END) != 0) return false;
            size_ = (uint64_t) _ftelli64 (f);
        #else
            if (std::fseek (f, 0, SEEK_END) != 0) return false;
            size_ = (uint64_t) std::ftell (f);
        #endif
            return size_ >= 16;
        }

        uint64_t size() const { return size_; }

        std::vector<uint8_t> at (uint64_t pos, size_t count) const
        {
            std::vector<uint8_t> out;
            if (f == nullptr || pos >= size_) return out;
            const size_t n = (size_t) std::min<uint64_t> (size_ - pos, count);
            if (n == 0) return out;
        #ifdef _WIN32
            if (_fseeki64 (f, (__int64) pos, SEEK_SET) != 0) return out;
        #else
            if (std::fseek (f, (long) pos, SEEK_SET) != 0) return out;
        #endif
            out.resize (n);
            const size_t got = std::fread (out.data(), 1, n, f);
            out.resize (got);
            return out;
        }

    private:
        std::FILE* f = nullptr;
        uint64_t size_ = 0;
    };

    // ────────────────────────── 字节工具 ──────────────────────────
    static uint32_t be32 (const uint8_t* b)
    {
        return (uint32_t (b[0]) << 24) | (uint32_t (b[1]) << 16)
             | (uint32_t (b[2]) << 8)  | uint32_t (b[3]);
    }
    static uint32_t le32 (const uint8_t* b)
    {
        return uint32_t (b[0]) | (uint32_t (b[1]) << 8)
             | (uint32_t (b[2]) << 16) | (uint32_t (b[3]) << 24);
    }
    static uint32_t synchsafe (const uint8_t* b)
    {
        return (uint32_t (b[0]) << 21) | (uint32_t (b[1]) << 14)
             | (uint32_t (b[2]) << 7)  | uint32_t (b[3]);
    }
    static bool eq (const std::vector<uint8_t>& v, size_t off, const char* s, size_t n)
    {
        return off + n <= v.size() && std::memcmp (v.data() + off, s, n) == 0;
    }
    static bool startsWith (const std::vector<uint8_t>& v, const char* s, size_t n)
    {
        return eq (v, 0, s, n);
    }
    static std::string upper (std::string s)
    {
        for (auto& c : s)
            if (c >= 'a' && c <= 'z') c = char (c - 'a' + 'A');
        return s;
    }
    static std::string clean (std::string s)
    {
        while (! s.empty() && (s.back() == '\0' || s.back() == ' ' || s.back() == '\t'
                               || s.back() == '\r' || s.back() == '\n'))
            s.pop_back();
        size_t b = 0;
        while (b < s.size() && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r'
                                || s[b] == '\n' || s[b] == '\0')) ++b;
        return b == 0 ? s : s.substr (b);
    }

    // UTF-8 合法性粗检（ID3 enc=0 常被中文工具违规写入 UTF-8）
    static bool looksUtf8 (const uint8_t* p, size_t n)
    {
        size_t i = 0;
        while (i < n)
        {
            const uint8_t c = p[i];
            if (c < 0x80) { ++i; continue; }
            size_t need = 0;
            if      ((c & 0xE0) == 0xC0) need = 1;
            else if ((c & 0xF0) == 0xE0) need = 2;
            else if ((c & 0xF8) == 0xF0) need = 3;
            else return false;
            if (i + need >= n) return false;
            for (size_t k = 1; k <= need; ++k)
                if ((p[i + k] & 0xC0) != 0x80) return false;
            i += need + 1;
        }
        return true;
    }

    static void appendUtf8 (std::string& out, uint32_t cp)
    {
        if (cp < 0x80) out.push_back (char (cp));
        else if (cp < 0x800)
        {
            out.push_back (char (0xC0 | (cp >> 6)));
            out.push_back (char (0x80 | (cp & 0x3F)));
        }
        else if (cp < 0x10000)
        {
            out.push_back (char (0xE0 | (cp >> 12)));
            out.push_back (char (0x80 | ((cp >> 6) & 0x3F)));
            out.push_back (char (0x80 | (cp & 0x3F)));
        }
        else
        {
            out.push_back (char (0xF0 | (cp >> 18)));
            out.push_back (char (0x80 | ((cp >> 12) & 0x3F)));
            out.push_back (char (0x80 | ((cp >> 6) & 0x3F)));
            out.push_back (char (0x80 | (cp & 0x3F)));
        }
    }

    static std::string decodeText (const uint8_t* p, size_t n, int enc)
    {
        if (p == nullptr || n == 0) return {};
        if (enc == 3) return clean (std::string ((const char*) p, n)); // UTF-8
        if (enc == 0)                                                  // ISO-8859-1 / 违规 UTF-8
        {
            if (looksUtf8 (p, n)) return clean (std::string ((const char*) p, n));
            std::string out;
            for (size_t i = 0; i < n; ++i)
                if (p[i] != 0) appendUtf8 (out, p[i]);
            return clean (out);
        }
        // UTF-16（enc=1 带 BOM，enc=2 强制 BE）
        bool little = false;
        size_t i = 0;
        if (enc == 1 && n >= 2 && p[0] == 0xFF && p[1] == 0xFE) { little = true;  i = 2; }
        else if (enc == 1 && n >= 2 && p[0] == 0xFE && p[1] == 0xFF) { little = false; i = 2; }
        else if (enc == 1) little = true; // 无 BOM 时按 LE 兜底

        std::string out;
        auto unit = [&] (size_t k) -> uint32_t
        {
            return little ? uint32_t (p[k] | (uint32_t (p[k + 1]) << 8))
                          : uint32_t (uint32_t (p[k]) << 8 | p[k + 1]);
        };
        while (i + 1 < n)
        {
            const uint32_t u = unit (i);
            i += 2;
            if (u == 0) break;
            uint32_t cp = u;
            if (u >= 0xD800 && u <= 0xDBFF && i + 1 < n)
            {
                const uint32_t lo = unit (i);
                if (lo >= 0xDC00 && lo <= 0xDFFF)
                {
                    cp = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                    i += 2;
                }
            }
            appendUtf8 (out, cp);
        }
        return clean (out);
    }

    // 返回「描述符结束、正文开始」的下标
    static size_t skipDescriptor (const std::vector<uint8_t>& b, size_t from, int enc)
    {
        if (from >= b.size()) return b.size();
        if (enc == 1 || enc == 2)
        {
            for (size_t i = from; i + 1 < b.size(); i += 2)
                if (b[i] == 0 && b[i + 1] == 0) return i + 2;
            return b.size();
        }
        for (size_t i = from; i < b.size(); ++i)
            if (b[i] == 0) return i + 1;
        return b.size();
    }

    static std::vector<uint8_t> deunsync (const std::vector<uint8_t>& in)
    {
        std::vector<uint8_t> out;
        out.reserve (in.size());
        for (size_t i = 0; i < in.size(); ++i)
        {
            out.push_back (in[i]);
            if (in[i] == 0xFF && i + 1 < in.size() && in[i + 1] == 0x00) ++i;
        }
        return out;
    }

    static std::string msStamp (uint32_t ms)
    {
        char buf[32];
        std::snprintf (buf, sizeof (buf), "[%02u:%02u.%03u]",
                       ms / 60000, (ms % 60000) / 1000, ms % 1000);
        return std::string (buf);
    }

    // 至少两个行首时间戳才算同步歌词，避免把 "[" 开头的普通文本误判
    static bool hasTimestamps (const std::string& s)
    {
        int colon = 0, comma = 0;
        for (size_t i = 0; i + 1 < s.size(); ++i)
        {
            if (s[i] != '[') continue;
            size_t j = i + 1;
            bool digits = true, hasComma = false, hasColon = false;
            while (j < s.size() && s[j] != ']' && j - i < 20)
            {
                const char c = s[j];
                if (c >= '0' && c <= '9') { /* ok */ }
                else if (c == ',') hasComma = true;
                else if (c == ':' || c == '.') hasColon = true;
                else { digits = false; break; }
                ++j;
            }
            if (digits && j < s.size() && s[j] == ']')
            {
                if (hasComma) ++comma;
                else if (hasColon) ++colon;
            }
            i = j;
        }
        return colon >= 2 || comma >= 2;
    }

    static void putTag (Tags* tags, const std::string& key, const std::string& val)
    {
        if (tags == nullptr) return;
        const auto k = upper (key);
        const auto v = clean (val);
        if (v.empty()) return;
        if      (k == "TITLE")       tags->title = v;
        else if (k == "ARTIST")      tags->artist = v;
        else if (k == "ALBUMARTIST") tags->albumArtist = v;
    }

    // ────────────────────────── 总入口 ──────────────────────────
    static std::vector<std::string> collect (Reader& r, Tags* tags)
    {
        std::vector<std::string> texts;
        // 文件头 ID3；同时算出真实音频头位置（部分 FLAC/OGG 被塞了 ID3 前缀）
        readId3 (r, 0, texts, tags);
        const uint64_t start = locateAudioStart (r, id3TagEnd (r, 0));

        const auto head = r.at (start, 64);
        if (startsWith (head, "fLaC", 4))
            readFlac (r, start, texts, tags);
        else if (startsWith (head, "OggS", 4))
            readOgg (r, start, texts, tags);
        else if (head.size() >= 8 && eq (head, 4, "ftyp", 4))
            readMp4 (r, start, texts, tags);
        else if (startsWith (head, "RIFF", 4))
            readWavId3 (r, texts, tags);

        // APEv2 尾标签（MP3 / Monkey's Audio 常见，也可补基础标签）
        readApe (r, texts, tags);
        return texts;
    }

    static bool isKnownMagic (const std::vector<uint8_t>& h)
    {
        if (startsWith (h, "fLaC", 4)) return true;
        if (startsWith (h, "OggS", 4)) return true;
        if (startsWith (h, "RIFF", 4)) return true;
        return h.size() >= 8 && eq (h, 4, "ftyp", 4);
    }

    // ID3 tag size 不准 / 标签后带额外 padding 时，在小窗口内重新定位音频魔数。
    static uint64_t locateAudioStart (const Reader& r, uint64_t start)
    {
        if (isKnownMagic (r.at (start, 64))) return start;
        const auto scan = r.at (start, 4u << 10);
        for (size_t i = 0; i + 8 <= scan.size(); ++i)
        {
            std::vector<uint8_t> h (scan.begin() + (long) i,
                                    scan.begin() + (long) (i + 8));
            if (isKnownMagic (h)) return start + i;
        }
        return start;
    }

    static uint64_t id3TagEnd (const Reader& r, uint64_t off)
    {
        const auto h = r.at (off, 10);
        if (h.size() < 10 || ! eq (h, 0, "ID3", 3) || h[3] > 4) return off;
        uint64_t end = off + 10 + synchsafe (h.data() + 6);
        if (h[5] & 0x10) end += 10;   // footer
        return std::min (end, r.size());
    }

    // ────────────────────────── ID3v2 ──────────────────────────
    static void readId3 (const Reader& r, uint64_t off,
                         std::vector<std::string>& texts, Tags* tags)
    {
        const auto h = r.at (off, 10);
        if (h.size() < 10 || ! eq (h, 0, "ID3", 3) || h[3] > 4) return;
        const int ver = h[3];
        const uint32_t tagSize = synchsafe (h.data() + 6);
        if (tagSize == 0 || tagSize > (8u << 20)) return;

        auto body = r.at (off + 10, tagSize);
        if (body.size() < tagSize) return;
        if ((h[5] & 0x80) != 0 && ver <= 3) body = deunsync (body); // v2.3 全局反同步

        const size_t headerSize = (ver == 2) ? 6 : 10;
        size_t p = 0;
        while (p + headerSize <= body.size())
        {
            std::string id;
            uint32_t frameSize = 0;
            uint8_t flags = 0;
            if (ver == 2)
            {
                id.assign ((const char*) body.data() + p, 3);
                frameSize = (uint32_t (body[p + 3]) << 16)
                          | (uint32_t (body[p + 4]) << 8) | body[p + 5];
            }
            else
            {
                id.assign ((const char*) body.data() + p, 4);
                frameSize = (ver == 4) ? synchsafe (body.data() + p + 4)
                                       : be32 (body.data() + p + 4);
                flags = body[p + 9];
            }
            p += headerSize;
            if (id.empty() || id[0] == '\0') break;    // padding
            if (frameSize == 0 || frameSize > body.size() - p) break;

            std::vector<uint8_t> c (body.begin() + (long) p,
                                    body.begin() + (long) (p + frameSize));
            p += frameSize;
            if (ver == 4 && (flags & 0x02) != 0) c = deunsync (c); // 帧级反同步
            if (ver == 4 && (flags & 0x01) != 0 && c.size() >= 4)
                c.erase (c.begin(), c.begin() + 4);                  // data length indicator
            if (c.empty()) continue;

            const bool isUslt  = (id == "USLT" || id == "ULT");
            const bool isSylt  = (id == "SYLT" || id == "SLT");
            const bool isArtist = (id == "TPE1" || id == "TP1");
            const bool isTitle  = (id == "TIT2" || id == "TT2");

            if (isUslt || isSylt || isArtist || isTitle)
            {
                const int enc = c[0];
                if (isUslt || isArtist || isTitle)
                {
                    const size_t from = isUslt ? skipDescriptor (c, 4, enc) : 1;
                    const size_t at = std::min (from, c.size());
                    const auto txt = decodeText (c.data() + at, c.size() - at, enc);
                    if (isUslt) { if (! txt.empty()) texts.push_back (txt); }
                    else putTag (tags, isArtist ? "ARTIST" : "TITLE", txt);
                }
                else
                {
                    // SYLT：enc(1)+lang(3)+format(1)+type(1)+desc(null)+[text\0 + 4B时间]...
                    const size_t from = skipDescriptor (c, 6, enc);
                    std::string lrc;
                    size_t q = from;
                    while (q < c.size())
                    {
                        size_t term = q;
                        if (enc == 1 || enc == 2)
                        {
                            while (term + 1 < c.size() && !(c[term] == 0 && c[term + 1] == 0)) term += 2;
                            term += 2;
                        }
                        else
                        {
                            while (term < c.size() && c[term] != 0) ++term;
                            ++term;
                        }
                        if (term > c.size() || term + 4 > c.size()) break;
                        const auto txt = decodeText (c.data() + q, term - q - 1, enc);
                        const uint32_t ts = be32 (c.data() + term);
                        lrc += msStamp (ts) + txt + "\n";
                        q = term + 4;
                    }
                    if (! lrc.empty()) texts.push_back (lrc);
                }
            }
        }
    }

    // ────────────────────────── Vorbis Comment ──────────────────────────
    static void parseVorbis (const std::vector<uint8_t>& b, size_t start,
                             std::vector<std::string>& texts, Tags* tags)
    {
        if (b.size() < start + 8) return;
        const uint32_t vendorLen = le32 (b.data() + start);
        size_t p = start + 4 + vendorLen;
        if (p + 4 > b.size()) return;
        const uint32_t count = le32 (b.data() + p);
        p += 4;
        if (count > 4096) return;

        for (uint32_t i = 0; i < count && p + 4 <= b.size(); ++i)
        {
            const uint32_t l = le32 (b.data() + p);
            p += 4;
            if (l > b.size() - p) break;
            std::string entry ((const char*) b.data() + p, l);
            p += l;
            const auto e = entry.find ('=');
            if (e == std::string::npos) continue;
            const auto key = entry.substr (0, e);
            const auto val = entry.substr (e + 1);
            const auto k = upper (key);
            if (k.find ("LYRICS") != std::string::npos)
            {
                if (! val.empty()) texts.push_back (val);
            }
            else if (k == "TITLE" || k == "ARTIST" || k == "ALBUMARTIST")
            {
                putTag (tags, k, val);
            }
        }
    }

    static void readFlac (const Reader& r, uint64_t start,
                          std::vector<std::string>& texts, Tags* tags)
    {
        uint64_t pos = start + 4;
        for (int guard = 0; guard < 64; ++guard)
        {
            const auto h = r.at (pos, 4);
            if (h.size() < 4) return;
            const bool last = (h[0] & 0x80) != 0;
            const int type = h[0] & 0x7F;
            const uint32_t sz = (uint32_t (h[1]) << 16) | (uint32_t (h[2]) << 8) | h[3];
            if (type == 4)
            {
                const auto blk = r.at (pos + 4, sz);
                if (blk.size() >= sz) parseVorbis (blk, 0, texts, tags);
                return;
            }
            if (last || sz > (16u << 20)) return;
            pos += 4 + sz;
        }
    }

    static void readOgg (const Reader& r, uint64_t start,
                         std::vector<std::string>& texts, Tags* tags)
    {
        const auto scan = r.at (start, 512u << 10);
        for (size_t i = 0; i + 8 <= scan.size(); ++i)
        {
            size_t cs = 0;
            if (scan[i] == 0x03 && eq (scan, i + 1, "vorbis", 6)) cs = i + 7;
            else if (eq (scan, i, "OpusTags", 8)) cs = i + 8;
            if (cs != 0) { parseVorbis (scan, cs, texts, tags); return; }
        }
    }

    // ────────────────────────── MP4 / M4A ──────────────────────────
    struct Atom { int64_t pos = -1; uint64_t size = 0; };

    static Atom findAtom (const Reader& r, uint64_t begin, uint64_t end, const char* name)
    {
        uint64_t p = begin;
        for (int guard = 0; guard < 1024 && p + 8 <= end; ++guard)
        {
            const auto h = r.at (p, 16);
            if (h.size() < 8) break;
            uint64_t sz = be32 (h.data());
            if (sz == 1 && h.size() >= 16)                 // 64 位扩展长度
                sz = (uint64_t (be32 (h.data() + 8)) << 32) | be32 (h.data() + 12);
            else if (sz == 0) sz = end - p;                // 到容器末尾
            if (sz < 8 || p + sz > end) break;
            if (std::memcmp (h.data() + 4, name, 4) == 0) return { (int64_t) p, sz };
            p += sz;
        }
        return {};
    }

    static void readMp4Item (const Reader& r, const Atom& item,
                             std::vector<std::string>& texts, Tags* tags, bool isLyrics)
    {
        if (item.pos < 0 || item.size < 24) return;
        const auto dataHead = r.at ((uint64_t) item.pos + 8, 16);
        if (dataHead.size() < 16 || ! eq (dataHead, 4, "data", 4)) return;
        const size_t textLen = (size_t) (item.size - 24);
        if (textLen == 0 || textLen > (4u << 20)) return;
        const auto bytes = r.at ((uint64_t) item.pos + 24, textLen);
        if (bytes.size() < textLen) return;
        const auto txt = clean (std::string ((const char*) bytes.data(), bytes.size()));
        if (txt.empty()) return;
        if (isLyrics) texts.push_back (txt);
        else putTag (tags, "TITLE", txt);
    }

    static void readMp4 (const Reader& r, uint64_t start,
                         std::vector<std::string>& texts, Tags* tags)
    {
        const uint64_t len = r.size();
        const auto moov = findAtom (r, start, len, "moov");   // moov 也可能在文件尾
        if (moov.pos < 0) return;
        const uint64_t mEnd = (uint64_t) moov.pos + moov.size;
        const auto udta = findAtom (r, (uint64_t) moov.pos + 8, mEnd, "udta");
        if (udta.pos < 0) return;
        const uint64_t uEnd = (uint64_t) udta.pos + udta.size;
        const auto meta = findAtom (r, (uint64_t) udta.pos + 8, uEnd, "meta");
        if (meta.pos < 0) return;
        const uint64_t tEnd = (uint64_t) meta.pos + meta.size;
        // meta 原子头后还有 4 字节 version/flags
        const auto ilst = findAtom (r, (uint64_t) meta.pos + 12, tEnd, "ilst");
        if (ilst.pos < 0) return;
        const uint64_t iEnd = (uint64_t) ilst.pos + ilst.size;
        const uint64_t iBeg = (uint64_t) ilst.pos + 8;

        const char lyrName[4] = { (char) 0xA9, 'l', 'y', 'r' };
        const auto lyr = findAtom (r, iBeg, iEnd, lyrName);
        if (lyr.pos >= 0) readMp4Item (r, lyr, texts, tags, true);

        const char namName[4] = { (char) 0xA9, 'n', 'a', 'm' };
        const auto nam = findAtom (r, iBeg, iEnd, namName);
        if (nam.pos >= 0) readMp4Item (r, nam, texts, tags, false);
    }

    // ────────────────────────── APEv2 ──────────────────────────
    static void readApe (const Reader& r, std::vector<std::string>& texts, Tags* tags)
    {
        const uint64_t len = r.size();
        if (len < 32) return;
        const auto foot = r.at (len - 32, 32);
        if (foot.size() < 32 || ! eq (foot, 0, "APETAGEX", 8)) return;
        const uint32_t tagSize = le32 (foot.data() + 12);
        const uint32_t count   = le32 (foot.data() + 16);
        if (tagSize == 0 || tagSize > (4u << 20) || tagSize > len || count > 4096) return;
        const auto items = r.at (len - tagSize, tagSize);
        if (items.size() < tagSize) return;

        size_t p = eq (items, 0, "APETAGEX", 8) ? 32 : 0;   // 有头则跳过
        for (uint32_t i = 0; i < count && p + 8 <= items.size(); ++i)
        {
            const uint32_t valSize = le32 (items.data() + p); p += 4;
            const uint32_t flags   = le32 (items.data() + p); p += 4;
            const size_t ks = p;
            while (p < items.size() && items[p] != 0) ++p;
            std::string key ((const char*) items.data() + ks, p - ks);
            if (p < items.size()) ++p;
            if (valSize > items.size() - p) break;
            const bool isText = (flags & 0x6) == 0;
            const auto k = upper (key);
            if (isText && (k.find ("LYRICS") != std::string::npos
                           || k == "ARTIST" || k == "TITLE"))
            {
                const auto val = clean (std::string ((const char*) items.data() + p, valSize));
                if (k.find ("LYRICS") != std::string::npos) { if (! val.empty()) texts.push_back (val); }
                else putTag (tags, k, val);
            }
            p += valSize;
        }
    }

    // ────────────────────────── WAV 内嵌 ID3 ──────────────────────────
    static void readWavId3 (const Reader& r, std::vector<std::string>& texts, Tags* tags)
    {
        const auto scan = r.at (0, 1u << 20);
        for (size_t i = 12; i + 10 <= scan.size(); ++i)
            if (eq (scan, i, "ID3", 3) && scan[i + 3] <= 4)
                readId3 (r, i, texts, tags);
    }
};
