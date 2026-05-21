#include "qrreader_win_common.hpp"

#include <atomic>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    qrreader::setConsoleLogging(true);
    std::string port = (argc >= 2) ? argv[1] : qrreader::getConfigPort();
    std::atomic<bool> stop{false};
    qrreader::runReaderLoop(stop, port);
    return 0;
}
