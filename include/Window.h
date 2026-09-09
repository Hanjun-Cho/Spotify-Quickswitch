#pragma once

#include "Wheel.h"
#include <windows.h>
#include <iterator>
#include <string>
#include <vector>
#include <SDL3/SDL.h>

struct TTF_Font;

class Window {
    public:
        Window();
        ~Window();

        void show();
        void hide(Wheel wheel);
        void setMonitor(MONITORINFO* monitor);

        void updateSelectedAction(POINT cursor);
        void update(POINT cursor);
        void updateAlbumArt(const SpotifyTrack& track);
        SDL_Texture* albumArt() const { return albumTexture; }
        void setAlbumBorder(int thickness, SDL_Color color);
        void setInnerShadow(bool enabled, SDL_Color color, int inset, int softness, float opacity);

        void drawScanline(float xl, float xr, int y);
        void drawCircle(float radius, float x, float y);
        void drawBorderedCircle(float border, float radius, float x, float y, SDL_Color outerColor, SDL_Color innerColor);
        void drawRoundedRect(SDL_Renderer* renderer, float x, float y, float w, float h);
        void drawRoundedRectSolid(SDL_Renderer* renderer, float x, float y, float w, float h);
        void render();

        void loadIcons();
        void destroyIcons();
        void drawActionIcon(WheelAction action, float cx, float cy);
        SDL_Texture* iconForAction(WheelAction action) const;

    private:
        int width;
        int height;

        int buttonRadius = 36;
        int buttonOffset = 60;

        WheelAction actions[3] = {
            WheelAction::Previous,
            WheelAction::PlayPause,
            WheelAction::Next
        };

        WheelAction selectedAction = WheelAction::Previous;
        int buttonCount = std::size(actions);

        int buttonWidthTotal = (buttonCount * buttonRadius * 2) + ((buttonCount - 1) * buttonOffset);

        SDL_Color defaultColor = {69, 85, 114, 255};
        SDL_Color selectedColor = {161, 189, 240, 255};
        SDL_Color defaultBorderColor = {48, 60, 82, 255};
        SDL_Color borderColor = defaultColor;

        SDL_Window* window;
        SDL_Renderer* renderer;

        float albumAreaWidth = 300;
        SDL_FRect albumArea = {0, 15, albumAreaWidth, 75};
        int albumBorderThickness = 2;
        SDL_Color albumBorderColor = {0, 0, 0, 255};

        bool innerShadowEnabled = true;
        SDL_Color innerShadowColor = {0, 0, 0, 255};
        int innerShadowInset = 8;
        int innerShadowSoftness = 8;
        float innerShadowOpacity = 1.0f;

        std::vector<TTF_Font*> titleFontFallbacks;
        std::vector<TTF_Font*> artistFontFallbacks;

        std::string lastAlbumURL;
        SDL_Texture* albumTexture = nullptr;

        struct TTF_Font* titleFont = nullptr;
        struct TTF_Font* artistFont = nullptr;

        std::string lastTitle;
        std::string lastArtists;
        SDL_Texture* titleTexture = nullptr;
        SDL_Texture* artistTexture = nullptr;

        bool isPlaying = false;

        SDL_Texture* previousIcon = nullptr;
        SDL_Texture* playIcon = nullptr;
        SDL_Texture* pauseIcon = nullptr;
        SDL_Texture* nextIcon = nullptr;

        // Baked button drop shadow, generated once from the current button geometry.
        SDL_Texture* buttonShadowTexture = nullptr;
        int buttonShadowSide = 0;

        int iconSize = 30;
        // Icons are drawn a bit smaller than iconSize so they sit comfortably
        // inside the button face.
        float iconSizeScale = 0.88f;

        SDL_Color textColor = {255, 255, 255, 255};
        SDL_Color subTextColor = {215, 222, 230, 255};
        SDL_Color textBackdrop = {0, 0, 0, 80};
        int titleSize = 18;
        int artistSize = 12;
        int textPad = 18;

        void loadAlbumImage(const std::string& url);
        SDL_Texture* makeAlbumTexture(SDL_Surface* art);
        void applyInnerShadow(SDL_Surface* dst);
        void buildButtonShadows();
        void destroyButtonShadows();
        void initFonts();
        void initFallbackFonts();
        void destroyText();
        void rebuildText();
        SDL_Texture* renderText(const std::string& text, struct TTF_Font* font, SDL_Color color, int maxWidth);
        SDL_Texture* loadIconTexture(const char* filename);
        std::string iconPath(const char* filename) const;
};
