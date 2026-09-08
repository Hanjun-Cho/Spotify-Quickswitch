#pragma once

#include "SpotifyAuth.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

struct SpotifyTrack {
        std::string id; 

        std::string trackName;
        std::string artists;

        std::string albumName;
        std::string albumImageURL;

        long durationMS;
        long progressMS;

        bool isPlaying;
};

class Spotify {
    public:
        Spotify(SpotifyAuth* auth);

        SpotifyTrack getCurrentlyPlayingTrack();
        SpotifyTrack getCurrentTrack();
        SpotifyTrack playPause();

        void play();
        void pause();
        void previous();
        void next();

        void startPolling();
        void stopPolling();

    private:
        SpotifyAuth* auth;

        SpotifyTrack currentTrack;
        std::mutex trackMutex;
        std::atomic<bool> polling{false};
        std::thread pollThread;

        void pollLoop();
};
