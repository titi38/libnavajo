// bench_libnavajo.cc
#include "libnavajo/libnavajo.hh"
#include <thread>
#include <iostream>

class PlainTextPage : public DynamicPage {
public:
    bool getPage(HttpRequest* request, HttpResponse* response) override {
        response->setMimeType("text/plain");
        return fromString("Hello, World!", response);
    }
};

int main() {
    LogRecorder::getInstance()->setDebugMode(false);

    WebServer server;
    std::cout << "std::thread::hardware_concurrency(): " << std::thread::hardware_concurrency() << std::endl;
    server.setThreadsPoolSize(std::thread::hardware_concurrency()*4);
    server.setServerPort(8080);

    DynamicRepository repo;
    PlainTextPage plaintext;

    repo.add("/plaintext", &plaintext);
    server.addRepository(&repo);

    server.startService();
    server.wait();

    LogRecorder::freeInstance();
    return 0;
}
