#include "web_server.hpp"

int main() {
    app().addListener("127.0.0.1", 12345).run();
}