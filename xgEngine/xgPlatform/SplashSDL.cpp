// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgSplash.h"
#include "xgEnv.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace xg
{
    namespace
    {
        struct Rgb
        {
            uint8_t R, G, B;
        };

        constexpr Rgb WindowBg   = { 29, 29, 29 };
        constexpr Rgb LogoBg     = { 21, 21, 21 };
        constexpr Rgb Divider    = { 42, 42, 42 };
        constexpr Rgb Outline    = { 58, 58, 58 };
        constexpr Rgb ChipBg     = { 37, 37, 37 };
        constexpr Rgb BarTrack   = { 46, 46, 46 };
        constexpr Rgb Accent     = { 71, 114, 179 };
        constexpr Rgb TextBright = { 230, 230, 230 };
        constexpr Rgb TextDim    = { 168, 168, 168 };
        constexpr Rgb TextFaint  = { 138, 138, 138 };
        constexpr Rgb TextCorner = { 92, 92, 92 };

        constexpr float SplashWidth = 600.0f;
        constexpr float SplashHeight = 340.0f;
        constexpr float LogoPanelWidth = 228.0f;
        constexpr float LogoSize = 132.0f;
        constexpr float Pad = 26.0f;
        constexpr const char* Ellipsis = "\xE2\x80\xA6";

        uint32_t DecodeUTF8(const char*& p)
        {
            const uint8_t c = static_cast<uint8_t>(*p++);
            if (c < 0x80)
                return c;
            int extra = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : 0;
            uint32_t cp = c & (0x3F >> extra);
            for (; extra > 0 && (static_cast<uint8_t>(*p) & 0xC0) == 0x80; --extra)
                cp = (cp << 6) | (static_cast<uint8_t>(*p++) & 0x3F);
            return extra == 0 ? cp : 0xFFFD;
        }

        bool EndsWith(const std::string& text, const char* suffix)
        {
            const size_t length = std::strlen(suffix);
            return text.size() >= length && text.compare(text.size() - length, length, suffix) == 0;
        }

        class FontFace
        {
        public:
            FontFace() = default;
            FontFace(const FontFace&) = delete;
            FontFace& operator=(const FontFace&) = delete;

            ~FontFace()
            {
                if (_face)
                    FT_Done_Face(_face);
                if (_data)
                    SDL_free(_data);
            }

            bool Load(FT_Library library, const std::string& directory, const char* primary, const char* fallback)
            {
                for (const char* name : { primary, fallback })
                {
                    size_t size = 0;
                    void* data = SDL_LoadFile((directory + "/" + name).c_str(), &size);
                    if (!data)
                        continue;
                    FT_Face face = nullptr;
                    if (FT_New_Memory_Face(library, static_cast<const FT_Byte*>(data), static_cast<FT_Long>(size), 0, &face) == 0)
                    {
                        if (name == fallback)
                            std::fprintf(stderr, "Splash: %s not found, using %s\n", primary, fallback);
                        _face = face;
                        _data = data;
                        return true;
                    }
                    SDL_free(data);
                }
                std::fprintf(stderr, "Splash: font %s not found in %s\n", primary, directory.c_str());
                return false;
            }

            FT_Face Get() const { return _face; }

        private:
            FT_Face _face = nullptr;
            void* _data = nullptr;
        };

        struct FreeTypeLibrary
        {
            ~FreeTypeLibrary()
            {
                if (Handle)
                    FT_Done_FreeType(Handle);
            }

            FT_Library Handle = nullptr;
        };

        struct TextImage
        {
            TextImage() = default;
            TextImage(const TextImage&) = delete;
            TextImage& operator=(const TextImage&) = delete;

            ~TextImage()
            {
                if (Texture)
                    SDL_DestroyTexture(Texture);
            }

            std::string Text;
            int PixelSize = 0;
            SDL_Texture* Texture = nullptr;
            int Width = 0;
            int Height = 0;
            int Ascent = 0;
            float Advance = 0.0f;
        };

        float MeasureText(FT_Face face, int pixelSize, const std::string& text, float tracking)
        {
            if (!face)
                return 0.0f;
            FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixelSize));
            float advance = 0.0f;
            bool first = true;
            for (const char* p = text.c_str(); *p;)
            {
                const uint32_t cp = DecodeUTF8(p);
                if (FT_Load_Char(face, cp, FT_LOAD_DEFAULT | FT_LOAD_TARGET_LIGHT) != 0)
                    continue;
                advance += static_cast<float>(face->glyph->advance.x) / 64.0f + (first ? 0.0f : tracking);
                first = false;
            }
            return advance;
        }

        void RenderText(SDL_Renderer* renderer, FT_Face face, int pixelSize, const std::string& text, float tracking, TextImage& image)
        {
            if (image.Texture)
                SDL_DestroyTexture(image.Texture);
            image.Texture = nullptr;
            image.Text = text;
            image.PixelSize = pixelSize;
            image.Width = image.Height = image.Ascent = 0;
            image.Advance = 0.0f;
            if (!face || text.empty())
                return;

            image.Advance = MeasureText(face, pixelSize, text, tracking);
            const int ascent = static_cast<int>(std::ceil(face->size->metrics.ascender / 64.0));
            const int descent = static_cast<int>(std::ceil(-face->size->metrics.descender / 64.0));
            const int margin = 2;
            const int width = static_cast<int>(std::ceil(image.Advance)) + margin * 2 + pixelSize / 4;
            const int height = ascent + descent;
            if (width <= 0 || height <= 0)
                return;

            SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
            if (!surface)
                return;
            SDL_FillSurfaceRect(surface, nullptr, SDL_MapSurfaceRGBA(surface, 255, 255, 255, 0));

            float pen = static_cast<float>(margin);
            bool first = true;
            for (const char* p = text.c_str(); *p;)
            {
                const uint32_t cp = DecodeUTF8(p);
                if (FT_Load_Char(face, cp, FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT) != 0)
                    continue;
                if (!first)
                    pen += tracking;
                first = false;
                const FT_GlyphSlot glyph = face->glyph;
                const FT_Bitmap& bitmap = glyph->bitmap;
                const int originX = static_cast<int>(std::lround(pen)) + glyph->bitmap_left;
                const int originY = ascent - glyph->bitmap_top;
                for (unsigned row = 0; row < bitmap.rows; ++row)
                {
                    const int y = originY + static_cast<int>(row);
                    if (y < 0 || y >= height)
                        continue;
                    uint8_t* line = static_cast<uint8_t*>(surface->pixels) + static_cast<size_t>(surface->pitch) * y;
                    const uint8_t* source = bitmap.buffer + static_cast<ptrdiff_t>(bitmap.pitch) * row;
                    for (unsigned column = 0; column < bitmap.width; ++column)
                    {
                        const int x = originX + static_cast<int>(column);
                        if (x < 0 || x >= width)
                            continue;
                        uint8_t& alpha = line[x * 4 + 3];
                        alpha = (std::max)(alpha, source[column]);
                    }
                }
                pen += static_cast<float>(glyph->advance.x) / 64.0f;
            }

            image.Texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_DestroySurface(surface);
            if (!image.Texture)
                return;
            SDL_SetTextureBlendMode(image.Texture, SDL_BLENDMODE_BLEND);
            image.Width = width;
            image.Height = height;
            image.Ascent = ascent;
        }
    }

    class SplashSDL final : public SplashScreen
    {
    public:
        ~SplashSDL() override
        {
            if (_logo)
                SDL_DestroyTexture(_logo);
            for (TextImage* image : AllTexts())
            {
                if (image->Texture)
                    SDL_DestroyTexture(image->Texture);
                image->Texture = nullptr;
            }
            if (_renderer)
                SDL_DestroyRenderer(_renderer);
            if (_window)
                SDL_DestroyWindow(_window);
            if (_videoInit)
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }

        bool Init(const SplashDesc& desc)
        {
            auto copy = [](const char* text) { return std::string(text ? text : ""); };
            _title = copy(desc.Title);
            _tagline = copy(desc.Tagline);
            for (const char* chip : { desc.Version, desc.Config, desc.Renderer })
            {
                if (chip && *chip)
                    _chips.emplace_back(chip);
            }
            _project = copy(desc.Project);
            _copyright = copy(desc.Copyright);
            _chipTexts = std::vector<TextImage>(_chips.size());

            if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
                return false;
            _videoInit = true;

            float scale = 1.0f;
            char forced[32] = {};
            if (xg::GetEnv("XG_UI_DPI_SCALE", forced, sizeof(forced)) && std::atof(forced) > 0.0)
                scale = static_cast<float>(std::atof(forced));
            else if (const SDL_DisplayID display = SDL_GetPrimaryDisplay())
                scale = (std::max)(SDL_GetDisplayContentScale(display), 1.0f);

            const int width = static_cast<int>(std::lround(SplashWidth * scale));
            const int height = static_cast<int>(std::lround(SplashHeight * scale));
            _window = SDL_CreateWindow(_title.c_str(), width, height,
                SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_UTILITY | SDL_WINDOW_ALWAYS_ON_TOP);
            if (!_window)
                return false;
            SDL_SetWindowPosition(_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

            _renderer = SDL_CreateRenderer(_window, nullptr);
            if (!_renderer)
                return false;
            SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
            int outputWidth = width;
            int outputHeight = height;
            SDL_GetRenderOutputSize(_renderer, &outputWidth, &outputHeight);
            _scale = static_cast<float>(outputWidth) / SplashWidth;

            if (desc.LogoPath)
            {
                if (SDL_Surface* surface = SDL_LoadSurface(desc.LogoPath))
                {
                    _logo = SDL_CreateTextureFromSurface(_renderer, surface);
                    SDL_DestroySurface(surface);
                    if (_logo)
                    {
                        SDL_SetTextureBlendMode(_logo, SDL_BLENDMODE_BLEND);
                        SDL_SetTextureScaleMode(_logo, SDL_SCALEMODE_LINEAR);
                    }
                }
                if (!_logo)
                    std::fprintf(stderr, "Splash: could not load %s\n", desc.LogoPath);
            }

            if (FT_Init_FreeType(&_library.Handle) == 0)
            {
                const std::string fonts = desc.FontDirectory ? desc.FontDirectory : "../editor/fonts";
                _regular.Load(_library.Handle, fonts, "NotoSans-Regular.ttf", "DejaVuSans.ttf");
                _bold.Load(_library.Handle, fonts, "NotoSans-Bold.ttf", "DejaVuSans-Bold.ttf");
                _mono.Load(_library.Handle, fonts, "NotoSansMono-Regular.ttf", "DejaVuSans.ttf");
            }

            Draw();
            SDL_ShowWindow(_window);
            Draw();
            return true;
        }

        void SetStep(const char* text, float progress) override
        {
            if (text)
                _step = text;
            _progress = progress;
            _dirty = true;
        }

        void SetDetail(const char* text) override
        {
            _detail = text ? text : "";
            _dirty = true;
        }

        void Pump() override
        {
            std::vector<SDL_Event> others;
            SDL_Event e;
            while (SDL_PollEvent(&e))
            {
                if (SDL_GetWindowFromEvent(&e) == _window)
                {
                    if (e.type == SDL_EVENT_WINDOW_EXPOSED)
                        _dirty = true;
                    continue;
                }
                others.push_back(e);
            }
            for (SDL_Event& other : others)
                SDL_PushEvent(&other);

            const uint64_t now = SDL_GetTicks();
            if (!_dirty && _progress >= 0.0f)
                return;
            if (now - _lastDraw < 16)
                return;
            Draw();
        }

    private:
        std::vector<TextImage*> AllTexts()
        {
            std::vector<TextImage*> images = { &_titleText, &_taglineText, &_projectLabelText, &_projectText, &_stepText,
                &_percentText, &_detailText, &_copyrightText };
            for (TextImage& chip : _chipTexts)
                images.push_back(&chip);
            return images;
        }

        float Px(float logical) const { return logical * _scale; }

        void Fill(float x, float y, float w, float h, Rgb color)
        {
            SDL_SetRenderDrawColor(_renderer, color.R, color.G, color.B, 255);
            const SDL_FRect rect = { std::floor(Px(x)), std::floor(Px(y)), std::ceil(Px(w)), std::ceil(Px(h)) };
            SDL_RenderFillRect(_renderer, &rect);
        }

        // Draws text with its baseline at y; maxWidth > 0 cuts the end with an ellipsis. Returns the advance in logical units.
        float Text(TextImage& image, FontFace& font, float size, std::string text, float x, float baseline, Rgb color,
            float maxWidth = 0.0f, float tracking = 0.0f, bool alignRight = false)
        {
            FT_Face face = font.Get();
            if (!face)
                return 0.0f;
            const int pixelSize = (std::max)(1, static_cast<int>(std::lround(Px(size))));
            const float pixelTracking = Px(tracking);
            if (maxWidth > 0.0f && !text.empty() && MeasureText(face, pixelSize, text, pixelTracking) > Px(maxWidth))
            {
                while (!text.empty() && MeasureText(face, pixelSize, text + Ellipsis, pixelTracking) > Px(maxWidth))
                {
                    size_t cut = text.size() - 1;
                    while (cut > 0 && (static_cast<uint8_t>(text[cut]) & 0xC0) == 0x80)
                        --cut;
                    text.erase(cut);
                }
                text += Ellipsis;
            }
            if (image.Text != text || image.PixelSize != pixelSize || (!image.Texture && !text.empty()))
                RenderText(_renderer, face, pixelSize, text, pixelTracking, image);
            if (!image.Texture)
                return 0.0f;

            const float left = alignRight ? Px(x) - image.Advance - 2.0f : Px(x) - 2.0f;
            const SDL_FRect rect = { std::round(left), std::round(Px(baseline)) - static_cast<float>(image.Ascent),
                static_cast<float>(image.Width), static_cast<float>(image.Height) };
            SDL_SetTextureColorMod(image.Texture, color.R, color.G, color.B);
            SDL_RenderTexture(_renderer, image.Texture, nullptr, &rect);
            return image.Advance / _scale;
        }

        void Draw()
        {
            _dirty = false;
            _lastDraw = SDL_GetTicks();

            SDL_SetRenderDrawColor(_renderer, WindowBg.R, WindowBg.G, WindowBg.B, 255);
            SDL_RenderClear(_renderer);

            Fill(0.0f, 0.0f, LogoPanelWidth, SplashHeight, LogoBg);
            Fill(LogoPanelWidth, 0.0f, 1.0f, SplashHeight, Divider);
            if (_logo)
            {
                const float x = (LogoPanelWidth - LogoSize) * 0.5f;
                const float y = (SplashHeight - LogoSize) * 0.5f;
                const SDL_FRect rect = { std::round(Px(x)), std::round(Px(y)), std::round(Px(LogoSize)), std::round(Px(LogoSize)) };
                SDL_RenderTexture(_renderer, _logo, nullptr, &rect);
            }
            Text(_copyrightText, _regular, 11.0f, _copyright, 10.0f, SplashHeight - 9.0f, TextCorner, LogoPanelWidth - 20.0f);

            const float left = LogoPanelWidth + Pad;
            const float right = SplashWidth - Pad;
            const float width = right - left;

            Text(_titleText, _bold, 46.0f, _title, left, 64.0f, TextBright, width, 4.0f);
            Text(_taglineText, _regular, 13.0f, _tagline, left, 88.0f, TextDim, width);

            float chipX = left;
            for (size_t i = 0; i < _chips.size(); ++i)
            {
                const float textWidth = MeasureText(_regular.Get(), (std::max)(1, static_cast<int>(std::lround(Px(11.0f)))), _chips[i], 0.0f) / _scale;
                const float chipWidth = textWidth + 12.0f;
                if (chipX + chipWidth > right)
                    break;
                Fill(chipX, 102.0f, chipWidth, 18.0f, Outline);
                Fill(chipX + 1.0f, 103.0f, chipWidth - 2.0f, 16.0f, ChipBg);
                Text(_chipTexts[i], _regular, 11.0f, _chips[i], chipX + 6.0f, 115.0f, TextBright);
                chipX += chipWidth + 6.0f;
            }

            if (!_project.empty())
            {
                const float labelWidth = Text(_projectLabelText, _regular, 12.0f, "Project", left, 146.0f, TextDim);
                Text(_projectText, _regular, 12.0f, _project, left + labelWidth + 5.0f, 146.0f, TextBright, width - labelWidth - 5.0f);
            }

            const float barY = SplashHeight - 24.0f - 11.0f - 9.0f;
            const bool determinate = _progress >= 0.0f;
            const float progress = determinate ? (std::min)(_progress, 1.0f) : 0.0f;
            std::string percent;
            if (determinate)
            {
                char buffer[16];
                std::snprintf(buffer, sizeof(buffer), "%d%%", static_cast<int>(std::lround(progress * 100.0f)));
                percent = buffer;
            }
            const float percentWidth = Text(_percentText, _regular, 12.0f, percent, right, barY - 9.0f, TextFaint, 0.0f, 0.0f, true);
            std::string step = _step;
            if (!step.empty() && progress < 1.0f && !EndsWith(step, Ellipsis) && !EndsWith(step, "."))
                step += Ellipsis;
            Text(_stepText, _regular, 12.0f, step, left, barY - 9.0f, TextBright, width - percentWidth - 12.0f);

            Fill(left, barY, width, 3.0f, BarTrack);
            if (determinate)
            {
                if (progress > 0.0f)
                    Fill(left, barY, width * progress, 3.0f, Accent);
            }
            else
            {
                const float segment = width * 0.3f;
                const float t = std::fmod(static_cast<float>(SDL_GetTicks()) / 1400.0f, 1.0f);
                const float start = -segment + t * (width + segment);
                const float from = (std::max)(start, 0.0f);
                const float to = (std::min)(start + segment, width);
                if (to > from)
                    Fill(left + from, barY, to - from, 3.0f, Accent);
            }

            Text(_detailText, _mono, 11.0f, _detail, left, SplashHeight - 24.0f, TextFaint, width);

            SDL_SetRenderDrawColor(_renderer, Outline.R, Outline.G, Outline.B, 255);
            const SDL_FRect border = { 0.0f, 0.0f, Px(SplashWidth), Px(SplashHeight) };
            SDL_RenderRect(_renderer, &border);

            SDL_RenderPresent(_renderer);
        }

        SDL_Window* _window = nullptr;
        SDL_Renderer* _renderer = nullptr;
        SDL_Texture* _logo = nullptr;
        bool _videoInit = false;
        float _scale = 1.0f;

        FreeTypeLibrary _library;
        FontFace _regular;
        FontFace _bold;
        FontFace _mono;

        std::string _title;
        std::string _tagline;
        std::vector<std::string> _chips;
        std::string _project;
        std::string _copyright;
        std::string _step;
        std::string _detail;
        float _progress = 0.0f;
        bool _dirty = true;
        uint64_t _lastDraw = 0;

        TextImage _titleText;
        TextImage _taglineText;
        std::vector<TextImage> _chipTexts;
        TextImage _projectLabelText;
        TextImage _projectText;
        TextImage _stepText;
        TextImage _percentText;
        TextImage _detailText;
        TextImage _copyrightText;
    };

    XG_DECLARE_UNIQUE_TYPE(SplashSDL)
    XG_API SplashScreenUnique NewSplashScreen(const SplashDesc& desc)
    {
        auto* splash = new SplashSDL();
        if (!splash->Init(desc))
        {
            delete splash;
            return SplashScreenUnique();
        }
        return SplashScreenUnique(MakeSplashSDLUniqueHandle(splash));
    }
}
