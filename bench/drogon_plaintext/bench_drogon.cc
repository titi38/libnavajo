#include <drogon/drogon.h>
#include <thread>
#include <functional>

int main() {
    drogon::app().registerHandler(
        "/plaintext",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
            resp->setBody("Hello, World!");
            callback(resp);
        },
        {drogon::Get}
    );

    drogon::app().addListener("0.0.0.0", 8082);
    drogon::app().setThreadNum(std::thread::hardware_concurrency());
    drogon::app().run();
    return 0;
}
