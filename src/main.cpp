#include <iostream>
#include <curl/curl.h>
#include <string>
#include <sstream>

#include "rapidjson/document.h"
#include "geojson.h"
#include "tileData.h"

static size_t write_data(void *_ptr, size_t _size, size_t _nmemb, void *_stream) {
    ((std::stringstream*) _stream)->write(reinterpret_cast<char *>(_ptr), _size * _nmemb);
    return _size * _nmemb;
}

std::unique_ptr<TileData> downloadTile(const std::string& _url, const Tile& _tile) {
    bool success = true;

    CURL* curlHandle = curl_easy_init();

    std::stringstream out;

    // set up curl to perform fetch
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curlHandle, CURLOPT_URL, _url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_HEADER, 0L);
    curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curlHandle, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curlHandle, CURLOPT_NOSIGNAL, 1L);
    
    std::cout << "Fetching URL with curl: " << _url << std::endl;

    CURLcode result = curl_easy_perform(curlHandle);
    
    if (result != CURLE_OK) {
        std::cout << "curl_easy_perform failed: " << curl_easy_strerror(result) << std::endl;
        success = false;
    } else {
        std::cout << "Fetched tile: " << _url << std::endl;

        // parse written data into a JSON object
        rapidjson::Document doc;
        doc.Parse(out.str().c_str());

        if (doc.HasParseError()) {
            std::cout << "Error parsing" << std::endl;
            return nullptr;
        }

        std::unique_ptr<TileData> data = std::make_unique<TileData>();
        for (auto layer = doc.MemberBegin(); layer != doc.MemberEnd(); ++layer) {
            std::cout << "Extracting layer " << layer->name.GetString() << std::endl;
            data->layers.emplace_back(std::string(layer->name.GetString()));
            GeoJson::extractLayer(layer->value, data->layers.back(), _tile);
        }

        return std::move(data);
    }

    return nullptr;
}

int main() {
    int tileX = 19294;
    int tileY = 24642;
    int tileZ = 16;

    std::string apiKey = "vector-tiles-qVaBcRA";
    std::string url = "http://vector.mapzen.com/osm/all/" 
        + std::to_string(tileZ) + "/" 
        + std::to_string(tileX) + "/" 
        + std::to_string(tileY) + ".json?api_key=" + apiKey;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    auto data = downloadTile(url, { tileX, tileY, tileZ });

}