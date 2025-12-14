#ifndef RFT_TIMERC_H
#define RFT_TIMERC_H
#include <chrono>


class timerC {
private:
    bool running;
    std::chrono::system_clock::time_point startTime;
    std::chrono::duration<float> duration;

public:
    timerC() : running(false), startTime(), duration(0) {};
    explicit timerC(int milliseconds) : running(false), startTime(), duration(milliseconds) {};
    void setDuration(int milliseconds);
    void start();
    void stop();
    [[nodiscard]] bool timeout() const;
};


#endif //RFT_TIMERC_H
