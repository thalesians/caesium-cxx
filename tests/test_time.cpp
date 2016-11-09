#include <sodium/sodium.h>
#include <sodium/time.h>
#include <queue>
#include <iostream>
#include <assert.h>

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
    sodium::cell_sink<boost::optional<int>> period(boost::optional<int>(500));
    sodium::stream<int> timer = sodium::periodic_timer(ts, period);
    std::vector<std::string> out;
    auto kill1 = timer.listen([&out] (int t) {
        char buf[128];
        sprintf(buf, "tick %d", t);
        out.push_back(buf);
    });
    sodium::stream_sink<sodium::unit> sAskCurrentTime;
    sodium::stream<int> sCurrentTime = sAskCurrentTime.snapshot(ts.time);
    auto kill2 = sCurrentTime.listen([&out] (int t) {
        char buf[128];
        sprintf(buf, "ask %d", t);
        out.push_back(buf);
    });
    for (int t = 0; t <= 10000; t += 666) {
        if (t >= 4000)
            period.send(boost::optional<int>());
        if (t >= 5000)
            period.send(boost::optional<int>(2000));
        impl->set_time(t);
        sAskCurrentTime.send(sodium::unit());
    }
    kill1();
    kill2();
    for (auto it = out.begin(); it != out.end(); ++it)
        std::cout << *it << std::endl;
    assert(out == std::vector<std::string>({
        "ask 0",
        "tick 500",
        "ask 666",
        "tick 1000",
        "ask 1332",
        "tick 1500",
        "ask 1998",
        "tick 2000",
        "tick 2500",
        "ask 2664",
        "tick 3000",
        "ask 3330",
        "tick 3500",
        "ask 3996",
        "ask 4662",
        "ask 5328",
        "tick 5500",
        "ask 5994",
        "ask 6660",
        "ask 7326",
        "tick 7500",
        "ask 7992",
        "ask 8658",
        "ask 9324",
        "tick 9500",
        "ask 9990"
    }));
    std::cout << "PASS" << std::endl;
    return 0;
}
