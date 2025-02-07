#include <boost/filesystem.hpp>
#include <iostream>

namespace fs = boost::filesystem;

int
main() {
    for (const auto& entry : fs::directory_iterator(fs::current_path())) {
        std::cout << entry.path() << std::endl;
    }
    return 0;
}
