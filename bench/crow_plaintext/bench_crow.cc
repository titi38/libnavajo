// bench_crow.cc
#include "crow.h"

int main() {
crow::logger::setLogLevel(crow::LogLevel::Critical);
    crow::SimpleApp app;

    CROW_ROUTE(app, "/plaintext")([] {
        crow::response res;
        res.code = 200;
        res.set_header("Content-Type", "text/plain");
        res.body = "Hello, World!";
        return res;
    });



    app.port(8081)
       .concurrency(std::thread::hardware_concurrency())
       .run();

    return 0;
}
