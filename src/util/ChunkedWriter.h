#ifndef SOFAR_CHUNKED_WRITER_H
#define SOFAR_CHUNKED_WRITER_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ArduinoJson custom writer that batches output into N-byte chunks.
//
// Serialising straight into PubSubClient forwards every byte as its own
// WiFiClient::write(). Into a stalled socket (half-open link, zero TCP
// window behind a proxy) each of those calls blocks for the full client
// timeout while yielding — a 1.3 kB state message becomes ~1.8 hours with
// the hardware watchdog fed throughout. Here the first short write latches
// failure and every later byte is dropped, so one stall costs one timeout.
template <class Out, size_t N>
class ChunkedWriter {
public:
    explicit ChunkedWriter(Out& out) : _out(out) {}

    size_t write(uint8_t c) {
        if (_failed) return 0;
        _buf[_n++] = c;
        if (_n == N) drain();
        return 1;
    }

    size_t write(const uint8_t* s, size_t len) {
        size_t done = 0;
        while (done < len && !_failed) {
            size_t take = N - _n;
            if (take > len - done) take = len - done;
            memcpy(_buf + _n, s + done, take);
            _n += take;
            done += take;
            if (_n == N) drain();
        }
        return _failed ? 0 : done;
    }

    // Flushes the tail; true only if every byte reached the sink.
    bool finish() {
        drain();
        return !_failed;
    }

private:
    void drain() {
        if (_n == 0 || _failed) return;
        if (_out.write(_buf, _n) != _n) _failed = true;
        _n = 0;
    }

    Out&    _out;
    uint8_t _buf[N];
    size_t  _n      = 0;
    bool    _failed = false;
};

#endif // SOFAR_CHUNKED_WRITER_H
