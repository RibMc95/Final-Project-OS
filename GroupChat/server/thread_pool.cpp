#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <vector>
class ThreadPool
{

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
public:
    ThreadPool(size_t threads) : stop(false)
    {

        for (size_t i = 0; i < threads; ++i)
        {
            workers.emplace_back([this]
                {
                    while(true)
                    {
                        std::function<void()> task;

                        {
                            std::unique_lock<std::mutex> lock(queue_mutex);
                            condition.wait(lock, [this] { return stop || !tasks.empty(); });
                            if (stop && tasks.empty())
                                return;
                                task = std::move(tasks.front());
                                tasks.pop();
                        }

                    task();
                    } 
                }
            );
    }
    }

    ~ThreadPool()
    {

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }

        condition.notify_all();
        for (auto &worker : workers)
        {

            worker.join();
        }
    }

    void enqueue(std::function<void()> task)
    {

        {

            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.push(std::move(task));
        }

        condition.notify_one();
    }
};

void handle_client(int client_socket)
{

    std::cout << "Handling client " << client_socket << std::endl;

    // Add chat logic here
}

int main()
{

    ThreadPool pool(4); // 4-thread pool

    for (int i = 0; i < 10; ++i)
    {

        pool.enqueue([i]
                     { handle_client(i); });
    }

    return 0;
}
