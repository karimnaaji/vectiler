#include <sstream>
#include <string>
#include <curl/curl.h>
#include <iostream>

size_t writeData(void* ptr, size_t size, size_t nmemb, void *stream) {
    ((std::stringstream*) stream)->write(reinterpret_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

bool downloadData(std::string& out, const std::string& url) {
    std::stringstream stream;

    static bool curlInitialized = false;
    static CURL* curlHandle = nullptr;

    if (!curlInitialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curlHandle = curl_easy_init();
        curlInitialized = true;

        // set up curl to perform fetch
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writeData);
        curl_easy_setopt(curlHandle, CURLOPT_HEADER, 0L);
        curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 0L);
        curl_easy_setopt(curlHandle, CURLOPT_ACCEPT_ENCODING, "gzip");
        curl_easy_setopt(curlHandle, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curlHandle, CURLOPT_DNS_CACHE_TIMEOUT, -1);
    }

    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &stream);
    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());

    printf("URL request: %s", url.c_str());

    CURLcode result = curl_easy_perform(curlHandle);

    if (result != CURLE_OK) {
        printf(" -- Failure: %s\n", curl_easy_strerror(result));
    } else {
        printf(" -- OK\n");
    }

    out = stream.str();
    return result == CURLE_OK && stream.rdbuf()->in_avail();
}
