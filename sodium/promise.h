#ifndef _SODIUM_PROMISE_H_
#define _SODIUM_PROMISE_H_

#include <sodium/sodium.h>
#include <memory>

namespace sodium {
    template <class A>
    class promise {
    template <class B>
    friend class promise;
    private:
        promise(
            const stream<A>& sDeliver_,
            const cell<boost::optional<A>>& oValue_)
        : sDeliver(sDeliver_),
          oValue(oValue_)
        {
        }
    public:
        promise()
        : oValue(boost::optional<A>())
        {
        }
        promise(const stream<A>& sDeliver_)
        : sDeliver(sDeliver_.once()),
          oValue(this->sDeliver.map([] (const A& a) {
                  return boost::optional<A>(a);
              }).hold(boost::optional<A>()))
        {
        }
        stream<A> sDeliver;
        cell<boost::optional<A>> oValue;

        void then_do(const std::function<void(const A&)>& f) const {
            transaction trans;
            filter_optional(oValue.value()).listen_once(f);
        }

        template <typename Fn>
        promise<typename std::result_of<Fn(A)>::type> map(Fn f) const {
            typedef typename std::result_of<Fn(A)>::type B;
            return promise<B>(
                sDeliver.map(f),
                oValue.map(
                    [f] (const boost::optional<A>& ob) {
                        return ob ? boost::optional<B>(f(ob.get()))
                                  : boost::optional<B>();
                    }
                ));
        }
    };

    template <class A, class B, class C>
    promise<C> lift(const std::function<C(const A&, const B&)>& f,
        const promise<A>& pa, const promise<B>& pb)
    {
        transaction trans;
        return promise<C>(
            lift<boost::optional<A>, boost::optional<B>, boost::optional<C>>(
                [f] (const boost::optional<A>& oa, const boost::optional<B>& ob) {
                    return oa && ob ? boost::optional<C>(f(oa.get(), ob.get()))
                                    : boost::optional<C>();
                },
                pa.oValue, pb.oValue
            ));
    }
}

#endif
