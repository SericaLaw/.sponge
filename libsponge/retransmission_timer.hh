//
// Created by serica on 9/15/21.
//

#ifndef SPONGE_LIBSPONGE_RETRANSMISSION_TIMER_HH
#define SPONGE_LIBSPONGE_RETRANSMISSION_TIMER_HH

#include <cstddef>
#include <iostream>

class RetransmissionTimer {
  private:
    size_t  _timeout{0};
    size_t  _time_elapsed{0};
    bool _running{false};
  public:
    void start(size_t timeout) {
        reset(timeout);
        _running = true;
    }
    bool timeout() const {
        return _running and _time_elapsed >= _timeout;
    }
    bool running() const { return _running; }
    void tick(size_t ms_since_last_tick) {
        if (_running)
            _time_elapsed += ms_since_last_tick;
    }
    void reset(size_t timeout) {
        _timeout = timeout;
        _time_elapsed = 0;
        _running = false;
    }
    void stop() {
        _running = false;
    }

};

#endif  // SPONGE_LIBSPONGE_RETRANSMISSION_TIMER_HH
