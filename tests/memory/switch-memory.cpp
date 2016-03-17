#include <sodium/sodium.h>
#include <unistd.h>
#include <string>

using namespace sodium;
using namespace std;

/*!
 * Run:
 *     valgrind --tool=massif memory/switch-memory
 *     massif-visualizer massif.out.*
 *
 * What you should see:
 *
 *     flat memory usage profile (no leak)
 */
int main(int argc, char* argv[])
{
    #define N 100
    cell<string>* as[N];
    cell_sink<string>* bs[N];
    cell_sink<cell<string>>* ss[N];
    cell<string>* os[N];
    std::function<void()> unlistens[N];
    {
        transaction t;
        for (int i = 0; i < N; i++) {
            as[i] = new cell<string>("hello");
            bs[i] = new cell_sink<string>("world");
            ss[i] = new cell_sink<cell<string>>(*as[i]);
            os[i] = new cell<string>(switch_c(*ss[i]));
            os[i]->updates().listen([] (const string& s) {
                //printf("%s\n", s.c_str());
            });
        }
    }
    for (int iter = 0; iter < 100; iter++) {
        for (int i = 0; i < N; i++)
            ss[i]->send(*as[i]);
        for (int i = 0; i < N; i++)
            ss[i]->send(*bs[i]);
    }
    return 0;
}
