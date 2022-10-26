/********************************************************************************************************
 kropla -- a program to play Kropki; file cnn_workers.cc -- reading NN in
multiple processes. Copyright (C) 2022 Bartek Dyda, email: bartekdyda (at)
protonmail (dot) com

    Some parts are inspired by Pachi http://pachi.or.cz/
      by Petr Baudis and Jean-loup Gailly

    This file is part of Kropla.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************************************************/

#include "cnn_workers.h"

#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "caffe/mcaffe.h"

namespace
{
void* create_shared_memory(size_t size)
{
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_SHARED | MAP_ANONYMOUS;
    return mmap(NULL, size, protection, visibility, -1, 0);
}

template <typename T>
std::size_t sizeOfVec(const std::vector<T>& v)
{
    return v.size() * sizeof(T);
}

std::mutex caffe_mutex;
bool is_parent{true};
bool madeQuiet = false;

}  // namespace

void* add(void* ptr, std::size_t arg)
{
    return static_cast<void*>(static_cast<char*>(ptr) + arg);
}

class SharedMemWithSemaphores
{
    size_t size;
    const int n_sem{2};
    void* mem{nullptr};

   public:
    SharedMemWithSemaphores() = delete;
    SharedMemWithSemaphores(const SharedMemWithSemaphores&) = delete;
    SharedMemWithSemaphores& operator=(const SharedMemWithSemaphores&) = delete;
    SharedMemWithSemaphores& operator=(SharedMemWithSemaphores&&) = delete;

    SharedMemWithSemaphores(SharedMemWithSemaphores&& other) noexcept
        : size{other.size}, n_sem{other.n_sem}, mem{other.mem}
    {
        other.mem = nullptr;
    }

    SharedMemWithSemaphores(size_t size) : size{size}
    {
        mem = create_shared_memory(size + sizeof(sem_t) * n_sem +
                                   sizeof(uint32_t));
        const int shared = 1;
        const unsigned value = 0;
        for (int n = 0; n < n_sem; ++n)
        {
            if (sem_init(getSemaphore(n), shared, value) < 0)
                throw std::runtime_error("Error creating semaphore");
        }
    }

    void sendJob(uint32_t datav, const void* data, size_t s, void* incoming,
                 size_t si)
    {
        getControlWord() = 0;
        memcpy(getData(), &datav, sizeof(datav));
        memcpy(add(getData(), sizeof(datav)), data, s);
        sem_post(getSemaphore(0));
        sem_wait(getSemaphore(1));
        memcpy(incoming, getData(), si);
    }

    sem_t* getSemaphore(int n) { return static_cast<sem_t*>(mem) + n; }
    void* getData()
    {
        return static_cast<void*>(static_cast<char*>(mem) +
                                  sizeof(sem_t) * n_sem + sizeof(uint32_t));
    }
    uint32_t& getControlWord()
    {
        return *static_cast<uint32_t*>(static_cast<void*>(
            static_cast<char*>(mem) + sizeof(sem_t) * n_sem));
    }

    ~SharedMemWithSemaphores()
    {
        if (is_parent and mem)
        {
            std::cerr << "Killing... " << mem << std::endl;
            getControlWord() = 1;
            sem_post(getSemaphore(0));
            sem_wait(getSemaphore(1));
            sem_destroy(getSemaphore(1));
            sem_destroy(getSemaphore(0));
            munmap(mem, size + sizeof(sem_t) * n_sem + sizeof(uint32_t));
        }
    }
};

namespace workers
{
class WorkersPool : public WorkersPoolBase
{
   public:
    WorkersPool(const std::string& config_file, int wlkx, bool use_this_thread,
                std::size_t memory_needed);
    WorkersPool() = default;
    WorkersPool(WorkersPool&&) = delete;
    WorkersPool(const WorkersPool&) = delete;
    WorkersPool operator=(WorkersPool&&) = delete;
    WorkersPool operator=(const WorkersPool&) = delete;
    ~WorkersPool() override = default;

    void doWork(uint32_t datav, const void* data, size_t s, void* incoming,
                size_t si);
    int getCount() const { return count; }
    int getPlanes() const override { return planes; }
    std::pair<bool, std::vector<float>> getCnnInfo(std::vector<float>& input,
                                                   uint32_t wlkx) override;

   private:
    void child_worker(void* data);
    void worker(int number, SharedMemWithSemaphores& sh);
    bool setupWorkers(int n, std::size_t memory_needed);
    int findWorker();
    void releaseWorker(int which);

    std::mutex jobs_mutex;
    std::condition_variable cv;

    std::vector<int> is_free;
    int how_many_free;
    int count{0};
    std::vector<pid_t> pids;
    std::vector<SharedMemWithSemaphores> mems;

    MCaffe cnn;
    bool use_this_thread{false};
    int planes = 10;
    std::string config_file;
    std::string model_file_name{};
    std::string weights_file_name{};
    constexpr static int DEFAULT_CNN_BOARD_SIZE = 20;
};

bool WorkersPool::setupWorkers(int n, std::size_t memory_needed)
{
    count = n;
    pids.reserve(count);
    mems.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        mems.emplace_back(memory_needed);
        pid_t id = fork();
        if (id == -1)
        {
            mems.pop_back();
            continue;
        }
        else if (id == 0)
        {
            is_parent = false;
            worker(i, mems.back());
            return false;
        }
        else
        {
            pids.push_back(id);
        }
    }
    count = mems.size();
    if (count == 0) throw std::runtime_error("no workers");
    std::cerr << "setup " << count << " workers" << std::endl;
    is_free.resize(count);
    std::fill(is_free.begin(), is_free.end(), 1);
    how_many_free = count;
    return true;
}

int WorkersPool::findWorker()
{
    int taken = -1;
    if (how_many_free > 0)
    {
        const auto it = std::find(is_free.begin(), is_free.end(), 1);
        taken = std::distance(is_free.begin(), it);
        *it = 0;
        --how_many_free;
    }
    return taken;
}

void WorkersPool::releaseWorker(int which)
{
    {
        std::lock_guard<std::mutex> l(jobs_mutex);
        is_free.at(which) = 1;
        ++how_many_free;
    }
    cv.notify_one();
}

void WorkersPool::doWork(uint32_t datav, const void* data, size_t s,
                         void* incoming, size_t si)
{
    std::unique_lock<std::mutex> lock(jobs_mutex);
    if (how_many_free == 0) cv.wait(lock, [&]() { return how_many_free; });
    std::cerr << "Free: " << how_many_free << "  ";
    for (auto e : is_free) std::cerr << e;
    std::cerr << std::endl;
    int taken = findWorker();

    std::cerr << "Free after take #" << taken << ": " << how_many_free << "  ";
    for (auto e : is_free) std::cerr << e;
    std::cerr << std::endl;

    lock.unlock();
    if (taken == -1) throw std::runtime_error("do Work");
    mems.at(taken).sendJob(datav, data, s, incoming, si);
    releaseWorker(taken);
}

void WorkersPool::worker(int number, SharedMemWithSemaphores& sh)
{
    std::cerr << "Hello from child #" << number << std::endl;
    for (;;)
    {
        sem_wait(sh.getSemaphore(0));
        if (sh.getControlWord())
        {
            sem_post(sh.getSemaphore(1));
            break;
        }
        child_worker(sh.getData());
        sem_post(sh.getSemaphore(1));
    }
    std::cerr << "Bye from child #" << number << std::endl;
    exit(0);
}

void WorkersPool::child_worker(void* data)
try
{
    const uint32_t wlkx = *static_cast<uint32_t*>(data);
    cnn.caffe_init(wlkx, model_file_name, weights_file_name,
                   DEFAULT_CNN_BOARD_SIZE);
    float* datafl = static_cast<float*>(add(data, sizeof(uint32_t)));
    auto res = cnn.caffe_get_data(datafl, wlkx, planes, wlkx);
    static_cast<uint32_t*>(data)[0] = true;
    memcpy(add(data, sizeof(uint32_t)), static_cast<void*>(res.data()),
           sizeOfVec(res));
}
catch (const CaffeException& exc)
{
    std::cerr << "Failed to load cnn" << std::endl;
    static_cast<uint32_t*>(data)[0] = false;
}

WorkersPool::WorkersPool(const std::string& config_file, int wlkx,
                         bool use_this_thread, std::size_t memory_needed)
    : use_this_thread{use_this_thread}, config_file{config_file}
{
    if (not madeQuiet)
    {
        cnn.quiet_caffe("kropla");
        madeQuiet = true;
    }
    constexpr int default_n_workers = 7;
    int n_workers = default_n_workers;
    {
        std::string number_of_planes{};
        std::string n_workers_str{};
        std::ifstream t(config_file);
        if (std::getline(t, number_of_planes))
            if (std::getline(t, model_file_name))
                if (std::getline(t, weights_file_name))
                    std::getline(t, n_workers_str);
        planes = std::stoi(number_of_planes);
        if (planes != 7 and planes != 10 and planes != 20)
        {
            std::cerr << "Unsupported number of planes (" << planes
                      << "), assuming 10." << std::endl;
            planes = 10;
        }
        if (use_this_thread)
            cnn.caffe_init(wlkx, model_file_name, weights_file_name,
                           DEFAULT_CNN_BOARD_SIZE);
        try
        {
            int n_workers = std::stoi(n_workers_str);
            if (n_workers >= 1 and n_workers <= 1024)
                n_workers = default_n_workers;
        }
        catch (const std::invalid_argument&)
        {
            n_workers = default_n_workers;
        }
    }

    if (n_workers > int(use_this_thread))
    {
        try
        {
            setupWorkers(n_workers - int(use_this_thread), memory_needed);
        }
        catch (const std::runtime_error& e)
        {
            std::cerr << "Error setting up " << n_workers
                      << " workers: " << e.what() << std::endl;
        }
    }
}

std::pair<bool, std::vector<float>> WorkersPool::getCnnInfo(
    std::vector<float>& input, uint32_t wlkx)
try
{
    if (input.empty())
    {
        std::cerr << "No input for cnn" << std::endl;
        return {false, {}};
    }

    std::unique_lock<std::mutex> lock{caffe_mutex, std::defer_lock};
    bool acquired_lock = false;
    if (use_this_thread)
    {
        if (getCount() == 0)  // only this thread available, block
        {
            lock.lock();
            acquired_lock = true;
        }
        else
        {
            acquired_lock = lock.try_lock();
        }
    }
    if (acquired_lock)
    {
        auto debug_time = std::chrono::high_resolution_clock::now();
        auto res = cnn.caffe_get_data(input.data(), wlkx, planes, wlkx);
        lock.unlock();
        std::cerr << "Forward time [micros]: "
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::high_resolution_clock::now() - debug_time)
                         .count()
                  << std::endl;
        return {true, res};
    }
    // use worker
    std::vector<float> res(wlkx * wlkx, 0.0f);
    doWork(wlkx, static_cast<void*>(input.data()), sizeOfVec(input),
           static_cast<void*>(res.data()), sizeOfVec(res));
    return {true, res};
}
catch (const CaffeException& exc)
{
    std::cerr << "Failed to load cnn" << std::endl;
    return {false, {}};
}

std::unique_ptr<WorkersPoolBase> buildWorkerPool(const std::string& config_file,
                                                 std::size_t memory_needed,
                                                 uint32_t wlkx,
                                                 bool use_this_thread)
{
    return std::make_unique<WorkersPool>(config_file, wlkx, use_this_thread,
                                         memory_needed);
}

}  // namespace workers
