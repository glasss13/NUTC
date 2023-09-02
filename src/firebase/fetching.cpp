#include "fetching.hpp"

namespace nutc {
namespace client {

void
print_algo_info(const glz::json_t& algo, const std::string& algo_id)
{
    log_i(firebase, "Running {}", algo["name"].get<std::string>());
    log_i(firebase, "Description: {}", algo["description"].get<std::string>());
    log_i(firebase, "Upload date: {}", algo["uploadDate"].get<std::string>());
    log_d(firebase, "Downloading at url {}", algo["downloadURL"].get<std::string>());
    log_i(firebase, "Algo id: {}", algo_id);
}

glz::json_t
get_user_info(const std::string& uid)
{
    auto url = fmt::format("{}/users/{}.json", std::string(FIREBASE_URL), uid);
    return firebase_request("GET", url);
}

static size_t
write_callback(void* contents, size_t size, size_t nmemb, void* userp)
{
    auto* str = reinterpret_cast<std::string*>(userp);
    auto* data = static_cast<char*>(contents);

    str->append(data, size * nmemb);
    return size * nmemb;
}

std::string
storage_request(const std::string& firebase_url)
{
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, firebase_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res)
                      << std::endl;
        }

        curl_easy_cleanup(curl);
    }

    return readBuffer;
}

std::optional<std::string>
get_algo(const std::string& uid, const std::string& algo_id)
{
    glz::json_t user_info = get_user_info(uid);
    // if not has "algos"
    if (!user_info.contains("algos")) {
        log_w(
            firebase, "User {} has no algos. Will not participate in simulation.", uid
        );
        return std::nullopt;
    }
    if (!user_info["algos"].contains(algo_id)) {
        log_w(firebase, "User {} does not have algo id {}.", uid, algo_id);
        return std::nullopt;
    }
    glz::json_t algo_info = user_info["algos"][algo_id];
    std::string downloadURL = algo_info["downloadURL"].get<std::string>();
    print_algo_info(algo_info, algo_id);
    std::string algo_file = storage_request(downloadURL);
    return algo_file;
}

glz::json_t
firebase_request(
    const std::string& method, const std::string& url, const std::string& data
)
{
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        }
        else if (method == "PUT") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        }
        else if (method == "DELETE") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            log_e(firebase, "curl_easy_perform() failed: {}", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }

    glz::json_t json{};
    auto error = glz::read_json(json, readBuffer);
    if (error) {
        std::string descriptive_error = glz::format_error(error, readBuffer);
        log_e(firebase, "glz::read_json() failed: {}", descriptive_error);
    }
    return json;
}
} // namespace client
} // namespace nutc
