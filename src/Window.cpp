#include <iostream>
#include <cmath>
#include "Window.h"
#include "Http.h"
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iterator>

namespace {
    std::wstring widen(const std::string& s) {
        std::wstring wide;
        wide.reserve(s.size());
        for (char c : s) { wide.push_back(static_cast<wchar_t>(c)); }
        return wide;
    }

    // Splits an absolute https URL into (host, path).
    std::pair<std::wstring, std::wstring> splitAlbumUrl(const std::string& url) {
        const std::string scheme = "https://";
        size_t p = url.find(scheme);
        if (p == std::string::npos) { return {}; }

        std::string rest = url.substr(p + scheme.size());
        size_t slash = rest.find('/');
        std::string host = (slash == std::string::npos) ? rest : rest.substr(0, slash);
        std::string path = (slash == std::string::npos) ? "/" : rest.substr(slash);
        if (host.empty()) { return {}; }

        return { widen(host), widen(path) };
    }
}

Window::Window() {
    width = (buttonCount * buttonRadius * 2) + ((buttonCount - 1) * buttonOffset) + buttonCount;
    height = 200;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cout << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return;
    }

    window = SDL_CreateWindow("quickwheel", width, height, 
            SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_TRANSPARENT);
    if (!window) {
        std::cout << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return;
    }

    std::cout << "Window Created" << std::endl;

    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cout << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    std::cout << "Renderer Created" << std::endl;

    if (!TTF_Init()) {
        std::cout << "TTF_Init failed: " << SDL_GetError() << std::endl;
    }
    initFonts();
    loadIcons();

    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    HWND hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

    SetWindowLongPtr(hwnd, GWL_EXSTYLE, style | WS_EX_TOPMOST | WS_EX_NOACTIVATE);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

Window::~Window() {
    if (albumTexture) { SDL_DestroyTexture(albumTexture); }
    destroyIcons();
    destroyText();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}

void Window::show() {
    SDL_ShowWindow(window);
}

void Window::hide(Wheel wheel) {
    SDL_HideWindow(window);

    wheel.PerformAction(selectedAction);
}

void Window::setMonitor(MONITORINFO* monitor) {
    width = monitor->rcMonitor.right - monitor->rcMonitor.left;
    height = monitor->rcMonitor.bottom - monitor->rcMonitor.top;
    SDL_SetWindowSize(window, width, height);
    SDL_SetWindowPosition(window, monitor->rcMonitor.left, monitor->rcMonitor.top);
}

void Window::updateSelectedAction(POINT cursor) {
    int cx = cursor.x;
    int cy = cursor.y;

    int windowX, windowY;
    int gap = width / std::size(actions);
    SDL_GetWindowPosition(window, &windowX, &windowY);

    bool triggered = false;
    for (int i = 0; i < buttonCount; i++) {
        int bx = windowX + (i * gap);
        if (cx >= bx) {
            selectedAction = actions[i];
            triggered = true;
        }
    }

    if (!triggered) {
        selectedAction = actions[0];
    }
}

void Window::update(POINT cursor) {
    updateSelectedAction(cursor);
}

void Window::updateAlbumArt(const SpotifyTrack& track) {
    if (track.albumImageURL != lastAlbumURL) {
        loadAlbumImage(track.albumImageURL);
    }

    isPlaying = track.isPlaying;

    if (track.trackName != lastTitle || track.artists != lastArtists) {
        lastTitle = track.trackName;
        lastArtists = track.artists;
        rebuildText();
    }
}

void Window::initFonts() {
    static const char* titleCandidates[] = {
        "C:/Windows/Fonts/segoeuisb.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "C:/Windows/Fonts/arial.ttf"
    };
    static const char* artistCandidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf"
    };

    for (const char* path : titleCandidates) {
        titleFont = TTF_OpenFont(path, (float)titleSize);
        if (titleFont) { break; }
    }
    for (const char* path : artistCandidates) {
        artistFont = TTF_OpenFont(path, (float)artistSize);
        if (artistFont) { break; }
    }

    if (!titleFont || !artistFont) {
        std::cout << "Failed to open a system font: " << SDL_GetError() << std::endl;
    }

    initFallbackFonts();
}

void Window::initFallbackFonts() {
    // The primary fonts (Segoe UI / Arial) do not carry CJK glyphs. Attach
    // Windows fonts that cover Korean/Japanese so missing glyphs are pulled in.
    static const char* cjkCandidates[] = {
        "C:/Windows/Fonts/malgun.ttf",
        "C:/Windows/Fonts/meiryo.ttc",
        "C:/Windows/Fonts/msgothic.ttc",
        "C:/Windows/Fonts/malgunbd.ttf",
        "C:/Windows/Fonts/meiryob.ttc"
    };

    auto attach = [](struct TTF_Font* base, float size, std::vector<TTF_Font*>& store) {
        if (!base) { return; }
        for (const char* path : cjkCandidates) {
            struct TTF_Font* fb = TTF_OpenFont(path, size);
            if (!fb) { continue; }
            if (TTF_AddFallbackFont(base, fb)) {
                store.push_back(fb);
            } else {
                TTF_CloseFont(fb);
            }
        }
    };

    attach(titleFont, (float)titleSize, titleFontFallbacks);
    attach(artistFont, (float)artistSize, artistFontFallbacks);
}

void Window::destroyText() {
    if (titleTexture) { SDL_DestroyTexture(titleTexture); titleTexture = nullptr; }
    if (artistTexture) { SDL_DestroyTexture(artistTexture); artistTexture = nullptr; }
    for (struct TTF_Font* fb : titleFontFallbacks) { TTF_CloseFont(fb); }
    for (struct TTF_Font* fb : artistFontFallbacks) { TTF_CloseFont(fb); }
    titleFontFallbacks.clear();
    artistFontFallbacks.clear();
    if (titleFont) { TTF_CloseFont(titleFont); titleFont = nullptr; }
    if (artistFont) { TTF_CloseFont(artistFont); artistFont = nullptr; }
}

void Window::rebuildText() {
    if (titleTexture) { SDL_DestroyTexture(titleTexture); titleTexture = nullptr; }
    if (artistTexture) { SDL_DestroyTexture(artistTexture); artistTexture = nullptr; }

    int maxWidth = (int)albumArea.w - 2 * textPad;
    if (maxWidth < 16) { maxWidth = 16; }

    if (!lastTitle.empty()) {
        titleTexture = renderText(lastTitle, titleFont, textColor, maxWidth);
    }
    if (!lastArtists.empty()) {
        artistTexture = renderText(lastArtists, artistFont, subTextColor, maxWidth);
    }
}

SDL_Texture* Window::renderText(const std::string& text, struct TTF_Font* font, SDL_Color color, int maxWidth) {
    if (!font || text.empty()) { return nullptr; }

    std::string display = text;
    int w = 0, h = 0;
    if (TTF_GetStringSize(font, display.c_str(), display.size(), &w, &h) && w > maxWidth) {
        // Trim so that text + an ellipsis fits within maxWidth.
        int ellW = 0, ellH = 0;
        std::string ellipsis = "...";
        TTF_GetStringSize(font, ellipsis.c_str(), ellipsis.size(), &ellW, &ellH);

        int target = maxWidth - ellW;
        if (target < 1) { target = 1; }

        size_t measuredLength = 0;
        int measuredWidth = 0;
        if (TTF_MeasureString(font, text.c_str(), text.size(), target, &measuredWidth, &measuredLength)) {
            display = text.substr(0, measuredLength) + ellipsis;
        } else {
            display = ellipsis;
        }
    }

    SDL_Surface* surface = TTF_RenderText_Blended(font, display.c_str(), display.size(), color);
    if (!surface) { return nullptr; }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (tex) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    }
    return tex;
}

void Window::setAlbumBorder(int thickness, SDL_Color color) {
    albumBorderThickness = thickness;
    albumBorderColor = color;
}

void Window::setInnerShadow(bool enabled, SDL_Color color, int inset, int softness, float opacity) {
    innerShadowEnabled = enabled;
    innerShadowColor = color;
    innerShadowInset = inset;
    innerShadowSoftness = softness;
    innerShadowOpacity = opacity;
}

void Window::loadAlbumImage(const std::string& url) {
    if (albumTexture) {
        SDL_DestroyTexture(albumTexture);
        albumTexture = nullptr;
    }
    lastAlbumURL = url;
    if (url.empty()) { return; }

    auto endpoint = splitAlbumUrl(url);
    if (endpoint.first.empty()) {
        std::cout << "Invalid album image URL: " << url << std::endl;
        return;
    }

    std::string bytes = Http::get(endpoint.first, endpoint.second);
    if (bytes.empty()) {
        std::cout << "Failed to download album image: " << url << std::endl;
        return;
    }

    SDL_IOStream* io = SDL_IOFromMem(bytes.data(), bytes.size());
    if (!io) {
        std::cout << "SDL_IOFromMem failed: " << SDL_GetError() << std::endl;
        return;
    }

    SDL_Surface* art = IMG_Load_IO(io, true);
    if (!art) {
        std::cout << "IMG_Load_IO failed: " << SDL_GetError() << std::endl;
        return;
    }

    albumTexture = makeAlbumTexture(art);
    SDL_DestroySurface(art);
    if (!albumTexture) {
        std::cout << "Failed to build album art texture" << std::endl;
    }
}

std::string Window::iconPath(const char* filename) const {
    std::vector<std::string> candidates;

    // Executable-relative (works regardless of launch cwd). Exe typically
    // lives in build/Release or build/Debug, so assets/ may be up two levels.
    const char* base = SDL_GetBasePath();
    if (base) {
        candidates.push_back(std::string(base) + "assets/" + filename);
        candidates.push_back(std::string(base) + "../../assets/" + filename);
        candidates.push_back(std::string(base) + "../../../assets/" + filename);
    }

    // Working-directory-relative fallback.
    candidates.push_back(std::string("assets/") + filename);

    for (const std::string& c : candidates) {
        SDL_IOStream* io = SDL_IOFromFile(c.c_str(), "rb");
        if (io) {
            SDL_CloseIO(io);
            return c;
        }
    }
    return candidates.front();
}

SDL_Texture* Window::loadIconTexture(const char* filename) {
    std::string path = iconPath(filename);
    SDL_Surface* surf = IMG_Load(path.c_str());
    if (!surf) {
        std::cout << "Failed to load icon " << path << ": " << SDL_GetError() << std::endl;
        return nullptr;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    if (tex) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    }
    return tex;
}

void Window::loadIcons() {
    previousIcon = loadIconTexture("previous.svg");
    playIcon = loadIconTexture("play.svg");
    pauseIcon = loadIconTexture("pause.svg");
    nextIcon = loadIconTexture("next.svg");
}

void Window::destroyIcons() {
    if (previousIcon) { SDL_DestroyTexture(previousIcon); previousIcon = nullptr; }
    if (playIcon) { SDL_DestroyTexture(playIcon); playIcon = nullptr; }
    if (pauseIcon) { SDL_DestroyTexture(pauseIcon); pauseIcon = nullptr; }
    if (nextIcon) { SDL_DestroyTexture(nextIcon); nextIcon = nullptr; }
}

SDL_Texture* Window::iconForAction(WheelAction action) const {
    switch (action) {
        case WheelAction::Previous:  return previousIcon;
        case WheelAction::PlayPause: return isPlaying ? pauseIcon : playIcon;
        case WheelAction::Next:      return nextIcon;
        default:                     return nullptr;
    }
}

void Window::drawActionIcon(WheelAction action, float cx, float cy) {
    SDL_Texture* icon = iconForAction(action);
    if (!icon) { return; }

    float w = 0.0f, h = 0.0f;
    if (!SDL_GetTextureSize(icon, &w, &h)) { return; }
    if (w <= 0.0f || h <= 0.0f) { return; }

    // Scale to fit within iconSize px while preserving aspect ratio.
    float scale = (float)iconSize / (w > h ? w : h);
    float dw = w * scale;
    float dh = h * scale;

    SDL_FRect dst = { cx - dw / 2.0f, cy - dh / 2.0f, dw, dh };
    SDL_RenderTexture(renderer, icon, nullptr, &dst);
}

SDL_Texture* Window::makeAlbumTexture(SDL_Surface* art) {
    int W = (int)albumArea.w;
    int H = (int)albumArea.h;
    if (W <= 0 || H <= 0 || !art || art->w <= 0 || art->h <= 0) { return nullptr; }

    // Cover-crop: scale so the image fills the whole panel, cropping the overflow.
    double scale = ((double)W / art->w) > ((double)H / art->h) ? ((double)W / art->w) : ((double)H / art->h);
    int cw = (int)(W / scale + 0.5);
    int ch = (int)(H / scale + 0.5);
    cw = cw > art->w ? art->w : cw;
    ch = ch > art->h ? art->h : ch;
    int sx = (art->w - cw) / 2;
    int sy = (art->h - ch) / 2;

    SDL_Surface* dst = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_ARGB8888);
    if (!dst) { return nullptr; }

    SDL_Rect srcRect = { sx, sy, cw, ch };
    SDL_Rect dstRect = { 0, 0, W, H };
    SDL_BlitSurfaceScaled(art, &srcRect, dst, &dstRect, SDL_SCALEMODE_LINEAR);

    // Mask to the rounded panel. The art is fully enclosed by the border ring,
    // so its edge should be crisp (not anti-aliased) - only clear pixels that
    // fall completely outside the capsule.
    double R = H / 2.0;
    for (int y = 0; y < H; ++y) {
        double dy = (y + 0.5) - R;
        double rem = R * R - dy * dy;
        double halfW = rem < 0.0 ? 0.0 : std::sqrt(rem);

        double left = R - halfW;
        double right = (W - R) + halfW;
        int lc = (int)std::ceil(left - 0.5);
        int rc = (int)std::floor(right - 0.5);

        if (lc > rc) { lc = (int)std::ceil(left); rc = (int)std::floor(right); }

        for (int x = 0; x < W; ++x) {
            if (x >= lc && x <= rc) { continue; }

            Uint8 r, g, b, a;
            if (!SDL_ReadSurfacePixel(dst, x, y, &r, &g, &b, &a)) { continue; }
            SDL_WriteSurfacePixel(dst, x, y, r, g, b, 0);
        }
    }

    applyInnerShadow(dst);

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, dst);
    SDL_DestroySurface(dst);
    if (tex) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    }
    return tex;
}

void Window::applyInnerShadow(SDL_Surface* dst) {
    if (!dst || !innerShadowEnabled) { return; }
    if (innerShadowInset <= 0 || innerShadowSoftness <= 0 || innerShadowOpacity <= 0.0f) { return; }

    int W = dst->w;
    int H = dst->h;
    double R = H / 2.0;
    double bX = W / 2.0;
    double bY = H / 2.0;
    double soft = (std::max)(1.0, (double)innerShadowSoftness);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            Uint8 r, g, b, a;
            if (!SDL_ReadSurfacePixel(dst, x, y, &r, &g, &b, &a)) { continue; }
            if (a == 0) { continue; }

            // Signed distance to the rounded box boundary (negative inside).
            double px = std::fabs((x + 0.5) - bX);
            double py = std::fabs((y + 0.5) - bY);
            double qx = px - (bX - R);
            double qy = py - (bY - R);
            double ox = qx < 0.0 ? 0.0 : qx;
            double oy = qy < 0.0 ? 0.0 : qy;
            double sdf = std::sqrt(ox * ox + oy * oy)
                       + (std::min)((std::max)(qx, qy), 0.0) - R;

            // dd grows from 0 at the boundary toward the inside.
            double dd = -sdf;
            if (dd <= 0.0) { continue; }

            // Shadow: full strength exactly on the edge, falling off quickly
            // inward and smoothly reaching 0 at `inset`. The exponential term
            // drops to ~5% by `softness` px; the (1 - t) window guarantees an
            // exact, non-abrupt zero at `inset`.
            double t = dd / (double)innerShadowInset;
            if (t >= 1.0) { continue; }
            double factor = std::exp(-dd * (3.0 / soft)) * (1.0 - t);
            double alpha = innerShadowOpacity * factor;

            Uint8 nr = (Uint8)(r * (1.0 - alpha) + innerShadowColor.r * alpha);
            Uint8 ng = (Uint8)(g * (1.0 - alpha) + innerShadowColor.g * alpha);
            Uint8 nb = (Uint8)(b * (1.0 - alpha) + innerShadowColor.b * alpha);
            SDL_WriteSurfacePixel(dst, x, y, nr, ng, nb, a);
        }
    }
}

void Window::drawScanline(float xl, float xr, int yy) {
    Uint8 cr, cg, cb, ca;
    SDL_GetRenderDrawColor(renderer, &cr, &cg, &cb, &ca);

    if (xl > xr) {
        return;
    }

    int col0 = (int)std::floor(xl);
    int col1 = (int)std::floor(xr);

    if (col1 == col0) {
        float cov = xr - xl;
        if (cov > 0.0f) {
            SDL_SetRenderDrawColor(renderer, cr, cg, cb, (Uint8)(ca * cov));
            SDL_RenderLine(renderer, col0, yy, col1, yy);
        }
        SDL_SetRenderDrawColor(renderer, cr, cg, cb, ca);
        return;
    }

    float covL = (col0 + 1) - xl;
    int solidStart = col0;
    if (covL < 1.0f) {
        SDL_SetRenderDrawColor(renderer, cr, cg, cb, (Uint8)(ca * covL));
        SDL_RenderLine(renderer, col0, yy, col0, yy);
        solidStart = col0 + 1;
    }

    int solidEnd = col1 - 1;
    if (solidEnd >= solidStart) {
        SDL_SetRenderDrawColor(renderer, cr, cg, cb, ca);
        SDL_RenderLine(renderer, solidStart, yy, solidEnd, yy);
    }

    float covR = xr - col1;
    if (covR > 0.0f) {
        SDL_SetRenderDrawColor(renderer, cr, cg, cb, (Uint8)(ca * covR));
        SDL_RenderLine(renderer, col1, yy, col1, yy);
    }

    SDL_SetRenderDrawColor(renderer, cr, cg, cb, ca);
}

void Window::drawCircle(float radius, float x, float y) {
    int top = (int)std::floor(y - radius);
    int bottom = (int)std::ceil(y + radius);
    float r2 = radius * radius;

    for (int yy = top; yy <= bottom; yy++) {
        float relY = (float)yy - y;
        float rem = r2 - relY * relY;
        if (rem <= 0.0f) {
            continue;
        }

        float hw = std::sqrt(rem);
        drawScanline(x - hw, x + hw, yy);
    }
}

void Window::drawRoundedRect(SDL_Renderer* renderer, float x, float y, float w, float h) {
    float radius = h / 2.0f;
    float cxL = x + radius;
    float cxR = x + w - radius;
    float cy = y + radius;

    int top = (int)std::floor(y);
    int bottom = (int)std::ceil(y + h);
    float r2 = radius * radius;

    for (int yy = top; yy <= bottom; yy++) {
        float dy = (float)yy - cy;
        float rem = r2 - dy * dy;
        // A pixel row yy covers band [yy, yy+1). The shape spans
        // [cy-radius, cy+radius], so the bottom boundary row (dy == radius)
        // starts exactly at the edge and is not covered.
        if (rem < 0.0f || dy >= radius) {
            continue;
        }

        float hw = std::sqrt(rem);
        drawScanline(cxL - hw, cxR + hw, yy);
    }
}

void Window::drawRoundedRectSolid(SDL_Renderer* renderer, float x, float y, float w, float h) {
    // Crisp (non anti-aliased) capsule fill, used for content that is fully
    // enclosed by a border, so its soft edge doesn't overlap the border's edge.
    float radius = h / 2.0f;
    float cxL = x + radius;
    float cxR = x + w - radius;
    float cy = y + radius;

    int top = (int)std::floor(y);
    int bottom = (int)std::floor(y + h) - 1;

    for (int yy = top; yy <= bottom; yy++) {
        float dy = ((float)yy + 0.5f) - cy;
        float rem = radius * radius - dy * dy;
        if (rem < 0.0f) { continue; }

        float hw = std::sqrt(rem);
        int l = (int)std::floor(cxL - hw);
        int rr = (int)std::ceil(cxR + hw);
        if (l <= rr) {
            SDL_RenderLine(renderer, l, yy, rr, yy);
        }
    }
}

void Window::drawBorderedCircle(float border, float radius, float x, float y, SDL_Color outerColor, SDL_Color innerColor) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 230);
    drawCircle(radius, x + 3, y + 3);

    SDL_SetRenderDrawColor(renderer, outerColor.r, outerColor.g, outerColor.b, outerColor.a);
    drawCircle(radius, x, y);

    SDL_SetRenderDrawColor(renderer, innerColor.r, innerColor.g, innerColor.b, innerColor.a);
    drawCircle(radius - border, x, y);
}

void Window::render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 130);
    SDL_RenderClear(renderer);

    float t = (float)albumBorderThickness;

    albumArea.x = (width / 2) - (albumAreaWidth / 2);

    if (t > 0.0f) {
        // Outer rounded panel in the border color; the content drawn inside
        // reveals a border ring of `t` pixels.
        SDL_SetRenderDrawColor(renderer, albumBorderColor.r, albumBorderColor.g, albumBorderColor.b, albumBorderColor.a);
        drawRoundedRect(renderer, albumArea.x - t, albumArea.y - t, albumArea.w + 2.0f * t, albumArea.h + 2.0f * t);
    }

    if (albumTexture) {
        SDL_RenderTexture(renderer, albumTexture, nullptr, &albumArea);
    }
    else {
        // With a border, the interior sits flush against the ring so it must be
        // crisp; only anti-alias it when there's no border and it meets the
        // background directly.
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        if (t > 0.0f) {
            drawRoundedRectSolid(renderer, albumArea.x, albumArea.y, albumArea.w, albumArea.h);
        }
        else {
            drawRoundedRect(renderer, albumArea.x, albumArea.y, albumArea.w, albumArea.h);
        }
    }

    // Overlay the track name and artist, centered on the pill.
    if (titleTexture || artistTexture) {
        float tw = 0.0f, th = 0.0f;
        float aw = 0.0f, ah = 0.0f;
        if (titleTexture) { SDL_GetTextureSize(titleTexture, &tw, &th); }
        if (artistTexture) { SDL_GetTextureSize(artistTexture, &aw, &ah); }

        float gap = 3.0f;
        float hasArtist = artistTexture ? 1.0f : 0.0f;
        float blockH = th + hasArtist * (gap + ah);
        float blockY = albumArea.y + (albumArea.h - blockH) / 2.0f;

        // Translucent backdrop dimming the whole pill/art area for legibility.
        if (textBackdrop.a > 0) {
            SDL_SetRenderDrawColor(renderer, textBackdrop.r, textBackdrop.g, textBackdrop.b, textBackdrop.a);
            drawRoundedRectSolid(renderer, albumArea.x, albumArea.y, albumArea.w, albumArea.h);
        }

        if (titleTexture) {
            SDL_FRect dst = { albumArea.x + (albumArea.w - tw) / 2.0f, blockY, tw, th };
            SDL_RenderTexture(renderer, titleTexture, nullptr, &dst);
        }
        if (artistTexture) {
            SDL_FRect dst = { albumArea.x + (albumArea.w - aw) / 2.0f, blockY + th + gap, aw, ah };
            SDL_RenderTexture(renderer, artistTexture, nullptr, &dst);
        }
    }

    int gap = width / std::size(actions);

    for (int i = 0; i < std::size(actions); i++) {
        int x = ((i+1) * gap) - (gap / 2);
        int y = height / 2;

        if (selectedAction == actions[i]) {
            drawBorderedCircle(1, buttonRadius, x, y, borderColor, selectedColor);
        }
        else {
            drawBorderedCircle(1, buttonRadius, x, y, borderColor, defaultColor);
        }

        drawActionIcon(actions[i], (float)x, (float)y);
    }

    SDL_RenderPresent(renderer);
}
