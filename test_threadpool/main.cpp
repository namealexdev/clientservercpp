#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// Пул потоков на базе std::thread
class ThreadPool
{
public:
    ThreadPool( size_t n )
        : mIsDone( false )
    {
        // Создаем n рабочих потоков
        for( size_t i = 0; i < n; ++i )
        {
            mWorkers.emplace_back( [ this ] { Worker(); } );
        }
    }

    ~ThreadPool()
    {
        // Сигнализируем потокам завершиться и пробуждаем их
        mIsDone = true;
        mCv.notify_all();
        // Явно ждем завершения всех std::thread
        for( auto& t : mWorkers )
        {
            if( t.joinable() )
            {
                t.join();
            }
        }

        // Уничтожаем оставшиеся корутины в очереди, чтобы избежать утечек
        std::lock_guard< std::mutex > lock( mMutex );
        while( !mTasks.empty() )
        {
            mTasks.front().destroy();
            mTasks.pop();
        }
    }

    // Добавить корутину в очередь
    void Schedule( std::coroutine_handle<> h )
    {
        {
            std::lock_guard< std::mutex > lock( mMutex );
            mTasks.push( h );
        }
        mCv.notify_one();
    }

private:
    void Worker()
    {
        while( !mIsDone )
        {
            std::coroutine_handle<> h;
            {
                std::unique_lock< std::mutex > lock( mMutex );
                mCv.wait( lock, [ & ] { return mIsDone || !mTasks.empty(); } );
                if( mIsDone && mTasks.empty() )
                    return; // выходим, когда завершили все задачи
                h = mTasks.front();
                mTasks.pop();
            }
            h.resume(); // возобновляем корутину
        }
    }

    std::vector< std::thread > mWorkers;          // рабочие потоки
    std::queue< std::coroutine_handle<> > mTasks; // очередь корутин
    std::mutex mMutex;                            // защитный мьютекс
    std::condition_variable mCv;                  // условная переменная
    std::atomic< bool > mIsDone;                  // флаг завершения
};

// Awaiter для планирования на пул
struct ScheduleOnPool
{
    ThreadPool& mPool;

    bool await_ready() const noexcept
    {
        return false;
    }
    void await_suspend( std::coroutine_handle<> h ) noexcept
    {
        mPool.Schedule( h ); // ставим в очередь
    }
    void await_resume() noexcept
    {
    }
};

// Простейший Task для корутины
struct Task
{
    struct promise_type
    {
        Task get_return_object()
        {
            return Task{ std::coroutine_handle< promise_type >::from_promise( *this ) };
        }
        std::suspend_never initial_suspend() noexcept
        {
            return {};
        }
        std::suspend_always final_suspend() noexcept
        {
            return {};
        } // кадр корутины остается до destroy()
        void return_void() noexcept
        {
        }
        void unhandled_exception() noexcept
        {
            // Обрабатываем исключение, выводим описание
            try
            {
                throw;
            }
            catch( const std::exception& e )
            {
                std::cerr << "Coroutine error: " << e.what() << std::endl;
            }
            catch( ... )
            {
                std::cerr << "Coroutine unknown exception" << std::endl;
            }
        }
    };
    std::coroutine_handle< promise_type > mCoro;
};

// Пример корутины: демонстрация переключения потока
Task Example( ThreadPool& pool )
{
    std::cout << "До переключения (" << std::this_thread::get_id() << ")\n";
    co_await ScheduleOnPool{ pool };
    std::cout << "После переключения (" << std::this_thread::get_id() << ")\n";
}

int main()
{
    ThreadPool pool{ 2 };  // пул из 2 потоков
    Example( pool );       // запускаем корутину
    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
    // даем время на выполнение
    return 0;
}
