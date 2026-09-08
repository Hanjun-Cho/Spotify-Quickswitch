#include "Spotify.h"
#include "Http.h"

#include <string>
#include <iostream>
#include <chrono>

using Http::JsonNode;
using Http::parseJson;

namespace {
    std::string joinArtistNames(const JsonNode& artists) {
        std::string names;
        for (const JsonNode& artist : artists.arrayValue) {
            const JsonNode* name = artist.get("name");
            if (!name || name->type != JsonNode::Type::String) { continue; }

            if (!names.empty()) { names += ", "; }
            names += name->stringValue;
        }
        return names;
    }
}

Spotify::Spotify(SpotifyAuth* auth) : auth(auth) {}

SpotifyTrack Spotify::getCurrentlyPlayingTrack() {
    SpotifyTrack track;

    std::string json = Http::get(L"api.spotify.com", L"/v1/me/player/currently-playing", auth->getAccessToken());
    if (json.empty()) { return track; }


    JsonNode root = parseJson(json);
    if (root.type != JsonNode::Type::Object) { return track; }

    const JsonNode* item = root.get("item");
    if (!item || item->type != JsonNode::Type::Object) { return track; }

    const JsonNode* id = item->get("id");
    if (id && id->type == JsonNode::Type::String) { track.id = id->stringValue; }

    const JsonNode* trackName = item->get("name");
    if (trackName && trackName->type == JsonNode::Type::String) { track.trackName = trackName->stringValue; }

    const JsonNode* artists = item->get("artists");
    if (artists && artists->type == JsonNode::Type::Array) { track.artists = joinArtistNames(*artists); }

    const JsonNode* album = item->get("album");
    if (album && album->type == JsonNode::Type::Object) {
        const JsonNode* albumName = album->get("name");
        if (albumName && albumName->type == JsonNode::Type::String) { track.albumName = albumName->stringValue; }

        const JsonNode* images = album->get("images");
        if (images && images->type == JsonNode::Type::Array && !images->arrayValue.empty()) {
            const JsonNode* image = images->get(0);
            const JsonNode* url = image ? image->get("url") : nullptr;
            if (url && url->type == JsonNode::Type::String) { track.albumImageURL = url->stringValue; }
        }
    }

    const JsonNode* durationMS = item->get("duration_ms");
    if (durationMS && durationMS->type == JsonNode::Type::Number) { track.durationMS = durationMS->longValue; }

    const JsonNode* progressMS = root.get("progress_ms");
    if (progressMS && progressMS->type == JsonNode::Type::Number) { track.progressMS = progressMS->longValue; }

    const JsonNode* isPlaying = root.get("is_playing");
    if (isPlaying && isPlaying->type == JsonNode::Type::Bool) { track.isPlaying = isPlaying->boolValue; }

    {
        std::lock_guard<std::mutex> lock(trackMutex);
        currentTrack = track;
    }
    return track;
}

SpotifyTrack Spotify::playPause() {
    bool playing;
    {
        std::lock_guard<std::mutex> lock(trackMutex);
        playing = currentTrack.isPlaying;
    }

    if (!playing) {
        play();
    }
    else {
        pause();
    }

    return getCurrentlyPlayingTrack();
}

void Spotify::startPolling() {
    if (polling.load()) { return; }

    polling.store(true);
    pollThread = std::thread(&Spotify::pollLoop, this);
}

void Spotify::stopPolling() {
    polling.store(false);
    if (pollThread.joinable()) { pollThread.join(); }
}

void Spotify::pollLoop() {
    while (polling.load()) {
        getCurrentlyPlayingTrack();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Spotify::play() {
    Http::put(L"api.spotify.com", L"/v1/me/player/play", auth->getAccessToken());
}

void Spotify::pause() {
    Http::put(L"api.spotify.com", L"/v1/me/player/pause", auth->getAccessToken());
}

void Spotify::previous() {
    Http::post(L"api.spotify.com", L"/v1/me/player/previous", auth->getAccessToken());
}

void Spotify::next() {
    Http::post(L"api.spotify.com", L"/v1/me/player/next", auth->getAccessToken());
}
