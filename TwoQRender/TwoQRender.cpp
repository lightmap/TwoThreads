#include <iostream>
#include <stdint.h>
#include <thread>
#include <iostream>
#include <queue>
#include <string>
#include <functional>

#include <ppl.h>
#include <concurrent_queue.h>
#include <concurrent_vector.h>
#include <sstream>

#include <chrono>
#include <ctime>

#include <semaphore>

using std::thread;
using std::queue;
using std::string;
using std::cout;
using std::endl;
using std::ref;

using namespace concurrency;

//Ќеобходимо разработать двухпоточную очередь рендера
// Ќеобходима возможность дождатьс€ выполнени€ всех текущих команд из главного потока (Flush операци€).
// Ќеобходима возможность дождатьс€ выполнени€ команд из главного потока и сбросить очередь(Reset операци€).
// ќчередь команд должна быть написана с использованием семафоров и атомиков (использование мутекса нецелесообразно из соображений производительности).
// ¬ажно (!): система должна быть отказоустойчива€ к любым видам ошибок и исключени€м, никаких вылетов
// ¬ качестве тестовых команд можно сделать вывод сообщени€ в консоль или бипов(Beep(50, 50)) winapi.

concurrent_queue<string> itemQueue;// очередь рендер комманд
std::atomic<bool> bRenderCollect = false;//
std::atomic<bool> bRenderRunning = true;
std::counting_semaphore<1> signalFlushDone(0);
std::counting_semaphore<1> signalCollectRC(0);

void RenderFlush() {
    signalFlushDone.acquire();
}

void RenderReset() {
    itemQueue.clear();
    bRenderCollect.store(false, std::memory_order_release);
    signalCollectRC.release();
}

//MainThread - вставл€ет команды (с параметрами) в очередь команд рендера
void MainWork(concurrent_queue<string> &itemQueue) {
    cout << "START Main\n";
    int frames = 10;
    while(frames--) {
        signalCollectRC.release();
        bRenderCollect.store(true, std::memory_order_release);
        for(int i = 0;i < 10;i++) {
            string item = "command " + std::to_string(i);
            cout << "push:" << item.c_str() << endl;
            itemQueue.push(item);
        }
        bRenderCollect.store(false, std::memory_order_release);
        RenderFlush();
        int random_delay = rand() / 1000;
        std::this_thread::sleep_for(std::chrono::milliseconds(random_delay));//0-1s
        cout << "Main Frame\n";
    }
    RenderReset();
    bRenderRunning.store(false, std::memory_order_release);// cast end of render loop
    cout << "END Main\n";
}

//GraphicsThread - √рафический поток в свою очередь считывает эти команды и обрабатывает
void RenderWork(concurrent_queue<string> &itemQueue) {
    string item;
    cout << "START Render\n";
    while(bRenderRunning.load(std::memory_order_acquire)) {

        int rendered = 0;
        signalCollectRC.acquire();
        while(bRenderCollect.load(std::memory_order_acquire)) {
            while(!itemQueue.empty()) {
                if(itemQueue.try_pop(item)) {
                    cout << "render:" << item.c_str() << endl;
                    int random_delay = 1 + rand() / 10;//1-10ms
                    //std::this_thread::sleep_for(std::chrono::milliseconds(random_delay));// simulate load
                    rendered++;
                }
            }
        }
        cout << "rendered:" << rendered << " commands\n";
        signalFlushDone.release();
        //std::this_thread::sleep_for(std::chrono::milliseconds(random_delay));
    }
    cout << "END Render\n";
}

int main()
{
    cout << "START\n";
    srand(std::time(nullptr));

    thread MainThread(MainWork, ref(itemQueue));
    thread GraphicsThread(RenderWork, ref(itemQueue));

    MainThread.join();
    GraphicsThread.join();

    cout << "END\n";
}