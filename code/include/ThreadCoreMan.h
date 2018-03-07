#pragma once
#include <vector>
#include <mutex>
class ThreadCoreMan {

    std::vector<bool> activeThreads;
    std::mutex lock;
    int threadsDispatched;
    int threadsDefuncted;

public:
    ThreadCoreMan(int numThreads) {
        activeThreads.resize(numThreads);
        threadsDispatched = 1;
        threadsDefuncted = 0;
    }
    void setThreadStatus(int i, bool status) {
        activeThreads[i] = status;
    }
    int getUnusedCores() {
        return threadsDispatched - threadsDefuncted;
    }

    void Lock() {
        lock.lock();
    }
    void UnLock() {
        lock.unlock();
    }

    void defunctThread() {
        threadsDefuncted++;
    }
    void dispatchThread() {
        threadsDispatched++;
    }
};