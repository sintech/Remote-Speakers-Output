// Added 2026-11-16. SPDX-License-Identifier: GPL-3.0-or-later
#include "Debugger.h"
#include "impl/OutputObserver.h"
#include "RAOPEngine.h"
#include "NTPTimestamp.h"
#include <Poco/Net/Net.h>
#include <Poco/Exception.h>
#include <cstdio>
#include <exception>
#include <stdexcept>

class Observer : public OutputObserver {
public:
    void onBytesOutput(size_t) override {}
};

int main()
{
    Poco::Net::initializeNetwork();
    Debugger::setPrintCallback([](const char* line) { std::puts(line); });
    int result = 0;
    try {
        Observer observer;
        RAOPEngine engine(observer);
        OutputInterval interval(0, 0);
        for (unsigned int delay : { 100U, 750U, 2000U }) {
            engine.reinit(interval, delay); // RSA OAEP, AES key, ALAC encoder, UDP/reactor.
            if (engine.latency(RAOPEngine::outputFormat()) <= 0)
                throw std::runtime_error("Invalid session latency");
        }
        for (unsigned int delay : { 99U, 2001U }) {
            bool rejected = false;
            try { engine.reinit(interval, delay); }
            catch (const std::invalid_argument&) { rejected = true; }
            if (!rejected) throw std::runtime_error("Out-of-range sync delay accepted");
        }
        Poco::Timestamp original(1700000000123456LL);
        NTPTimestamp ntp(original);
        Poco::Timestamp restored(ntp);
        const auto difference = original.epochMicroseconds() - restored.epochMicroseconds();
        if (difference < -1 || difference > 1)
            throw std::runtime_error("NTP timestamp round trip failed");
        std::puts("RAOP crypto, ALAC initialization, UDP sockets and NTP conversion passed.");
    } catch (const Poco::Exception& error) {
        std::fprintf(stderr, "Core session startup failed: %s (code %d)\n",
            error.displayText().c_str(), error.code());
        result = 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Core session startup failed: %s\n", error.what());
        result = 1;
    }
    Poco::Net::uninitializeNetwork();
    return result;
}
