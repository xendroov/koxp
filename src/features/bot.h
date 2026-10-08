#pragma once

namespace KO::Features {

struct BotConfig {
    bool enabled      = false;
    float attackRange = 1500.f;  // game unit
};

class Bot {
public:
    static Bot& Get() { static Bot inst; return inst; }

    void Start();
    void Stop();
    bool IsRunning() const { return running_; }

    void Tick();  // internal — bot thread çağırır

    BotConfig cfg;

private:
    bool     running_ = false;
    int32_t  currentTargetID_ = -1;

    void SelectTarget();
};

} // namespace KO::Features
