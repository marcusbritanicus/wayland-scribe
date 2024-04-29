#include <iostream>
#include <hello-world-client.hpp>

class MyHelloWord: public Wayland::Client::Greeter {
    protected:
        void hello(const std::string &greeting) {
            std::cout << "The server says: " << greeting << std::endl;
        };
}
