#include <sodium/sodium.h>
#include <sodium/time.h>
#include <queue>
#include <stdio.h>

int next_seq = 0;

struct test_impl : sodium::timer_system_impl<int>
{
    test_impl() : now_(0) {}

    struct entry {
        entry(int t_, std::function<void()> callback_)
        : t(t_), callback(callback_), seq(++next_seq) {}
        int t;
        std::function<void()> callback;
        int seq;
        bool operator < (const entry& other) const {
            if (t > other.t) return true;
            if (t < other.t) return false;
            return seq > other.seq;
        }
        bool operator == (const entry& other) const {
            return seq == other.seq;
        }
    };
    sodium::impl::priority_queue<entry> entries;
    int now_;

    /*!
     * Set a timer that will execute the specified callback at the specified time.
     * @return A function that can be used to cancel the timer.
     */
    virtual std::function<void()> set_timer(int t, std::function<void()> callback)
    {
        entry e(t, callback);
        entries.push(entry(t, callback));
        return [this, e] () {
            entries.remove(e);
        };
    }

    /**
     * Return the current clock time.
     */
    virtual int now() {
        return now_;
    }

    void set_time(int t) {
        while (true) {
            if (entries.empty())
                break;
            auto e = entries.top();
            if (e.t < t) {
                entries.pop();
                e.callback();
            }
            else
                break;
        }
        now_ = t;
    }
};

int main(int argc, char* argv[])
{
    std::shared_ptr<test_impl> impl(new test_impl);
    sodium::timer_system<int> ts(impl);
    sodium::stream<int> timer = sodium::periodic_timer(ts, sodium::cell<int>(1000));
    auto kill1 = timer.listen([] (int t) {
        printf("tick %d\n", t);
    });
    sodium::stream_sink<sodium::unit> sAskCurrentTime;
    sodium::stream<int> sCurrentTime = sAskCurrentTime.snapshot(ts.time);
    auto kill2 = sCurrentTime.listen([] (int t) {
        printf("ask %d\n", t);
    });
    for (int t = 0; t <= 10000; t += 666) {
        impl->set_time(t);
        sAskCurrentTime.send(sodium::unit());
    }
    kill1();
    kill2();
    return 0;
}
